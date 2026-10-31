import assert from "node:assert/strict";
import { spawn } from "node:child_process";
import { once } from "node:events";
import { setTimeout as pause } from "node:timers/promises";
import { test } from "node:test";
import { request } from "node:http";

/**
 * @brief Waits for an observable server state with a bounded timeout.
 * @param predicate Function returning the awaited value when ready.
 * @returns The first truthy predicate result.
 * @pre The server is starting or processing an asynchronous operation.
 * @post The requested state is observed or an error is thrown.
 * @throws Error If the condition is not met within five seconds.
 */
async function waitFor(predicate)
{
	const deadline = Date.now() + 5000;
	while (Date.now() < deadline) {
		const result = predicate();
		if (result) return result;
		await pause(20);
	}
	throw new Error("Timed out waiting for server state");
}

/**
 * @brief Checks the HTTP upgrade response independently of the WebSocket client.
 * @returns The upgrade response status code.
 * @pre The example server is listening on port 8080.
 * @post The diagnostic request and any upgraded socket have been closed.
 */
async function checkUpgrade()
{
	return new Promise((resolve, reject) => {
		const upgrade = request("http://127.0.0.1:8080/ws", { headers: {
			Connection: "Upgrade", Upgrade: "websocket",
			"Sec-WebSocket-Version": "13", "Sec-WebSocket-Key": "dGhlIHNhbXBsZSBub25jZQ=="
		} });
		upgrade.on("response", response => {
			console.log("WebSocket HTTP response:", response.statusCode, response.headers);
			response.resume();
			resolve(response.statusCode);
		});
		upgrade.on("upgrade", (response, socket) => {
			console.log("WebSocket upgrade:", response.statusCode, response.headers);
			socket.destroy();
			resolve(response.statusCode);
		});
		upgrade.on("error", reject);
		upgrade.setTimeout(3000, () => upgrade.destroy(new Error("Upgrade request timed out")));
		upgrade.end();
	});
}

/**
 * @brief Opens a real browser transport and builds a fresh logical page.
 * @returns The transport, received message list, and concrete heading/button IDs.
 * @pre The example server is listening on port 8080.
 * @post One logical page is initialized and its button has an event marker.
 */
async function connectPage()
{
	const socket = new WebSocket("ws://127.0.0.1:8080/ws");
	const messages = [];
	socket.addEventListener("message", event => messages.push(JSON.parse(event.data)));
	await new Promise((resolve, reject) => {
		const timeout = setTimeout(() => reject(new Error("WebSocket connection timed out")), 5000);
		socket.addEventListener("open", () => { clearTimeout(timeout); resolve(); }, { once: true });
		socket.addEventListener("error", event => {
			clearTimeout(timeout);
			reject(new Error("WebSocket connection failed: " + (event.message ?? "unknown error")));
		}, { once: true });
	});
	socket.send(JSON.stringify({ op: "connect", data: { route_path: "/", query: "", hash: "" } }));
	await waitFor(() => messages.some(message => message.op === "set_on_click"));
	const parts = messages.filter(message => message.op === "create_element_part").flatMap(message => message.data);
	return { socket, messages,
		heading: parts.find(part => part.tag === "h1").part_id,
		button: parts.find(part => part.tag === "button").part_id };
}

test("the running example maps logical updates and events across real WebSockets", { timeout: 30000 }, async () => {
	const server = spawn(process.argv[2], [], { stdio: ["ignore", "pipe", "pipe"] });
	let diagnostics = "";
	server.stdout.on("data", chunk => { diagnostics += chunk; });
	server.stderr.on("data", chunk => { diagnostics += chunk; });
	let first;
	let second;
	try {
		// Drogon upgrades only the first request on a TCP connection. Close the
		// HTTP probes so Node's shared HTTP/WebSocket pool opens a fresh socket.
		const deadline = Date.now() + 5000;
		let response;
		while (Date.now() < deadline) {
			try { response = await fetch("http://127.0.0.1:8080/", { signal: AbortSignal.timeout(1000), headers: { Connection: "close" } }); if (response.ok) break; }
			catch {}
			await pause(50);
		}
		assert.ok(response?.ok, diagnostics);
		assert.match(await response.text(), /id="root\|main"/);
		const script = await fetch("http://127.0.0.1:8080/app/main.js", { headers: { Connection: "close" } });
		assert.equal(script.status, 200);
		assert.match(await script.text(), /registerBrowserEventHandlers/);

		console.log("HTTP template and browser asset served");
		assert.equal(await checkUpgrade(), 101);
		first = await connectPage();
		console.log("First logical page connected");
		second = await connectPage();
		console.log("Second logical page connected");
		assert.notEqual(first.heading, second.heading);

		first.socket.send(JSON.stringify({ op: "on_click", data: { part_id: first.button } }));
		await waitFor(() => first.messages.find(message => message.op === "set_text"
			&& message.data.some(part => part.part_id === first.heading && part.text === "Clicked times: 1")));
		assert.equal(second.messages.filter(message => message.op === "set_text").length, 0);

		console.log("Logical click and text update completed");
		first.socket.send("{");
		await waitFor(() => first.messages.find(message => message.op === "error" && message.data.code === "invalid_json"));
		first.socket.send(JSON.stringify({ op: "connect", data: { route_path: "/" } }));
		await waitFor(() => first.messages.find(message => message.op === "error" && message.data.code === "already_connected"));
	}
	catch (error) {
		console.error(diagnostics);
		throw error;
	}
	finally {
		first?.socket.close();
		second?.socket.close();
		if (server.exitCode === null && server.signalCode === null) {
			const exited = once(server, "exit");
			server.kill("SIGTERM");
			await Promise.race([exited, pause(1000, undefined, { ref: false })]);
			if (server.exitCode === null && server.signalCode === null) {
				server.kill("SIGKILL");
				await exited;
			}
		}
	}
});
