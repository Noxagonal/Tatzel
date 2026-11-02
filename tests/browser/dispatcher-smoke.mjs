import { ProtocolSocket } from "./protocol-client.mjs";
import assert from "node:assert/strict";
import { spawn } from "node:child_process";
import { once } from "node:events";
import { setTimeout as pause } from "node:timers/promises";
import { test } from "node:test";

/**
 * @brief Waits for a bounded observable integration state.
 * @param predicate Synchronous state probe.
 * @returns The first truthy probe result.
 * @pre The server is processing asynchronous work.
 * @post The requested state is observed or a timeout is thrown.
 */
async function waitFor(predicate)
{
	const deadline = Date.now() + 5000;
	while (Date.now() < deadline) {
		const value = predicate();
		if (value) return value;
		await pause(10);
	}
	throw new Error("Timed out waiting for dispatcher integration state");
}

/**
 * @brief Builds an isolated dispatcher example page using its real browser protocol.
 * @returns Transport, accumulated messages, and page interaction helpers.
 * @pre The example server is listening on port 8080.
 * @post All controls and event registrations have arrived.
 */
async function connectPage()
{
	const socket = new ProtocolSocket("ws://127.0.0.1:8080/ws");
	const messages = [];
	socket.addEventListener("message", event => messages.push(JSON.parse(event.data)));
	await once(socket, "open", { signal: AbortSignal.timeout(5000) });
	socket.send(JSON.stringify({ op: "connect", data: { route_path: "/dispatcher" } }));
	const parts = () => messages.filter(message => message.op === "create_element_part").flatMap(message => message.data);
	await waitFor(() => {
		const last = parts().find(part => part.text_content === "Throw dispatched exception");
		return last && messages.some(message => message.op === "set_on_click" && message.data[0].part_id === last.part_id);
	});
	const send = (op, part_id, data = {}) => socket.send(JSON.stringify({ op, data: { part_id, ...data } }));
	const status = parts().find(part => part.text_content === "Ready").part_id;
	const text = value => messages.some(message => message.op === "set_text"
		&& message.data.some(part => part.part_id === status && part.text === value));
	return { socket, messages, parts, text,
		click: label => send("on_click", parts().find(part => part.tag === "button" && part.text_content === label).part_id),
		save: value => send("on_submit", parts().find(part => part.tag === "form").part_id, {
			fields: [{ part_id: parts().find(part => part.tag === "input").part_id, value }]
		}) };
}

test("background completion respects page ownership, removal, errors, disconnects, and shutdown", { timeout: 30000 }, async () => {
	const server = spawn(process.argv[2], [], { stdio: ["ignore", "pipe", "pipe"] });
	let output = "";
	server.stdout.on("data", chunk => { output += chunk; });
	server.stderr.on("data", chunk => { output += chunk; });
	const pages = [];
	try {
		await waitFor(() => output.includes("Tatzel listening"));
		const first = await connectPage();
		const other = await connectPage();
		pages.push(first, other);
		first.save("First client");
		await waitFor(() => first.text("Saving"));
		assert.ok(!other.text("Saving"));
		await waitFor(() => first.text("Save complete"));
		assert.ok(first.messages.some(message => message.op === "set_text" && message.data.some(part => part.text === "Saved: First client")));
		assert.ok(!other.text("Save complete"));

		const start = first.messages.length;
		first.save("Removed target");
		await waitFor(() => first.messages.slice(start).some(message => message.op === "set_text" && message.data.some(part => part.text === "Saving")));
		first.click("Remove result");
		await waitFor(() => first.text("Saved after result removal"));
		assert.ok(!first.messages.some(message => message.op === "set_text" && message.data.some(part => part.text === "Saved: Removed target")));

		first.click("Throw dispatched exception");
		await waitFor(() => first.text("Recovered after dispatched error"));
		await waitFor(() => output.includes("Dispatched example failure"));
		assert.match(output, /Route: \/dispatcher[\s\S]*Operation: dispatch[\s\S]*Submitted at:/);
		assert.ok(output.includes("Callback: Dispatched callback"));
		assert.deepEqual(first.messages.filter(message => message.op === "error").map(message => [message.data.code, message.data.fatal]), [["handler_failed", false]]);

		other.save("Closed page");
		await waitFor(() => other.text("Saving"));
		other.socket.close();
		await once(other.socket, "close");
		await waitFor(() => output.includes("completion discarded after page closure"));

		// Leave a worker active while the server closes its loop and session registry.
		const pending = await connectPage();
		pages.push(pending);
		pending.save("Server shutdown");
		await waitFor(() => pending.text("Saving"));
		server.kill("SIGTERM");
		const [code, signal] = await once(server, "exit");
		assert.equal(code, 0, output);
		assert.equal(signal, null, output);
		assert.ok(output.includes("Tatzel stopped"));
	} finally {
		for (const page of pages) page.socket.close();
		if (server.exitCode === null) { server.kill("SIGTERM"); await once(server, "exit"); }
	}
});
