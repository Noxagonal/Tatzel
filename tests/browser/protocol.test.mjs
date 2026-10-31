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
	appendChild(child) { this.children.push(child); nodes.set(child.id, child); }
	/** @brief Records an attribute. @param name Attribute name. @param value Attribute value. @pre None. @post The attribute is stored. */
	setAttribute(name, value) { this.attributes[name] = value; }
	/** @brief Finds this marked event source. @param selector Data-attribute selector. @returns This node if marked, otherwise null. @pre None. @post State is unchanged. */
	closest(selector) {
		const marker = selector.includes("on-click") ? "uiOnClick"
			: selector.includes("on-change") ? "uiOnChange" : "uiOnSubmit";
		return this.dataset[marker] ? this : null;
	}
}

/** @brief Minimal form input for checkbox and text event checks. */
class TestInput extends TestElement {}

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
globalThis.HTMLDialogElement = class extends TestElement {};
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
	dom.setText([{ part_id: "logical-heading|base", text: "After" }]);
	assert.equal(root.children[0].textContent, "After");
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
