import assert from "node:assert/strict";
import { spawn } from "node:child_process";
import { once } from "node:events";
import { setTimeout as pause } from "node:timers/promises";
import { test } from "node:test";

/** @brief Awaits a transport condition. @param predicate State probe. @returns Truthy result. @pre Server is running. @post Completes or throws within five seconds. */
async function waitFor(predicate)
{
	for (let attempt = 0; attempt < 250; ++attempt) {
		const result = predicate();
		if (result) return result;
		await pause(20);
	}
	throw new Error("Timed out waiting for protocol state");
}

/** @brief Opens an unmodified native wire client. @param path Registered route. @param version Explicit protocol version. @returns Socket, frames, send, and operation accessors. @pre Server is listening. @post Transport acknowledgment is received; initialization has been sent. */
async function connect(path, version = 2)
{
	const socket = new WebSocket("ws://127.0.0.1:8080/ws");
	const frames = [];
	socket.addEventListener("message", event => frames.push(JSON.parse(event.data)));
	await once(socket, "open");
	await waitFor(() => frames.some(frame => frame.op === "connected"));
	const send = (op, data) => socket.send(JSON.stringify({ op, data }));
	const operations = () => frames.filter(frame => frame.op === "update_batch").flatMap(frame => frame.data.operations);
	send("connect", { protocol_version: version, route_path: path, query: "", hash: "" });
	return { socket, frames, send, operations };
}

test("native frames preserve batching, revision, readiness, and error barriers", { timeout: 30000 }, async () => {
	const server = spawn(process.argv[2], [], { stdio: ["ignore", "pipe", "pipe"] });
	let diagnostics = "";
	server.stdout.on("data", chunk => { diagnostics += chunk; });
	server.stderr.on("data", chunk => { diagnostics += chunk; });
	const clients = [];
	try {
		await waitFor(() => diagnostics.includes("Tatzel listening"));
		const page = await connect("/protocol");
		clients.push(page);
		await waitFor(() => page.frames.some(frame => frame.op === "page_ready"));
		const batches = page.frames.filter(frame => frame.op === "update_batch");
		assert.deepEqual(batches.map(frame => frame.data.sequence), batches.map((_, index) => index + 1));
		assert.ok(batches.every(frame => frame.data.operations.length <= 128 && Buffer.byteLength(JSON.stringify(frame)) <= 1024 * 1024));
		assert.equal(page.frames.at(-1).op, "page_ready");
		assert.equal(page.frames.at(-1).data.sequence, batches.at(-1).data.sequence);
		const parts = page.operations().filter(operation => operation.op === "create_element_part").flatMap(operation => operation.data);
		assert.equal(parts.filter(part => part.text_content.startsWith("Batch row ")).length, 300);
		assert.ok(batches.length < 15, "page construction substantially reduces transport frames");
		console.log(`Initial page: ${parts.length} parts, ${page.operations().length} operations, ${batches.length} batches`);
		const button = label => parts.find(part => part.tag === "button" && part.text_content === label).part_id;
		const input = parts.find(part => part.tag === "input").part_id;
		const hasText = text => page.operations().some(operation => operation.op === "set_text" && operation.data.some(part => part.text === text));
		page.send("on_input", { part_id: input, value: "first", epoch: 0, revision: 1 });
		await waitFor(() => hasText("Draft: first"));
		page.send("on_click", { part_id: button("Normalize later") });
		await waitFor(() => hasText("Normalizing"));
		page.send("on_input", { part_id: input, value: "newer", epoch: 0, revision: 2 });
		await waitFor(() => hasText("Stale result ignored"));
		assert.ok(!page.operations().some(operation => operation.op === "set_value"));
		page.send("on_click", { part_id: button("Reset draft") });
		await waitFor(() => page.operations().some(operation => operation.op === "set_value"));
		const reset = page.operations().findLast(operation => operation.op === "set_value").data[0];
		assert.equal(reset.epoch, 1);
		assert.equal(reset.replace, true);
		page.send("on_input", { part_id: input, value: "obsolete", epoch: 0, revision: 99 });
		page.send("on_input", { part_id: input, value: "current", epoch: 1, revision: 1 });
		await waitFor(() => hasText("Draft: current"));
		assert.ok(!hasText("Draft: obsolete"));
		page.send("on_click", { part_id: button("Update then fail") });
		await waitFor(() => page.frames.some(frame => frame.op === "error"));
		const errorIndex = page.frames.findIndex(frame => frame.op === "error");
		assert.equal(page.frames[errorIndex].data.fatal, false);
		assert.equal(page.frames[errorIndex - 1].data.operations.at(-1).data[0].text, "Changed before failure");
		page.send("on_click", { part_id: button("Recover") });
		await waitFor(() => hasText("Recovered"));
		page.send("on_input", { part_id: input, value: "missing counters" });
		await waitFor(() => page.frames.some(frame => frame.op === "error" && frame.data.code === "invalid_message"));
		const mismatch = await connect("/protocol", 1);
		clients.push(mismatch);
		await waitFor(() => mismatch.frames.some(frame => frame.op === "error"));
		assert.deepEqual(mismatch.frames.filter(frame => frame.op === "error").map(frame => [frame.data.code, frame.data.fatal]), [["protocol_mismatch", true]]);
		assert.equal(mismatch.operations().length, 0);
		const failed = await connect("/failed-page");
		clients.push(failed);
		await waitFor(() => failed.frames.some(frame => frame.op === "error"));
		assert.equal(failed.frames.at(-1).data.code, "page_failed");
		assert.equal(failed.frames.at(-1).data.fatal, true);
		assert.ok(failed.operations().some(operation => operation.op === "create_element_part"));
		assert.ok(!failed.frames.some(frame => frame.op === "page_ready"));
	}
	catch (error) { console.error(diagnostics); throw error; }
	finally {
		for (const client of clients) client.socket.close();
		const closed = once(server, "close");
		server.kill("SIGTERM");
		await Promise.race([closed, pause(1000, undefined, { ref: false })]);
		if (server.exitCode === null && server.signalCode === null) { server.kill("SIGKILL"); await closed; }
	}
});
