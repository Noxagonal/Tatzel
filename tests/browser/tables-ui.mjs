import assert from "node:assert/strict";
import { spawn } from "node:child_process";
import { once } from "node:events";
import { mkdir } from "node:fs/promises";
import { join, resolve } from "node:path";
import { setTimeout as pause } from "node:timers/promises";
import { chromium } from "playwright";

/** @brief Awaits observable table state. @param predicate State probe. @returns Truthy result. @pre Test is active. @post Completes or throws within five seconds. */
async function waitFor(predicate)
{
	for (let attempt = 0; attempt < 250; ++attempt) {
		const value = await predicate();
		if (value) return value;
		await pause(20);
	}
	throw new Error("Timed out waiting for table state");
}

const server = spawn(resolve(process.argv[2]), [], { stdio: ["ignore", "pipe", "pipe"] });
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
	const context = await browser.newContext({ viewport: { width: 1440, height: 1000 } });
	const page = await context.newPage();
	page.setDefaultTimeout(5000);
	const errors = [];
	const frames = [];
	let route;
	page.on("pageerror", error => errors.push(error.message));
	await page.routeWebSocket("**/ws", socket => {
		route = socket;
		const peer = socket.connectToServer();
		peer.onMessage(message => { frames.push(JSON.parse(String(message))); socket.send(message); });
	});
	await page.goto("http://127.0.0.1:8080/");
	await page.getByRole("link", { name: "Logical tables and stable rows", exact: true }).click();
	await page.waitForFunction(() => document.documentElement.dataset.tatzelConnection === "ready");
	const table = page.getByRole("table", { name: "Editable records", exact: true });
	const rows = table.locator("tbody tr[data-ui-row-key]");
	await waitFor(async () => await rows.count() === 3);
	const first = table.locator('tr[data-ui-row-key="r1"]');
	const second = table.locator('tr[data-ui-row-key="r2"]');
	const input = first.getByLabel("Draft Alpha", { exact: true });
	await input.fill("Typing through row moves");
	await input.evaluate(element => element.setSelectionRange(7, 7));
	assert.equal(await input.evaluate(element => element.selectionStart), 7);
	const inputID = await input.getAttribute("id");
	const firstID = await first.getAttribute("id");
	const beforeSort = frames.length;
	await table.getByRole("button", { name: "Count", exact: true }).evaluate(element => element.click());
	await waitFor(async () => await rows.first().getAttribute("data-ui-row-key") === "r2");
	assert.equal(await input.getAttribute("id"), inputID);
	assert.equal(await first.getAttribute("id"), firstID);
	assert.equal(await input.inputValue(), "Typing through row moves");
	assert.equal(await input.evaluate(element => document.activeElement === element), true);
	assert.equal(await input.evaluate(element => element.selectionStart), 7);
	const sortOperations = frames.slice(beforeSort).filter(frame => frame.op === "update_batch").flatMap(frame => frame.data.operations);
	assert.ok(sortOperations.some(operation => operation.op === "set_child_order"));
	assert.ok(!sortOperations.some(operation => operation.op === "create_element_part" || operation.op === "remove_element_part"));
	assert.equal(await table.locator('th[data-ui-column="count"]').getAttribute("aria-sort"), "ascending");
	await table.getByRole("button", { name: "Count", exact: true }).evaluate(element => element.click());
	await waitFor(async () => await rows.first().getAttribute("data-ui-row-key") === "r1");
	await second.getByRole("button", { name: "Increment", exact: true }).evaluate(element => element.click());
	await waitFor(async () => await second.locator('td[data-ui-column="count"]').textContent() === "2");
	assert.equal(await input.inputValue(), "Typing through row moves");
	assert.equal(await input.evaluate(element => document.activeElement === element), true);
	await page.getByRole("button", { name: "Toggle loading", exact: true }).evaluate(element => element.click());
	await waitFor(async () => await table.getAttribute("aria-busy") === "true");
	assert.equal(await rows.count(), 3);
	assert.equal(await input.getAttribute("id"), inputID);
	assert.equal(await input.evaluate(element => document.activeElement === element), true);
	await page.getByRole("button", { name: "Toggle loading", exact: true }).evaluate(element => element.click());
	await waitFor(async () => await table.getAttribute("aria-busy") === "false");
	await first.getByRole("checkbox", { name: "Select Alpha", exact: true }).check();
	await second.getByRole("checkbox", { name: "Select Bravo", exact: true }).check();
	await page.getByText("2 rows selected", { exact: true }).waitFor();
	const removedField = await second.getByRole("checkbox").getAttribute("id");
	await second.getByRole("button", { name: "Delete", exact: true }).click();
	await waitFor(async () => await rows.count() === 2);
	await page.getByText("1 row selected", { exact: true }).waitFor();
	await page.evaluate(async part_id => {
		const { socket } = await import("/app/websocket.js");
		socket.send(JSON.stringify({ op: "on_change", data: { part_id, value: false, epoch: 0, revision: 100 } }));
	}, removedField);
	assert.equal(await first.getByRole("checkbox").isChecked(), true);
	const previews = resolve("build/table-preview");
	await mkdir(previews, { recursive: true });
	await page.screenshot({ path: join(previews, "table-desktop.png"), fullPage: true });
	await page.setViewportSize({ width: 390, height: 844 });
	assert.ok(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth + 1), "the table scrolls inside its wrapper on mobile");
	await page.screenshot({ path: join(previews, "table-mobile.png"), fullPage: true });
	await page.getByRole("button", { name: "Clear rows", exact: true }).click();
	await waitFor(async () => await rows.count() === 0);
	await table.getByText("No records. Add a row to begin.", { exact: true }).waitFor();
	assert.equal(await table.locator('td[colspan="5"]').count(), 1);
	assert.equal(await table.getByRole("columnheader").count(), 5);
	await page.getByRole("button", { name: "Toggle loading", exact: true }).click();
	await table.getByText("Loading…", { exact: true }).waitFor();
	await page.getByRole("button", { name: "Toggle loading", exact: true }).click();
	await page.getByRole("button", { name: "Add row", exact: true }).click();
	await waitFor(async () => await rows.count() === 1);
	await table.getByLabel("Draft Record 4", { exact: true }).fill("New identity after clearing");
	assert.deepEqual(errors, []);
	const body = await table.locator("tbody").getAttribute("id");
	const onlyRow = await rows.first().getAttribute("id");
	const sequence = frames.filter(frame => frame.op === "update_batch").at(-1).data.sequence;
	route.send(JSON.stringify({ op: "update_batch", data: { sequence: sequence + 1, operations: [
		{ op: "set_child_order", data: [{ parent_id: body, child_ids: [onlyRow] }] }
	] } }));
	await page.getByRole("alert").filter({ hasText: "out of sync" }).waitFor();
	assert.equal(await rows.count(), 1, "invalid complete-membership order does not remove a row");
	await page.getByRole("button", { name: "Reload page", exact: true }).click();
	await page.waitForFunction(() => document.documentElement.dataset.tatzelConnection === "ready");
	await waitFor(async () => await page.locator("tbody tr[data-ui-row-key]").count() === 3);
	console.log("Table UI passed: semantic headers, stable rows/control drafts/caret, sort and selection, incremental updates, removal, loading/empty states, mobile overflow, and invalid-order recovery");
}
catch (error) { console.error(diagnostics); throw error; }
finally {
	if (browser) await browser.close();
	const closed = once(server, "close");
	server.kill("SIGTERM");
	await Promise.race([closed, pause(1000, undefined, { ref: false })]);
	if (server.exitCode === null && server.signalCode === null) { server.kill("SIGKILL"); await closed; }
}
