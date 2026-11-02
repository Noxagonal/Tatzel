import assert from "node:assert/strict";
import { after, test } from "node:test";
import { mkdtemp, readFile, readdir, rm, writeFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import { resolve, join } from "node:path";
import { pathToFileURL } from "node:url";
import { stripTypeScriptTypes } from "node:module";

const generated = await mkdtemp(join(tmpdir(), "tatzel-browser-"));
const source = resolve("targets/typescript_build/src");
for (const filename of await readdir(source)) {
	if (!filename.endsWith(".ts")) continue;
	const javascript = stripTypeScriptTypes(await readFile(join(source, filename), "utf8"))
		.replaceAll(".js\"", ".mjs\"");
	await writeFile(join(generated, filename.replace(".ts", ".mjs")), javascript);
}
after(() => rm(generated, { recursive: true, force: true }));

const nodes = new Map();
const handlers = new Map();

/** @brief Minimal DOM node used to observe actual runtime operations. */
class TestElement {
	/**
	 * @brief Creates a DOM test node.
	 * @param id Browser part ID.
	 * @pre None.
	 * @post The node has empty attributes, children, text, and event markers.
	 */
	constructor(id = "") {
		this.id = id;
		this.dataset = {};
		this.attributes = {};
		this.children = [];
		this.textContent = "";
		this.className = "";
	}
	/** @brief Appends a test node. @param child Concrete child node. @pre None. @post The child is registered by its part ID. */
	appendChild(child) { this.children.push(child); child.parentElement = this; nodes.set(child.id, child); }
	/** @brief Records an attribute. @param name Attribute name. @param value Attribute value. @pre None. @post The attribute is stored. */
	setAttribute(name, value) {
		this.attributes[name] = value;
		if (name.startsWith("data-")) this.dataset[name.slice(5).replace(/-([a-z])/g, (_, c) => c.toUpperCase())] = value;
	}
	/** @brief Checks stored attribute presence. @param name Attribute name. @returns Whether the attribute exists. @pre None. @post State is unchanged. */
	hasAttribute(name) { return Object.hasOwn(this.attributes, name); }
	/** @brief Reads an attribute. @param name Attribute name. @returns Value or null. @pre None. @post State is unchanged. */
	getAttribute(name) { return this.attributes[name] ?? null; }
	/** @brief Removes a stored attribute. @param name Attribute name. @pre None. @post The attribute is absent. */
	removeAttribute(name) { delete this.attributes[name]; }
	/** @brief Checks native disabled state, including a disabled ancestor fieldset. @param selector Supported pseudo-class. @returns Whether disabled. @pre None. @post State is unchanged. */
	matches(selector) { return selector === ":disabled" && (Boolean(this.disabled) || Boolean(this.parentElement?.matches(selector))); }
	/** @brief Replaces native option nodes. @param children Replacement nodes. @pre None. @post Children are replaced. */
	replaceChildren(...children) { this.children = children; }
	/** @brief Checks DOM ancestry. @param element Candidate descendant. @returns Whether this node contains it. @pre None. @post State is unchanged. */
	contains(element) {
		for (let node = element; node; node = node.parentElement) if (node === this) return true;
		return false;
	}
	/** @brief Finds open dialog descendants. @param selector Dialog selector. @returns Matching descendant nodes. @pre None. @post State is unchanged. */
	querySelectorAll(selector) {
		return this.children.flatMap(child => [
			...(child instanceof TestDialog && child.open ? [child] : []), ...child.querySelectorAll(selector)
		]);
	}
	/** @brief Reports connection to the test document. @returns Whether registered. @pre None. @post State is unchanged. */
	get isConnected() { return nodes.get(this.id) === this; }
	/** @brief Focuses this surviving node. @pre The node is registered. @post It is the document's active element. */
	focus() { document.activeElement?.onBlur?.(); document.activeElement = this; }
	/** @brief Releases native focus. @pre None. @post The document body receives focus. */
	blur() { document.activeElement = document.body; }
	/** @brief Records a one-shot blur handler. @param type Event type. @param callback Handler. @pre Only blur is supported. @post The handler runs when focus leaves. */
	addEventListener(type, callback) { if (type === "blur") this.onBlur = () => { delete this.onBlur; callback(); }; }
	/** @brief Removes this subtree. @pre None. @post Descendants are disconnected and lost focus returns to body. */
	remove() {
		if (this.contains(document.activeElement)) document.activeElement = document.body;
		for (const child of [...this.children]) child.remove();
		nodes.delete(this.id);
		if (this.parentElement) this.parentElement.children = this.parentElement.children.filter(child => child !== this);
		this.parentElement = null;
	}
	/** @brief Finds this marked event source. @param selector Data-attribute selector. @returns This node if marked, otherwise null. @pre None. @post State is unchanged. */
	closest(selector) {
		const name = selector.slice(1, -1);
		const marker = name.slice(5).replace(/-([a-z])/g, (_, c) => c.toUpperCase());
		return this.hasAttribute(name) || this.dataset[marker] ? this : this.parentElement?.closest(selector) ?? null;
	}
}

/** @brief Minimal form input for checkbox and text event checks. */
class TestInput extends TestElement {}

/** @brief Native-form double collecting its current controls. */
class TestForm extends TestElement {
	/** @brief Creates an empty form. @param id Browser part ID. @pre None. @post No controls are registered. */
	constructor(id) { super(id); this.elements = []; }
	/** @brief Dispatches the native submission event. @pre Handlers are registered. @post The form submit handler has run. */
	requestSubmit() { handlers.get("submit")({ target: this, preventDefault() {} }); }
}

/** @brief Native-dialog double recording modal transitions. */
class TestDialog extends TestElement {
	/** @brief Opens the dialog. @pre The dialog is closed. @post Its open flag is set. */
	showModal() { this.open = true; this.opens = (this.opens ?? 0) + 1; }
	/** @brief Closes the dialog. @pre The dialog is open. @post Its open flag is cleared. */
	close() { this.open = false; this.closes = (this.closes ?? 0) + 1; if (this.opener) document.activeElement = this.opener; }
}

/** @brief Input-event double providing the browser composition flag. */
class TestInputEvent {
	/** @brief Creates an edit event. @param target Editor source. @param isComposing Whether IME composition is active. @pre None. @post The event exposes its source and composition flag. */
	constructor(target, isComposing) { this.target = target; this.isComposing = isComposing; }
}

/** @brief WebSocket double that captures actual browser messages. */
class TestSocket extends EventTarget {
	static OPEN = 1;
	/**
	 * @brief Creates an open test transport.
	 * @param url Requested WebSocket URL.
	 * @pre None.
	 * @post Messages are captured locally without network access.
	 */
	constructor(url) { super(); this.url = url; this.readyState = 1; this.messages = []; }
	/** @brief Captures a serialized event. @param text JSON message. @pre None. @post The decoded message is retained. */
	send(text) { this.messages.push(JSON.parse(text)); }
}

globalThis.Element = TestElement;
globalThis.HTMLElement = TestElement;
globalThis.HTMLInputElement = TestInput;
globalThis.HTMLTextAreaElement = class extends TestElement {};
globalThis.HTMLSelectElement = class extends TestElement {};
globalThis.HTMLFormElement = TestForm;
globalThis.HTMLFieldSetElement = class extends TestElement {};
globalThis.HTMLDialogElement = TestDialog;
globalThis.HTMLButtonElement = class extends TestElement {};
globalThis.InputEvent = TestInputEvent;
globalThis.WebSocket = TestSocket;
globalThis.window = { location: { protocol: "https:", host: "example.test" } };
globalThis.document = {
	body: new TestElement("body"),
	getElementById: id => nodes.get(id) ?? null,
	createElement: () => new TestElement(),
	addEventListener: (event, callback) => handlers.set(event, callback)
};
nodes.set(document.body.id, document.body);
document.activeElement = document.body;

const runtime = await import(pathToFileURL(join(generated, "websocket.mjs")));
const domRuntime = await import(pathToFileURL(join(generated, "dom-ops.mjs")));
const revisions = await import(pathToFileURL(join(generated, "field-revisions.mjs")));
const fieldUpdate = { epoch: 0, base_epoch: 0, client_revision: 0, replace: false };
const dom = { ...domRuntime };
for (const method of ["setValue", "setSelectOptions", "setRadioOptions"]) {
	dom[method] = parts => domRuntime[method](parts.map(part => ({ ...fieldUpdate, ...part })));
}
const parseOperation = runtime.parseServerOperation;
const withMetadata = event => {
	const message = JSON.parse(event.data);
	if (["set_value", "set_select_options", "set_radio_options"].includes(message.op) && Array.isArray(message.data))
		message.data = message.data.map(part => ({ ...fieldUpdate, ...part }));
	return parseOperation({ data: JSON.stringify(message) });
};
const events = await import(pathToFileURL(join(generated, "events.mjs")));
events.registerBrowserEventHandlers();

test("status and error messages use the same op/data envelope", () => {
	assert.equal(runtime.socket.url, "wss://example.test/ws");
	for (const message of [
		{ op: "connected", data: { message: "Connected", protocol_version: 2 } },
		{ op: "error", data: { code: "invalid_message", message: "Invalid", fatal: false } }
	]) assert.deepEqual(runtime.parseServerMessage({ data: JSON.stringify(message) }), message);
	assert.throws(() => runtime.parseServerMessage({ data: '{"type":"ok","message":"Connected"}' }));
});

test("malformed DOM payloads are rejected before DOM mutation", () => {
	for (const message of [
		{ op: "set_text", data: { part_id: "x", text: "bad shape" } },
		{ op: "set_text", data: [{ part_id: "x", text: 1 }] },
		{ op: "set_modal_open", data: [{ part_id: "x", open: "true" }] },
		{ op: "create_element_part", data: [{ part_id: "x", parent_id: "root|main", tag: "h1", text_content: "", classes: [], attributes: null }] },
		{ op: "constructor", data: [] }
	]) assert.throws(() => withMetadata({ data: JSON.stringify(message) }));
});

test("creation and text updates target concrete part IDs", () => {
	const root = new TestElement("root|main");
	nodes.set(root.id, root);
	const message = withMetadata({ data: JSON.stringify({
		op: "create_element_part",
		data: [{ part_id: "logical-heading|base", parent_id: root.id, tag: "h1",
			classes: [], attributes: { title: "Owned", onclick: "blocked" }, text_content: "Before" }]
	}) });
	dom.createElementPart(message.data);
	assert.equal(root.children[0].id, "logical-heading|base");
	assert.equal(root.children[0].attributes.onclick, undefined);
	dom.setText([{ part_id: "logical-heading|base", text: "<script>literal</script>\nAfter" }]);
	assert.equal(root.children[0].textContent, "<script>literal</script>\nAfter");
});

test("input preserves live text, defers composition, and sends the final value before submit", () => {
	const input = new TestInput("text-input|editor");
	input.type = "text";
	input.value = "editing";
	input.setAttribute("data-ui-on-input", "true");
	input.setAttribute("data-ui-on-submit", "true");
	nodes.set(input.id, input);
	const before = runtime.socket.messages.length;

	handlers.get("input")(new TestInputEvent(input, true));
	assert.equal(runtime.socket.messages.length, before);
	handlers.get("compositionend")({ target: input });
	assert.deepEqual(runtime.socket.messages.at(-1), { op: "on_input", data: { part_id: input.id, value: "editing", epoch: 0, revision: 2 } });

	let prevented = false;
	handlers.get("keydown")({ target: input, key: "Enter", isComposing: false,
		preventDefault() { prevented = true; } });
	assert.ok(prevented);
	assert.deepEqual(runtime.socket.messages.slice(-2).map(message => message.op), ["on_input", "on_submit"]);

	const submitted = runtime.socket.messages.length;
	input.readOnly = true;
	handlers.get("input")(new TestInputEvent(input, false));
	assert.equal(runtime.socket.messages.length, submitted);
	input.readOnly = false;
	input.disabled = true;
	handlers.get("keydown")({ target: input, key: "Enter" });
	assert.equal(runtime.socket.messages.length, submitted);
});

test("matching server values preserve the editor caret and checkbox values remain booleans", () => {
	const input = new TestInput("caret|editor");
	input.type = "text";
	let value = "unchanged";
	let assignments = 0;
	Object.defineProperty(input, "value", { get: () => value, set: next => { value = next; assignments++; } });
	nodes.set(input.id, input);
	dom.setValue([{ part_id: input.id, value: "unchanged" }]);
	assert.equal(assignments, 0);
	dom.setValue([{ part_id: input.id, value: "replacement" }]);
	assert.equal(assignments, 1);

	input.type = "checkbox";
	dom.setValue([{ part_id: input.id, value: "false" }]);
	assert.equal(input.checked, false);
	dom.setValue([{ part_id: input.id, value: "true" }]);
	assert.equal(input.checked, true);
});

test("modal dismissal leaves presentation to the server and suppresses close echoes", () => {
	const dialog = new TestDialog("modal|base");
	dialog.setAttribute("data-ui-modal", "true");
	dialog.setAttribute("data-ui-on-dismiss", "true");
	nodes.set(dialog.id, dialog);
	dom.setModalOpen([{ part_id: dialog.id, open: true }]);
	dom.setModalOpen([{ part_id: dialog.id, open: true }]);
	assert.equal(dialog.opens, 1);

	let prevented = false;
	handlers.get("cancel")({ target: dialog, preventDefault() { prevented = true; } });
	assert.ok(prevented);
	assert.equal(dialog.open, true);
	assert.deepEqual(runtime.socket.messages.at(-1), { op: "on_dismiss", data: { part_id: dialog.id } });

	const content = new TestElement("modal|content");
	dialog.appendChild(content);
	const before = runtime.socket.messages.length;
	handlers.get("click")({ target: content });
	assert.equal(runtime.socket.messages.length, before);
	handlers.get("click")({ target: dialog });
	assert.equal(runtime.socket.messages.length, before + 1);

	dom.setModalOpen([{ part_id: dialog.id, open: false }]);
	handlers.get("close")({ target: dialog });
	assert.equal(runtime.socket.messages.length, before + 1);
	assert.equal(dialog.closes, 1);

	dom.setModalOpen([{ part_id: dialog.id, open: true }]);
	dialog.close();
	handlers.get("close")({ target: dialog });
	assert.equal(runtime.socket.messages.length, before + 2);
});

test("delegated clicks send part identity inside data", () => {
	const button = new TestElement("logical-button|base");
	nodes.set(button.id, button);
	dom.setOnClick([{ part_id: button.id }]);
	handlers.get("click")({ target: button });
	assert.deepEqual(runtime.socket.messages.at(-1),
		{ op: "on_click", data: { part_id: "logical-button|base" } });
});

test("changes preserve nested values and checkbox boolean types", () => {
	const input = new TestInput("logical-input|editor");
	input.type = "checkbox";
	input.checked = true;
	nodes.set(input.id, input);
	dom.setOnChange([{ part_id: input.id }]);
	handlers.get("change")({ target: input });
	assert.deepEqual(runtime.socket.messages.at(-1),
		{ op: "on_change", data: { part_id: input.id, value: true, epoch: 0, revision: 1 } });
	input.type = "text";
	input.value = "edited";
	handlers.get("change")({ target: input });
	assert.equal(runtime.socket.messages.at(-1).data.value, "edited");
});

test("select choices validate IDs and preserve plain text labels", () => {
	const payload = { part_id: "select|editor", placeholder: "Choose", value: "b", options: [
		{ value: "a", label: "Same", disabled: false },
		{ value: "b", label: "<b>Same</b>", disabled: false },
		{ value: "off", label: "Unavailable", disabled: true }
	] };
	const parse = part => withMetadata({ data: JSON.stringify({ op: "set_select_options", data: [part] }) });
	for (const malformed of [
		{ ...payload, options: null },
		{ ...payload, options: [{ value: "a", label: "A", disabled: "false" }] },
		{ ...payload, options: [{ value: "a", label: "A", disabled: false }, { value: "a", label: "B", disabled: false }] },
		{ ...payload, value: "off" },
		{ ...payload, value: "missing" }
	]) assert.throws(() => parse(malformed));

	const select = new HTMLSelectElement("select|editor");
	nodes.set(select.id, select);
	dom.setSelectOptions(parse(payload).data);
	assert.equal(select.value, "b");
	assert.deepEqual(select.children.map(option => option.value), ["", "a", "b", "off"]);
	assert.equal(select.children[2].textContent, "<b>Same</b>");
	assert.equal(select.children[3].disabled, true);
});

test("form submission snapshots include latest values and omit disabled controls", () => {
	const form = new TestForm("form|base");
	form.setAttribute("data-ui-on-submit", "true");
	const name = new TestInput("name|editor");
	name.value = "";
	revisions.registerField(name);
	name.value = "Latest without an input event";
	const notes = new HTMLTextAreaElement("notes|editor");
	notes.value = "first\nsecond";
	const checkbox = new TestInput("consent|editor");
	checkbox.type = "checkbox";
	checkbox.checked = true;
	const ignored = new TestInput("disabled|editor");
	ignored.disabled = true;
	const foreign = new TestInput("foreign|editor");
	for (const field of [name, notes, checkbox, ignored, foreign]) {
		field.setAttribute("data-ui-form-field", "true");
		field.form = field === foreign ? new TestForm("other") : form;
		form.elements.push(field);
	}

	form.requestSubmit();
	assert.deepEqual(runtime.socket.messages.at(-1), { op: "on_submit", data: { part_id: form.id, fields: [
		{ part_id: name.id, value: name.value, epoch: 0, revision: 1 }, { part_id: notes.id, value: notes.value, epoch: 0, revision: 0 }, { part_id: checkbox.id, value: true, epoch: 0, revision: 0 }
	] } });

	name.setAttribute("data-ui-on-submit", "true");
	handlers.get("keydown")({ target: name, key: "Enter", preventDefault() {} });
	assert.equal(runtime.socket.messages.at(-1).data.part_id, form.id);

	notes.setAttribute("data-ui-on-submit", "true");
	const before = runtime.socket.messages.length;
	handlers.get("keydown")({ target: notes, key: "Enter", preventDefault() { assert.fail("Enter must remain a newline"); } });
	assert.equal(runtime.socket.messages.length, before);
	handlers.get("keydown")({ target: notes, key: "Enter", ctrlKey: true, preventDefault() {} });
	assert.equal(runtime.socket.messages.length, before + 1);
	assert.equal(runtime.socket.messages.at(-1).data.part_id, form.id);
});

test("removing a subtree closes open dialogs silently and keeps the surviving opener focused", () => {
	const opener = new HTMLButtonElement("opener");
	const group = new TestElement("removable-group");
	const dialog = new TestDialog("removable-modal");
	const input = new TestInput("focused-editor");
	document.body.appendChild(opener);
	document.body.appendChild(group);
	group.appendChild(dialog);
	dialog.appendChild(input);
	dialog.setAttribute("data-ui-modal", "true");
	dialog.dataset.uiModalOpen = "true";
	dialog.opener = opener;
	dialog.showModal();
	document.activeElement = input;
	const before = runtime.socket.messages.length;
	dom.removeElementPart([{ part_id: group.id }]);
	assert.equal(dialog.open, false);
	assert.equal(dialog.closes, 1);
	assert.equal(nodes.has(group.id), false);
	assert.equal(nodes.has(input.id), false);
	assert.equal(document.activeElement, opener);
	handlers.get("close")({ target: dialog });
	assert.equal(runtime.socket.messages.length, before);
});

test("number edits preserve invalid drafts and explicit empty values clear native bad input", () => {
	const input = new TestInput("number|editor");
	input.type = "number";
	input.value = "";
	input.validity = { badInput: true };
	input.setAttribute("data-ui-on-input", "true");
	input.setAttribute("data-ui-on-change", "true");
	nodes.set(input.id, input);
	assert.equal(dom.getElementValue(input), "NaN");
	handlers.get("input")({ target: input });
	assert.equal(runtime.socket.messages.at(-1).data.value, "NaN");
	handlers.get("change")({ target: input });
	assert.equal(runtime.socket.messages.at(-1).data.value, "NaN");
	let cleared = false;
	Object.defineProperty(input, "value", { get: () => "", set: value => {
		assert.equal(value, "");
		cleared = true;
		input.validity.badInput = false;
	} });
	dom.setValue([{ part_id: input.id, value: "", epoch: 1, base_epoch: 0, client_revision: 2, replace: true }]);
	assert.ok(cleared);
	assert.equal(dom.getElementValue(input), "");
});

test("radio payloads validate choices and forms collect exactly one value per group", () => {
	const parse = payload => withMetadata({ data: JSON.stringify({ op: "set_radio_options", data: [payload] }) });
	const payload = { part_id: "radio|editor", value: "a", options: [
		{ value: "a", label: "Same", disabled: false }, { value: "b", label: "Same", disabled: false }
	] };
	assert.equal(parse(payload).data[0].value, "a");
	assert.throws(() => parse({ ...payload, value: "missing" }));
	assert.throws(() => parse({ ...payload, options: [payload.options[0], payload.options[0]] }));
	assert.throws(() => parse({ ...payload, options: [{ value: "a", label: "A", disabled: true }] }));
	assert.throws(() => parse({ ...payload, options: [{ value: "a", label: "A", disabled: "false" }] }));

	const form = new TestForm("radio-form");
	form.setAttribute("data-ui-on-submit", "true");
	const group = new HTMLFieldSetElement(payload.part_id);
	group.setAttribute("data-ui-radio-group", "true");
	group.setAttribute("data-ui-form-field", "true");
	group.setAttribute("data-ui-on-change", "true");
	group.form = form;
	const radios = [new TestInput(), new TestInput()];
	radios[0].value = "a";
	radios[1].value = "b";
	for (const radio of radios) { group.appendChild(radio); radio.form = form; }
	group.querySelector = () => radios.find(radio => radio.checked) ?? null;
	group.querySelectorAll = () => radios;
	nodes.set(group.id, group);
	form.elements = [group, ...radios];
	dom.setValue([{ part_id: group.id, value: "b", epoch: 0, revision: 1 }]);
	assert.equal(radios[0].checked, false);
	assert.equal(radios[1].checked, true);
	handlers.get("change")({ target: radios[1] });
	assert.deepEqual(runtime.socket.messages.at(-1), { op: "on_change", data: { part_id: group.id, value: "b", epoch: 0, revision: 1 } });
	form.requestSubmit();
	assert.deepEqual(runtime.socket.messages.at(-1).data.fields, [{ part_id: group.id, value: "b", epoch: 0, revision: 1 }]);
	dom.setValue([{ part_id: group.id, value: "", client_revision: 1 }]);
	form.requestSubmit();
	assert.deepEqual(runtime.socket.messages.at(-1).data.fields, [{ part_id: group.id, value: "", epoch: 0, revision: 1 }]);
	radios[0].disabled = true;
	const before = runtime.socket.messages.length;
	handlers.get("change")({ target: radios[0] });
	assert.equal(runtime.socket.messages.length, before);
	group.disabled = true;
	form.requestSubmit();
	assert.deepEqual(runtime.socket.messages.at(-1).data.fields, []);
});

test("removing the focused control preserves its parent and chooses surviving focus", () => {
	const parent = new TestElement("surviving-parent");
	const button = new HTMLButtonElement("removed-button");
	document.body.appendChild(parent);
	parent.appendChild(button);
	document.activeElement = button;
	dom.removeElementPart([{ part_id: button.id }, { part_id: "already-missing" }]);
	assert.equal(nodes.has(parent.id), true);
	assert.equal(parent.children.length, 0);
	assert.equal(document.activeElement, parent);
	assert.equal(parent.getAttribute("tabindex"), "-1");
	document.body.focus();
	assert.equal(parent.hasAttribute("tabindex"), false);
});


test("same-page navigation accepts bounded hash locations and rejects external targets", () => {
	for (const hash of ["", "#/tasks", "#/task/42"]) {
		const message = { op: "set_location", data: { hash } };
		assert.deepEqual(withMetadata({ data: JSON.stringify(message) }), message);
	}
	for (const hash of ["https://example.com", "/tasks", 42, "#".repeat(2049)]) {
		assert.throws(() => withMetadata({ data: JSON.stringify({ op: "set_location", data: { hash } }) }));
	}
});

test("whole batches validate bounded sequences, revisions, and all commands before application", () => {
	const operation = { op: "set_text", data: [{ part_id: "x", text: "hello" }] };
	const batch = operations => ({ op: "update_batch", data: { sequence: 1, operations } });
	const parse = message => runtime.parseServerMessage({ data: JSON.stringify(message) });
	assert.deepEqual(parse(batch([operation])), batch([operation]));
	for (const message of [
		batch([]), batch(Array(129).fill(operation)), batch([operation, { op: "unknown", data: [] }]),
		batch([batch([operation])]), { ...batch([operation]), data: { sequence: 0, operations: [operation] } },
		{ ...batch([operation]), data: { sequence: Number.MAX_SAFE_INTEGER + 1, operations: [operation] } },
		{ op: "connected", data: { message: "old", protocol_version: 1 } },
		{ op: "error", data: { code: "bad", message: "bad" } },
		batch([{ op: "set_value", data: [{ part_id: "x", value: "bad", ...fieldUpdate, client_revision: -1 }] }]),
		batch([{ op: "set_text", data: [{ part_id: "x", text: "x".repeat(1024 * 1024) }] }])
	]) assert.throws(() => parse(message));
});

test("newer browser drafts survive delayed corrections and matching corrections preserve caret", () => {
	const input = new TestInput("revision|editor");
	input.value = "before";
	nodes.set(input.id, input);
	revisions.registerField(input);
	input.value = "first";
	assert.deepEqual(revisions.captureEdit(input, true), { epoch: 0, revision: 1 });
	input.value = "newest";
	revisions.captureEdit(input, true);
	dom.setValue([{ part_id: input.id, value: "FIRST", client_revision: 1 }]);
	assert.equal(input.value, "newest");
	dom.setValue([{ part_id: input.id, value: "NEWEST", client_revision: 2 }]);
	assert.equal(input.value, "NEWEST");
	assert.deepEqual(revisions.captureEdit(input), { epoch: 0, revision: 2 });
});

test("composition protects drafts and an explicit reset cannot resurrect old composing text", () => {
	const input = new TestInput("composition|editor");
	input.value = "before";
	nodes.set(input.id, input);
	revisions.registerField(input);
	revisions.beginComposition(input);
	input.value = "composing";
	revisions.captureEdit(input, true);
	dom.setValue([{ part_id: input.id, value: "ordinary", client_revision: 1 }]);
	assert.equal(input.value, "composing");
	dom.setValue([{ part_id: input.id, value: "reset", epoch: 1, replace: true }]);
	assert.equal(input.value, "reset");
	input.value = "late composition";
	assert.equal(revisions.captureEdit(input, true), null);
	assert.equal(revisions.endComposition(input), false);
	assert.equal(input.value, "reset");
	assert.deepEqual(revisions.captureEdit(input), { epoch: 1, revision: 0 });
	dom.setValue([{ part_id: input.id, value: "obsolete", client_revision: 1 }]);
	assert.equal(input.value, "reset");
});

test("choice replacement rebases surviving newer selections and invalidates removed selections", () => {
	const select = new HTMLSelectElement("revision-select|editor");
	select.value = "a";
	nodes.set(select.id, select);
	revisions.registerField(select);
	select.value = "b";
	revisions.captureEdit(select, true);
	const options = [{ value: "a", disabled: false }, { value: "b", disabled: false }];
	assert.deepEqual(revisions.prepareChoiceUpdate(select,
		{ ...fieldUpdate, epoch: 1, value: "a" }, options), { value: "b", edit: { epoch: 1, revision: 1 } });
	assert.deepEqual(revisions.captureEdit(select), { epoch: 1, revision: 1 });
	assert.equal(revisions.prepareChoiceUpdate(select, { ...fieldUpdate, value: "a" }, options), null);
	select.value = "a";
	revisions.captureEdit(select, true);
	assert.deepEqual(revisions.prepareChoiceUpdate(select,
		{ ...fieldUpdate, epoch: 2, base_epoch: 1, value: "b" }, [options[1]]), { value: "b" });
});
