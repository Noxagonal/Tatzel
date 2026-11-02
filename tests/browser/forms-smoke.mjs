import { ProtocolSocket } from "./protocol-client.mjs";
import assert from "node:assert/strict";
import { spawn } from "node:child_process";
import { once } from "node:events";
import { setTimeout as pause } from "node:timers/promises";
import { test } from "node:test";

/**
 * @brief Waits for an observable integration state with a bounded timeout.
 * @param predicate Synchronous state probe.
 * @returns The first truthy probe result.
 * @pre A server or browser transport is processing work.
 * @post The state is observed or a timeout is thrown.
 */
async function waitFor(predicate)
{
	const deadline = Date.now() + 5000;
	while (Date.now() < deadline) {
		const result = predicate();
		if (result) return result;
		await pause(20);
	}
	throw new Error("Timed out waiting for form integration state");
}

/**
 * @brief Builds the form example over a real WebSocket.
 * @returns Transport, messages, and helpers resolving adapter-assigned source parts.
 * @pre The example server is listening on port 8080.
 * @post The complete form example is built and ready for events.
 */
async function connectForm()
{
	const socket = new ProtocolSocket("ws://127.0.0.1:8080/ws");
	const messages = [];
	socket.addEventListener("message", event => messages.push(JSON.parse(event.data)));
	await once(socket, "open", { signal: AbortSignal.timeout(5000) });
	socket.send(JSON.stringify({ op: "connect", data: { route_path: "/forms" } }));
	const created = () => messages.filter(message => message.op === "create_element_part").flatMap(message => message.data);
	await waitFor(() => {
		const toggle = created().find(part => part.text_content === "Toggle form");
		return toggle && messages.some(message => message.op === "set_on_click" && message.data[0].part_id === toggle.part_id);
	});
	const parts = created();
	const part = id => parts.find(part => part.part_id === id);
	const editor = text => {
		const label = parts.find(part => part.text_content === text);
		return label.attributes.for ?? part(label.parent_id).attributes.for;
	};
	return { socket, messages, parts, editor,
		form: parts.find(part => part.tag === "form").part_id,
		status: parts.find(part => part.text_content === "Ready").part_id,
		button: text => parts.find(part => part.tag === "button" && part.text_content === text).part_id };
}

test("forms validate complete snapshots before callbacks and isolate client state", { timeout: 30000 }, async () => {
	const server = spawn(process.argv[2], [], { stdio: ["ignore", "pipe", "pipe"] });
	let diagnostics = "";
	server.stdout.on("data", chunk => { diagnostics += chunk; });
	server.stderr.on("data", chunk => { diagnostics += chunk; });
	let first;
	let second;
	try {
		let ready = false;
		for (let attempt = 0; attempt < 100; attempt++) {
			try {
				const response = await fetch("http://127.0.0.1:8080/forms", { headers: { Connection: "close" }, signal: AbortSignal.timeout(500) });
				if (response.ok) { ready = true; break; }
			} catch {}
			await pause(30);
		}
		assert.ok(ready, diagnostics);
		const stylesheet = await fetch("http://127.0.0.1:8080/framework/layout.css", { headers: { Connection: "close" } });
		assert.equal(stylesheet.status, 200);
		assert.match(await stylesheet.text(), /\.tatzel-row/);
		first = await connectForm();
		second = await connectForm();
		const send = (op, part_id, extra = {}) => first.socket.send(JSON.stringify({ op, data: { part_id, ...extra } }));
		const submit = fields => send("on_submit", first.form, { fields });
		const field = (name, value) => ({ part_id: first.editor(name), value });
		const values = (name = "Ada", delivery = "email", notes = "first\nsecond", consent = true) => [
			field("Name", name), field("Delivery", delivery), field("Notes", notes),
			field("I agree to be contacted", consent), field("Reference", "forged browser value")
		];
		const status = text => waitFor(() => first.messages.some(message => message.op === "set_text"
			&& message.data.some(part => part.part_id === first.status && part.text === text)));
		const errors = () => first.messages.filter(message => message.op === "error");
		const invalidSnapshot = async fields => {
			const count = errors().length;
			submit(fields);
			await waitFor(() => errors().length > count);
			assert.equal(errors().at(-1).data.code, "invalid_message");
		};
		assert.ok(first.parts.some(part => part.attributes.style?.includes("flex-direction:row")));
		assert.ok(first.parts.some(part => part.attributes.style?.includes("flex-direction:column")));
		const submitButton = first.parts.find(part => part.text_content === "Send");
		assert.equal(submitButton.attributes.type, "submit");
		assert.equal(submitButton.attributes["data-ui-on-click"], undefined);
		assert.ok(first.messages.some(message => message.op === "set_select_options"
			&& message.data[0].options.some(option => option.value === "postal" && option.disabled)));

		submit(values("", "", "", false));
		await status("Check highlighted fields");
		assert.equal(first.messages.filter(message => message.op === "set_attribute"
			&& message.data[0].attribute === "aria-invalid").length, 3);

		const beforeFieldSubmit = errors().length;
		send("on_submit", first.editor("Name"));
		await waitFor(() => errors().length > beforeFieldSubmit);
		assert.equal(errors().at(-1).data.code, "invalid_message");

		await invalidSnapshot(values().slice(1));
		const duplicated = values(); duplicated[4] = duplicated[0];
		await invalidSnapshot(duplicated);
		const wrongType = values(); wrongType[2].value = true;
		await invalidSnapshot(wrongType);
		const foreign = values(); foreign[0].part_id = second.editor("Name");
		await invalidSnapshot(foreign);
		await invalidSnapshot([...values(), field("Internal", "forged disabled value")]);
		await invalidSnapshot(values("Ada", "postal"));

		submit(values());
		await status("Sent: Ada | email | first\nsecond | TATZEL-001");
		assert.equal(first.messages.filter(message => message.op === "set_value").length, 0, "snapshot values must not echo");
		submit(values("Ada", "email", "x".repeat(81)));
		await waitFor(() => first.messages.some(message => message.op === "set_text"
			&& message.data[0].text === "Notes are too long."));

		send("on_click", first.button("Refresh choices"));
		await waitFor(() => first.messages.some(message => message.op === "set_select_options"
			&& message.data[0].options.some(option => option.value === "pickup")));
		const refreshed = first.messages.filter(message => message.op === "set_select_options").at(-1).data[0];
		assert.equal(refreshed.value, "");
		send("on_click", first.button("Use phone"));
		await waitFor(() => first.messages.some(message => message.op === "set_value" && message.data[0].value === "phone"));

		send("on_click", first.button("Toggle form"));
		await waitFor(() => first.messages.some(message => message.op === "set_attribute"
			&& message.data[0].attribute === "disabled" && message.data[0].part_id === first.parts.find(part => part.tag === "fieldset").part_id));
		const updates = first.messages.filter(message => message.op === "set_text").length;
		submit(values("Ignored", "phone"));
		send("on_click", first.button("Toggle form"));
		await waitFor(() => first.messages.some(message => message.op === "remove_attribute"
			&& message.data[0].attribute === "disabled"));
		assert.equal(first.messages.filter(message => message.op === "set_text").length, updates);
		submit(values("Ada", "phone", "Done"));
		await status("Sent: Ada | phone | Done | TATZEL-001");
		assert.equal(second.messages.filter(message => message.op === "set_text").length, 0);
	}
	catch (error) { console.error(diagnostics); throw error; }
	finally {
		first?.socket.close(); second?.socket.close();
		const exited = once(server, "exit");
		server.kill("SIGTERM");
		await Promise.race([exited, pause(1000, undefined, { ref: false })]);
		if (server.exitCode === null && server.signalCode === null) { server.kill("SIGKILL"); await exited; }
	}
});
