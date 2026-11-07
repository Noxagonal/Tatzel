import assert from "node:assert/strict";
import { spawn } from "node:child_process";
import { once } from "node:events";
import { cp, mkdir, readFile, writeFile } from "node:fs/promises";
import { dirname, join, resolve } from "node:path";
import { setTimeout as pause } from "node:timers/promises";
import { ProtocolSocket } from "./protocol-client.mjs";

/** @brief Waits for a complete observable protocol state. @param predicate State probe. @returns First truthy result. @pre The server is processing requests. @post The state is observed or a bounded error is thrown. */
async function waitFor(predicate)
{
	for (let attempt = 0; attempt < 250; ++attempt) {
		const result = predicate();
		if (result) return result;
		await pause(20);
	}
	throw new Error("Timed out waiting for installed sample state");
}

/** @brief Exercises one independently launched installed consumer. @param binary Moved executable path. @param cwd Empty unrelated working directory. @param args Optional explicit runtime argument. @param environment Runtime override environment. @param title Expected template title. @pre No other server owns port 8080. @post HTTP assets and logical events have passed; the process and transport are closed. */
async function exercise(binary, cwd, args, environment, title)
{
	const server = spawn(binary, args, { cwd, env: environment, stdio: ["ignore", "pipe", "pipe"] });
	let diagnostics = "";
	let socket;
	server.stdout.on("data", chunk => { diagnostics += chunk; });
	server.stderr.on("data", chunk => { diagnostics += chunk; });
	try {
		await waitFor(() => diagnostics.includes("Tatzel listening"));
		const response = await fetch("http://127.0.0.1:8080/", { headers: { Connection: "close" } });
		assert.equal(response.status, 200);
		assert.ok((await response.text()).includes(title));
		const scripts = await fetch("http://127.0.0.1:8080/app/main.js", { headers: { Connection: "close" } });
		assert.equal(scripts.status, 200);
		assert.match(await scripts.text(), /registerBrowserEventHandlers/);
		for (const path of ["/framework/layout.css", "/framework/components.css", "/framework/connection.css", "/app/websocket.js", "/app/dom-ops.js"]) {
			const asset = await fetch("http://127.0.0.1:8080" + path, { headers: { Connection: "close" } });
			assert.equal(asset.status, 200, path);
			assert.ok((await asset.text()).length > 0);
		}
		socket = new ProtocolSocket("ws://127.0.0.1:8080/ws");
		const messages = [];
		socket.addEventListener("message", event => messages.push(JSON.parse(event.data)));
		await waitFor(() => socket.readyState === WebSocket.OPEN);
		socket.send(JSON.stringify({ op: "connect", data: { route_path: "/", query: "", hash: "" } }));
		await waitFor(() => messages.some(message => message.op === "page_ready"));
		const parts = messages.filter(message => message.op === "create_element_part").flatMap(message => message.data);
		assert.ok(parts.some(part => part.tag === "h1" && part.text_content === "Tatzel package sample"));
		const button = label => parts.find(part => part.tag === "button" && part.text_content === label).part_id;
		const send = (op, data) => socket.send(JSON.stringify({ op, data }));
		const text = expected => messages.some(message => message.op === "set_text" && message.data.some(part => part.text === expected));
		send("on_click", { part_id: button("Increment") });
		await waitFor(() => text("Count: 1"));
		const input = parts.find(part => part.tag === "input").part_id;
		send("on_input", { part_id: input, value: "Ada" });
		send("on_click", { part_id: button("Greet") });
		await waitFor(() => text("Hello, Ada!"));
		send("on_click", { part_id: button("Run background task") });
		await waitFor(() => text("Background task complete."));
		assert.ok(!messages.some(message => message.op === "error"));
	}
	catch (error) { console.error(diagnostics); throw error; }
	finally {
		socket?.close();
		if (server.exitCode === null && server.signalCode === null) {
			const closed = once(server, "close");
			server.kill("SIGTERM");
			await Promise.race([closed, pause(1000, undefined, { ref: false })]);
			if (server.exitCode === null && server.signalCode === null) { server.kill("SIGKILL"); await closed; }
		}
	}
}

const binary = resolve(process.argv[2]);
const cwd = resolve(process.argv[3]);
const environment = { ...process.env };
delete environment.TATZEL_RESOURCE_DIRECTORY;
await exercise(binary, cwd, [], environment, "<title>Tatzel</title>");
const override = join(dirname(binary), "override runtime");
await mkdir(override);
await cp(join(dirname(binary), "tatzel-runtime"), override, { recursive: true });
const template = join(override, "data/template.html");
await writeFile(template, (await readFile(template, "utf8")).replace("<title>Tatzel</title>", "<title>Runtime override</title>"));
await exercise(binary, cwd, [], { ...environment, TATZEL_RESOURCE_DIRECTORY: override }, "<title>Runtime override</title>");
await exercise(binary, cwd, [override], { ...environment, TATZEL_RESOURCE_DIRECTORY: join(cwd, "missing runtime") }, "<title>Runtime override</title>");
const invalid = spawn(binary, [join(cwd, "missing explicit runtime")], {
	cwd, env: { ...environment, TATZEL_RESOURCE_DIRECTORY: override }, stdio: ["ignore", "pipe", "pipe"]
});
let invalidDiagnostics = "";
invalid.stdout.on("data", chunk => { invalidDiagnostics += chunk; });
invalid.stderr.on("data", chunk => { invalidDiagnostics += chunk; });
const invalidClosed = once(invalid, "close");
try {
	await waitFor(() => invalid.exitCode !== null);
	await invalidClosed;
	assert.equal(invalid.exitCode, 1);
	assert.match(invalidDiagnostics, /Tatzel runtime directory does not exist/);
} finally {
	if (invalid.exitCode === null && invalid.signalCode === null) { invalid.kill("SIGTERM"); await invalidClosed; }
}
console.log("Installed consumer passed: relocated executable, empty cwd, deployed HTTP assets, native UI actions, environment override, and explicit AppOptions precedence");
