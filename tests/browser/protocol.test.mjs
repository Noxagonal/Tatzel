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
	/** @brief Removes a stored attribute. @param name Attribute name. @pre None. @post The attribute is absent. */
	removeAttribute(name) { delete this.attributes[name]; }
	/** @brief Checks native disabled state, including a disabled ancestor fieldset. @param selector Supported pseudo-class. @returns Whether disabled. @pre None. @post State is unchanged. */
	matches(selector) { return selector === ":disabled" && (Boolean(this.disabled) || Boolean(this.parentElement?.matches(selector))); }
	/** @brief Replaces native option nodes. @param children Replacement nodes. @pre None. @post Children are replaced. */
	replaceChildren(...children) { this.children = children; }
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
	close() { this.open = false; this.closes = (this.closes ?? 0) + 1; }
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
globalThis.HTMLInputElement = TestInput;
globalThis.HTMLTextAreaElement = class extends TestElement {};
globalThis.HTMLSelectElement = class extends TestElement {};
globalThis.HTMLFormElement = TestForm;
globalThis.HTMLDialogElement = TestDialog;
globalThis.HTMLButtonElement = class extends TestElement {};
globalThis.InputEvent = TestInputEvent;
globalThis.WebSocket = TestSocket;
globalThis.window = { location: { protocol: "https:", host: "example.test" } };
globalThis.document = {
	getElementById: id => nodes.get(id) ?? null,
	createElement: () => new TestElement(),
	addEventListener: (event, callback) => handlers.set(event, callback)
};

const runtime = await import(pathToFileURL(join(generated, "websocket.mjs")));
const dom = await import(pathToFileURL(join(generated, "dom-ops.mjs")));
const events = await import(pathToFileURL(join(generated, "events.mjs")));
events.registerBrowserEventHandlers();

test("status and error messages use the same op/data envelope", () => {
	assert.equal(runtime.socket.url, "wss://example.test/ws");
	for (const message of [
		{ op: "connected", data: { message: "Connected" } },
		{ op: "error", data: { code: "invalid_message", message: "Invalid" } }
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
	]) assert.throws(() => runtime.parseServerMessage({ data: JSON.stringify(message) }));
});

test("creation and text updates target concrete part IDs", () => {
	const root = new TestElement("root|main");
	nodes.set(root.id, root);
	const message = runtime.parseServerMessage({ data: JSON.stringify({
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
	assert.deepEqual(runtime.socket.messages.at(-1), { op: "on_input", data: { part_id: input.id, value: "editing" } });

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
		{ op: "on_change", data: { part_id: input.id, value: true } });
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
	const parse = part => runtime.parseServerMessage({ data: JSON.stringify({ op: "set_select_options", data: [part] }) });
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
		{ part_id: name.id, value: name.value }, { part_id: notes.id, value: notes.value }, { part_id: checkbox.id, value: true }
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
