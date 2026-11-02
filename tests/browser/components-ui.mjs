import assert from "node:assert/strict";
import { spawn } from "node:child_process";
import { once } from "node:events";
import { mkdir } from "node:fs/promises";
import { resolve, join } from "node:path";
import { setTimeout as pause } from "node:timers/promises";
import { chromium } from "playwright";

/** @brief Awaits a component's observable state. @param predicate State probe. @returns Truthy result. @pre The test is active. @post Completes or throws within five seconds. */
async function waitFor(predicate)
{
	for (let attempt = 0; attempt < 250; ++attempt) {
		const value = await predicate();
		if (value) return value;
		await pause(20);
	}
	throw new Error("Timed out waiting for component state");
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
	const context = await browser.newContext({ viewport: { width: 1440, height: 1100 } });
	const page = await context.newPage();
	page.setDefaultTimeout(5000);
	const errors = [];
	page.on("pageerror", error => errors.push(error.message));
	await page.goto("http://127.0.0.1:8080/");
	await page.getByRole("link", { name: "Reusable components and themes", exact: true }).click();
	await page.waitForFunction(() => document.documentElement.dataset.tatzelConnection === "ready");
	assert.equal(await page.locator('[data-ui-component]').count(), 5);
	await page.getByLabel("Search first list", { exact: true }).fill("draft filter");
	await page.getByText("List 1: draft filter | all", { exact: true }).waitFor();
	assert.equal(await page.getByLabel("Search second list", { exact: true }).inputValue(), "");
	await page.getByLabel("First status", { exact: true }).selectOption("done");
	await page.getByText("List 1: draft filter | done", { exact: true }).waitFor();
	await page.getByRole("button", { name: "Reset filters", exact: true }).first().click();
	await page.getByText("List 1:  | all", { exact: true }).waitFor();
	assert.equal(await page.getByLabel("Search first list", { exact: true }).inputValue(), "");
	const opener = page.getByRole("button", { name: "Ask for confirmation", exact: true });
	await opener.click();
	const dialog = page.getByRole("dialog", { name: "Remove this example?", exact: true });
	await dialog.waitFor();
	await page.keyboard.press("Escape");
	await page.getByText("Cancelled once", { exact: true }).waitFor();
	assert.equal(await opener.evaluate(element => document.activeElement === element), true);
	await opener.click();
	await dialog.getByRole("button", { name: "Confirm", exact: true }).click();
	await page.getByText("Confirmed once", { exact: true }).waitFor();
	assert.equal(await dialog.isVisible(), false);
	const editors = page.locator('[data-ui-component="note-editor"]');
	const first = editors.nth(0);
	const second = editors.nth(1);
	await first.getByLabel("Title", { exact: true }).fill("");
	await first.getByRole("button", { name: "Save note", exact: true }).click();
	await first.getByText("This field is required.", { exact: true }).waitFor();
	assert.equal(await second.getByLabel("Title", { exact: true }).inputValue(), "A note");
	await first.getByLabel("Title", { exact: true }).fill("fail");
	await first.getByRole("button", { name: "Save note", exact: true }).click();
	await first.getByText("Example save failed. Your draft is preserved.", { exact: true }).waitFor();
	assert.equal(await first.getByLabel("Title", { exact: true }).inputValue(), "fail");
	await first.getByLabel("Title", { exact: true }).fill("Saved title");
	await first.getByRole("button", { name: "Save note", exact: true }).click();
	await first.getByText("Saved record-a", { exact: true }).waitFor();
	await first.getByRole("button", { name: "Save note", exact: true }).click();
	await page.getByRole("button", { name: "Load another record in First editor", exact: true }).click();
	await first.getByText("Loaded replacement", { exact: true }).waitFor();
	await pause(400);
	assert.equal(await first.getByLabel("Title", { exact: true }).inputValue(), "Loaded title");
	assert.equal(await first.getByText("Loaded replacement", { exact: true }).count(), 1);
	assert.equal(await second.getByLabel("Title", { exact: true }).inputValue(), "A note");
	const previews = resolve("build/components-preview");
	await mkdir(previews, { recursive: true });
	await page.screenshot({ path: join(previews, "components-desktop.png"), fullPage: true });
	const oldColor = await first.getByRole("button", { name: "Save note", exact: true }).evaluate(element => getComputedStyle(element).backgroundColor);
	await page.getByRole("button", { name: "Switch editor theme", exact: true }).click();
	await waitFor(async () => await first.getByRole("button", { name: "Save note", exact: true }).evaluate(element => getComputedStyle(element).backgroundColor) !== oldColor);
	assert.equal(await first.getByLabel("Title", { exact: true }).inputValue(), "Loaded title");
	await page.screenshot({ path: join(previews, "components-theme.png"), fullPage: true });
	await page.setViewportSize({ width: 390, height: 844 });
	assert.ok(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth + 1), "components fit the mobile viewport");
	await page.screenshot({ path: join(previews, "components-mobile.png"), fullPage: true });
	await second.getByRole("button", { name: "Save note", exact: true }).click();
	await page.getByRole("button", { name: "Remove Second editor", exact: true }).click();
	await waitFor(async () => await editors.count() === 1);
	await pause(400);
	assert.equal(await page.locator("html").getAttribute("data-tatzel-connection"), "ready");
	assert.deepEqual(errors, []);
	console.log("Component UI passed: independent instances, coherent filters/reset, confirmation outcomes/focus, validation, async success/failure/stale results, removal, themes, and mobile layout");
}
catch (error) { console.error(diagnostics); throw error; }
finally {
	if (browser) await browser.close();
	const closed = once(server, "close");
	server.kill("SIGTERM");
	await Promise.race([closed, pause(1000, undefined, { ref: false })]);
	if (server.exitCode === null && server.signalCode === null) { server.kill("SIGKILL"); await closed; }
}
