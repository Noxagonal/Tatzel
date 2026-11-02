import assert from "node:assert/strict";
import { spawn } from "node:child_process";
import { once } from "node:events";
import { setTimeout as pause } from "node:timers/promises";
import { chromium } from "playwright";

/** @brief Waits for an observed browser/transport condition. @param predicate State probe. @returns Truthy result. @pre Test is active. @post Completes or throws within five seconds. */
async function waitFor(predicate)
{
	for (let attempt = 0; attempt < 250; ++attempt) {
		const result = await predicate();
		if (result) return result;
		await pause(20);
	}
	throw new Error("Timed out waiting for browser protocol state");
}

const server = spawn(process.argv[2], [], { stdio: ["ignore", "pipe", "pipe"] });
let diagnostics = "";
server.stdout.on("data", chunk => { diagnostics += chunk; });
server.stderr.on("data", chunk => { diagnostics += chunk; });
let browser;
try {
	await waitFor(() => diagnostics.includes("Tatzel listening"));
	browser = await chromium.launch({ headless: true, ...(process.env.TATZEL_TEST_CHROMIUM ? {
		executablePath: process.env.TATZEL_TEST_CHROMIUM,
		args: ["--single-process", "--no-zygote", "--disable-dev-shm-usage"]
	} : {}) });
	const context = await browser.newContext();
	const page = await context.newPage();
	page.setDefaultTimeout(5000);
	const errors = [];
	page.on("pageerror", error => errors.push(error.message));
	let route;
	let hold = true;
	let readyFrame;
	let buffered = [];
	let holdUpdates = false;
	await page.routeWebSocket("**/ws", socket => {
		route = socket;
		const peer = socket.connectToServer();
		peer.onMessage(message => {
			const frame = JSON.parse(String(message));
			if (hold && frame.op === "page_ready") readyFrame = message;
			else if (holdUpdates) buffered.push(message);
			else socket.send(message);
		});
	});
	await page.goto("http://127.0.0.1:8080/protocol");
	await waitFor(() => readyFrame);
	assert.equal(await page.locator("html").getAttribute("data-tatzel-connection"), "initializing");
	assert.equal(await page.locator('[id="root|main"]').evaluate(element => element.inert), true);
	hold = false;
	route.send(readyFrame);
	await page.waitForFunction(() => document.documentElement.dataset.tatzelConnection === "ready");
	const input = page.getByLabel("Draft", { exact: true });
	await input.fill("first");
	holdUpdates = true;
	await page.getByRole("button", { name: "Normalize later", exact: true }).click();
	await waitFor(() => buffered.some(message => JSON.parse(String(message)).data?.operations?.some(operation => operation.op === "set_value")));
	await input.fill("newest");
	await input.evaluate(element => element.setSelectionRange(2, 2));
	const flush = () => { holdUpdates = false; for (const message of buffered) route.send(message); buffered = []; };
	flush();
	await page.getByText("Draft: newest", { exact: true }).waitFor();
	assert.equal(await input.inputValue(), "newest", "a correction already on the wire cannot replace a newer draft");
	assert.equal(await input.evaluate(element => element.selectionStart), 2);
	await page.getByRole("button", { name: "Reset draft", exact: true }).click();
	await waitFor(async () => await input.inputValue() === "initial");
	await input.focus();
	await input.evaluate(element => {
		element.dispatchEvent(new CompositionEvent("compositionstart", { bubbles: true }));
		element.value = "composing draft";
		element.dispatchEvent(new InputEvent("input", { bubbles: true, isComposing: true }));
	});
	await page.getByRole("button", { name: "Reset draft", exact: true }).click();
	await waitFor(async () => await input.inputValue() === "initial");
	await input.evaluate(element => {
		element.value = "late composition";
		element.dispatchEvent(new CompositionEvent("compositionend", { bubbles: true }));
	});
	assert.equal(await input.inputValue(), "initial");
	await page.getByRole("button", { name: "Update then fail", exact: true }).click();
	await page.getByRole("alert").filter({ hasText: "A page operation failed" }).waitFor();
	assert.equal(await page.locator('[id="root|main"]').evaluate(element => element.inert), false);
	await page.getByRole("button", { name: "Dismiss", exact: true }).click();
	await page.getByRole("button", { name: "Recover", exact: true }).click();
	await page.getByText("Recovered", { exact: true }).waitFor();
	await page.evaluate(() => window.dispatchEvent(new Event("offline")));
	await page.getByRole("button", { name: "Reload page", exact: true }).waitFor();
	assert.equal(await page.locator("html").getAttribute("data-tatzel-connection"), "disconnected");
	assert.equal(await page.locator('[id="root|main"]').evaluate(element => element.inert), true);

	const failed = await context.newPage();
	await failed.goto("http://127.0.0.1:8080/failed-page");
	await failed.getByRole("alert").filter({ hasText: "The page could not be prepared" }).waitFor();
	assert.equal(await failed.locator("html").getAttribute("data-tatzel-connection"), "failed");
	await failed.getByRole("button", { name: "Reload page", exact: true }).click();
	await failed.getByRole("alert").filter({ hasText: "The page could not be prepared" }).waitFor();

	const choices = await context.newPage();
	let choicesRoute;
	let delayedChoices = [];
	let delayChoices = false;
	const choiceEdits = [];
	await choices.routeWebSocket("**/ws", socket => {
		choicesRoute = socket;
		const peer = socket.connectToServer();
		socket.onMessage(message => {
			const event = JSON.parse(String(message));
			if (event.op === "on_change") choiceEdits.push(event);
			peer.send(message);
		});
		peer.onMessage(message => {
			if (delayChoices) delayedChoices.push(message);
			else socket.send(message);
		});
	});
	await choices.goto("http://127.0.0.1:8080/forms");
	await choices.waitForFunction(() => document.documentElement.dataset.tatzelConnection === "ready");
	delayChoices = true;
	await choices.getByRole("button", { name: "Refresh choices", exact: true }).click();
	await waitFor(() => delayedChoices.some(message => JSON.parse(String(message)).data?.operations?.some(operation => operation.op === "set_select_options")));
	await choices.getByLabel("Delivery", { exact: true }).focus();
	await choices.getByLabel("Delivery", { exact: true }).selectOption("phone");
	delayChoices = false;
	for (const message of delayedChoices) choicesRoute.send(message);
	await waitFor(() => choiceEdits.some(event => event.data.epoch === 1 && event.data.revision === 1));
	assert.equal(await choices.getByLabel("Delivery", { exact: true }).inputValue(), "phone");
	assert.equal(await choices.getByLabel("Delivery", { exact: true }).evaluate(element => document.activeElement === element), true);

	// A malformed later command must prevent even the first command from applying.
	const malformed = await context.newPage();
	await malformed.routeWebSocket("**/ws", socket => socket.onMessage(() => {
		socket.send(JSON.stringify({ op: "connected", data: { message: "Connected", protocol_version: 2 } }));
		socket.send(JSON.stringify({ op: "update_batch", data: { sequence: 1, operations: [
			{ op: "create_element_part", data: [{ part_id: "must-not-exist", parent_id: "root|main", tag: "p", text_content: "invalid partial update", attributes: {}, classes: [] }] },
			{ op: "unknown", data: [] }
		] } }));
	}));
	await malformed.goto("http://127.0.0.1:8080/protocol");
	await malformed.getByRole("alert").filter({ hasText: "out of sync" }).waitFor();
	assert.equal(await malformed.locator('[id="must-not-exist"]').count(), 0);
	for (const [sequence, operations] of [
		[2, [{ op: "set_text", data: [] }]],
		[1, [{ op: "set_text", data: [{ part_id: "missing-target", text: "bad" }] }]]
	]) {
		const invalid = await context.newPage();
		await invalid.routeWebSocket("**/ws", socket => socket.onMessage(() => {
			socket.send(JSON.stringify({ op: "connected", data: { message: "Connected", protocol_version: 2 } }));
			socket.send(JSON.stringify({ op: "update_batch", data: { sequence, operations } }));
		}));
		await invalid.goto("http://127.0.0.1:8080/protocol");
		await invalid.getByRole("alert").filter({ hasText: "out of sync" }).waitFor();
		assert.equal(await invalid.locator('[id="root|main"]').evaluate(element => element.inert), true);
	}
	const timedOut = await context.newPage();
	await timedOut.clock.install();
	await timedOut.routeWebSocket("**/ws", socket => socket.onMessage(() => {
		socket.send(JSON.stringify({ op: "connected", data: { message: "Connected", protocol_version: 2 } }));
	}));
	await timedOut.goto("http://127.0.0.1:8080/protocol");
	await timedOut.waitForFunction(() => document.documentElement.dataset.tatzelConnection === "initializing");
	await timedOut.clock.fastForward(15001);
	await timedOut.getByRole("alert").filter({ hasText: "did not become ready" }).waitFor();
	assert.deepEqual(errors, []);
	console.log("Browser reliability passed: readiness, in-flight edits/caret, composition reset, recoverable errors, offline recovery, failed factory, and full-batch validation");
}
catch (error) { console.error(diagnostics); throw error; }
finally {
	if (browser) await browser.close();
	const closed = once(server, "close");
	server.kill("SIGTERM");
	await Promise.race([closed, pause(1000, undefined, { ref: false })]);
	if (server.exitCode === null && server.signalCode === null) { server.kill("SIGKILL"); await closed; }
}
