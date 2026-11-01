import assert from "node:assert/strict";
import { spawn } from "node:child_process";
import { once } from "node:events";
import { mkdtemp, rm } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join, resolve } from "node:path";
import { setTimeout as pause } from "node:timers/promises";

/**
 * @brief Waits for an observable application state with a bounded deadline.
 * @param probe Synchronous state probe.
 * @returns First truthy result.
 * @pre The sample server is running.
 * @post A state is observed or a descriptive timeout is raised.
 */
async function waitFor(probe) {
	const deadline = Date.now() + 8000;
	while (Date.now() < deadline) {
		const result = probe();
		if (result) return result;
		await pause(15);
	}
	throw new Error("Timed out waiting for workspace state");
}

/** @brief Live protocol client maintaining the current server-generated part tree. */
class Client {
	/**
	 * @brief Connects a protocol observer to one independent browser session.
	 * @param hash Initial same-page route.
	 * @pre The server is accepting WebSocket upgrades.
	 * @post Parts and subsequent updates are tracked by concrete identity.
	 */
	constructor(hash = "#/overview") {
		this.parts = new Map();
		this.errors = [];
		this.socket = new WebSocket("ws://127.0.0.1:8080/ws");
		this.socket.addEventListener("message", event => {
			const message = JSON.parse(event.data);
			if (message.op === "error") this.errors.push(message.data);
			if (message.op === "set_location") this.hash = message.data.hash;
			if (!Array.isArray(message.data)) return;
			for (const entry of message.data) {
				if (message.op === "create_element_part") this.parts.set(entry.part_id, structuredClone(entry));
				const part = this.parts.get(entry.part_id);
				if (!part) continue;
				if (message.op === "set_text") part.text_content = entry.text;
				if (message.op === "set_attribute") part.attributes[entry.attribute] = entry.attribute_value;
				if (message.op === "remove_attribute") delete part.attributes[entry.attribute];
				if (message.op === "set_value") part.value = entry.value;
				if (message.op === "set_select_options") { part.options = entry.options; part.value = entry.value; }
				if (message.op === "remove_element_part") this.remove(entry.part_id);
			}
		});
		this.ready = once(this.socket, "open").then(() => this.send("connect", { route_path: "/", hash }));
	}
	/** @brief Removes the observed subtree. @param id Concrete root part. @pre None. @post Descendants are absent. */
	remove(id) {
		for (const part of this.parts.values()) if (part.parent_id === id) this.remove(part.part_id);
		this.parts.delete(id);
	}
	/** @brief Sends a browser event. @param op Operation name. @param data Envelope payload. @pre Socket is open. @post The event is sent. */
	send(op, data) { this.socket.send(JSON.stringify({ op, data })); }
	/** @brief Finds live exact plain text. @param text Text to find. @returns Matching part or undefined. @pre None. @post State is unchanged. */
	label(text) { return [...this.parts.values()].find(part => part.text_content === text); }
	/** @brief Resolves a live field by label. @param text Label text. @returns Native field part. @pre The field exists. @post State is unchanged. */
	field(text) { const label = this.label(text); assert.ok(label, `Missing field ${text}`); const parent = this.parts.get(label.parent_id); return this.parts.get(label.attributes.for ?? parent?.attributes.for ?? label.parent_id); }
	/** @brief Changes the same-page route. @param hash Hash location. @pre Socket is open. @post Navigation is requested. */
	navigate(hash) { this.send("on_navigate", { hash }); }
	/** @brief Clicks an exact action label. @param text Button text. @pre The button exists. @post Its concrete source is dispatched. */
	click(text) { const part = this.label(text); assert.ok(part, `Missing action ${text}`); this.send("on_click", { part_id: part.part_id }); }
	/** @brief Submits complete current field values. @param overrides Values keyed by accessible label. @pre One form is visible. @post A complete snapshot is sent. */
	submit(overrides) {
		const replacements = new Map(Object.entries(overrides).map(([label, value]) => [this.field(label).part_id, value]));
		const fields = [...this.parts.values()].filter(part => part.attributes["data-ui-form-field"]).map(part => ({
			part_id: part.part_id,
			value: replacements.has(part.part_id) ? replacements.get(part.part_id) : part.attributes.type === "checkbox"
				? (part.value ?? (Object.hasOwn(part.attributes, "checked") ? "true" : "false")) === "true"
				: part.value ?? part.attributes.value ?? part.text_content ?? ""
		}));
		const form = [...this.parts.values()].find(part => part.tag === "form");
		assert.ok(form);
		this.send("on_submit", { part_id: form.part_id, fields });
	}
}

const directory = await mkdtemp(join(tmpdir(), "tatzel-workspace-smoke-"));
const binary = resolve(process.argv[2]);
let server;
let diagnostics = "";
const clients = [];

/**
 * @brief Starts the real sample process with isolated persistent storage.
 * @pre No process is listening on the test port.
 * @post HTTP and static assets are available or startup fails.
 */
async function start() {
	server = spawn(binary, [], { env: { ...process.env, TATZEL_WORKSPACE_DB: join(directory, "workspace.sqlite3") }, stdio: ["ignore", "pipe", "pipe"] });
	server.stdout.on("data", data => { diagnostics += data; });
	server.stderr.on("data", data => { diagnostics += data; });
	for (let attempt = 0; attempt < 100; attempt++) {
		try {
			const response = await fetch("http://127.0.0.1:8080/", { headers: { Connection: "close" }, signal: AbortSignal.timeout(300) });
			if (response.ok) {
				assert.match(await response.text(), /Tatzel Workspace/);
				const css = await fetch("http://127.0.0.1:8080/workspace/workspace.css", { headers: { Connection: "close" } });
				assert.equal(css.status, 200);
				return;
			}
		} catch {}
		await pause(30);
	}
	throw new Error("Workspace did not start: " + diagnostics);
}

/**
 * @brief Closes protocol clients and stops the sample gracefully.
 * @pre server was started.
 * @post The port is released and the database worker has drained.
 */
async function stop() {
	for (const client of clients.splice(0)) client.socket.close();
	if (server && server.exitCode === null) {
		const exit = once(server, "exit");
		server.kill("SIGTERM");
		await exit;
	}
}

try {
	await start();
	const first = new Client();
	const second = new Client("#/tasks");
	clients.push(first, second);
	await Promise.all(clients.map(client => client.ready));
	await waitFor(() => first.label("Make room for good work.") && second.label("Search tasks"));
	assert.ok([...first.parts.values()].some(part => part.attributes["data-ui-appearance"] === "workspace-shell"));

	first.navigate("#/project/new");
	await waitFor(() => first.label("Save project"));
	first.submit({ "Project name": "Integration project ✓", "Description": "Persistent sample data" });
	await waitFor(() => first.label("Integration project ✓"));
	first.navigate("#/task/new");
	await waitFor(() => first.label("Save task"));
	const project = first.field("Project").options.find(option => option.label === "Integration project ✓").value;
	first.submit({ "Task title": "Integration task ✓", "Description": "Original description", "Project": project, "Status": "todo", "Priority": "high" });
	await waitFor(() => first.label("Integration task ✓") && second.label("Integration task ✓"));
	const taskRoute = first.label("Integration task ✓").attributes.href;
	first.navigate(taskRoute);
	second.navigate(taskRoute);
	await waitFor(() => first.label("Save task") && second.label("Save task"));
	const draftID = second.field("Task title").part_id;
	second.send("on_input", { part_id: draftID, value: "My unsaved draft" });
	first.submit({ "Task title": "Saved in first window" });
	await waitFor(() => first.label("Saved in first window"));
	await waitFor(() => second.label("Workspace updated. Your open draft has been preserved."));
	assert.equal(second.field("Task title").part_id, draftID, "external updates preserve editor identity");
	second.submit({ "Task title": "My unsaved draft" });
	await waitFor(() => [...second.parts.values()].some(part => part.text_content.includes("This item changed or was removed")));
	assert.equal(second.field("Task title").part_id, draftID, "conflicts preserve the draft form");

	first.navigate(taskRoute);
	await waitFor(() => first.label("Archive task"));
	first.click("Archive task");
	await waitFor(() => first.label("Task archive state updated."));
	assert.equal(first.label("Saved in first window"), undefined);
	const archived = first.field("Show archived tasks and projects");
	first.send("on_change", { part_id: archived.part_id, value: true });
	await waitFor(() => first.label("Saved in first window"));
	first.navigate(taskRoute);
	await waitFor(() => first.label("Restore task"));
	first.click("Restore task");
	await waitFor(() => first.label("Search tasks"));
	first.navigate(taskRoute);
	await waitFor(() => first.label("Delete task"));
	first.click("Delete task");
	await waitFor(() => first.label("Confirm"));
	const oldConfirmation = first.label("Confirm").part_id;
	first.click("Cancel");
	first.click("Delete task");
	await waitFor(() => first.label("Confirm")?.part_id !== oldConfirmation && first.label("Confirm"));
	first.click("Confirm");
	await waitFor(() => first.label("Task deleted."));
	assert.equal(first.label("Saved in first window"), undefined);

	first.navigate("#/settings");
	await waitFor(() => first.label("Save preferences"));
	first.submit({ "Use compact task rows": true, "Show completed tasks by default": false });
	await waitFor(() => first.label("Preferences saved."));
	first.navigate("#/not-a-view");
	await waitFor(() => first.label("This view could not be found."));
	assert.deepEqual(first.errors, []);
	assert.deepEqual(second.errors, []);
	await stop();

	await start();
	const reopened = new Client("#/projects");
	clients.push(reopened);
	await reopened.ready;
	await waitFor(() => reopened.label("Integration project ✓"));
	reopened.navigate("#/settings");
	await waitFor(() => reopened.label("Save preferences"));
	assert.equal(reopened.field("Use compact task rows").attributes.checked, "");
	reopened.click("Restore sample data");
	await waitFor(() => reopened.label("Confirm"));
	reopened.click("Confirm");
	await waitFor(() => reopened.label("Sample workspace restored."));
	await waitFor(() => reopened.label("Make room for good work."));
	assert.equal(reopened.label("Integration project ✓"), undefined);
	const errors = reopened.errors.length;
	reopened.navigate("https://example.com");
	await waitFor(() => reopened.errors.length > errors);
	assert.equal(reopened.errors.at(-1).code, "invalid_message");
	console.log("Workspace integration passed: CRUD, two-client sync, draft conflicts, archive/restore/delete, persistence, preferences, reset, and navigation validation.");
} catch (error) {
	console.error(diagnostics);
	throw error;
} finally {
	await stop();
	await rm(directory, { recursive: true, force: true });
}
