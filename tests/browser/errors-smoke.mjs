import assert from "node:assert/strict";
import { spawn } from "node:child_process";
import { once } from "node:events";
import { copyFile, mkdir, mkdtemp, readFile, rm, symlink, unlink, writeFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join, resolve } from "node:path";
import { setTimeout as pause } from "node:timers/promises";
import { test } from "node:test";

/**
 * @brief Waits for a complete server diagnostic with a bounded timeout.
 * @param predicate Synchronous state probe.
 * @returns The first truthy probe value.
 * @pre The example process is processing requests.
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
	throw new Error("Timed out waiting for an exception diagnostic");
}

test("HTTP failures report Tatzel and genuine STL exceptions without stopping the server", { timeout: 30000 }, async () => {
	const fixture = await mkdtemp(join(tmpdir(), "tatzel-error-check-"));
	await mkdir(join(fixture, "data"));
	await mkdir(join(fixture, "www"));
	await copyFile("data/drogon_config.json", join(fixture, "data/drogon_config.json"));
	const template = await readFile("data/template.html", "utf8");
	const templatePath = join(fixture, "data/template.html");
	const server = spawn(resolve(process.argv[2]), [], { cwd: fixture, stdio: ["ignore", "pipe", "pipe"] });
	let diagnostics = "";
	server.stdout.on("data", chunk => { diagnostics += chunk; });
	server.stderr.on("data", chunk => { diagnostics += chunk; });
	const request = () => fetch("http://127.0.0.1:8080/", { headers: { Connection: "close" }, signal: AbortSignal.timeout(5000) });
	try {
		await waitFor(() => diagnostics.includes("Tatzel listening"));
		let start = diagnostics.length;
		let response = await request();
		assert.equal(response.status, 500);
		assert.equal(await response.text(), "Internal server error");
		await waitFor(() => diagnostics.slice(start).includes(" - App.cpp:"));
		const ownedFailure = diagnostics.slice(start);
		assert.match(ownedFailure, /Exception: tatzel::Exception/);
		assert.match(ownedFailure, /Message: Template file does not exist/);
		assert.match(ownedFailure, /Thrown at: .*Utility\.cpp:\d+/);

		// A symlink loop makes std::filesystem itself throw, without an explicit test throw.
		await symlink("template.html", templatePath);
		start = diagnostics.length;
		response = await request();
		assert.equal(response.status, 500);
		assert.equal(await response.text(), "Internal server error");
		await waitFor(() => diagnostics.slice(start).includes(" - App.cpp:"));
		const stlFailure = diagnostics.slice(start);
		assert.match(stlFailure, /Exception: std::filesystem::(?:__cxx11::)?filesystem_error/);
		assert.ok(stlFailure.includes("Message:") && stlFailure.includes("Operation: http_request"));
		assert.ok(!stlFailure.includes("Thrown at:") && !stlFailure.includes("Stack trace:"));

		await unlink(templatePath);
		await writeFile(templatePath, template);
		response = await request();
		assert.equal(response.status, 200);
		assert.match(await response.text(), /id="root\|main"/);
	}
	catch (error) { console.error(diagnostics); throw error; }
	finally {
		const closed = once(server, "close");
		server.kill("SIGTERM");
		await Promise.race([closed, pause(1000, undefined, { ref: false })]);
		if (server.exitCode === null && server.signalCode === null) { server.kill("SIGKILL"); await closed; }
		await rm(fixture, { recursive: true, force: true });
	}
});
