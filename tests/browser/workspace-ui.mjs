import assert from "node:assert/strict";
import { spawn } from "node:child_process";
import { once } from "node:events";
import { mkdir, mkdtemp, rm } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join, resolve } from "node:path";
import { setTimeout as pause } from "node:timers/promises";
import { chromium } from "playwright";

const directory = await mkdtemp(join(tmpdir(), "tatzel-workspace-ui-"));
const previews = resolve("build/workspace-preview");
await mkdir(previews, { recursive: true });
const server = spawn(resolve(process.argv[2]), [], {
	env: { ...process.env, TATZEL_WORKSPACE_DB: join(directory, "workspace.sqlite3") },
	stdio: ["ignore", "pipe", "pipe"]
});
let diagnostics = "";
server.stdout.on("data", chunk => { diagnostics += chunk; });
server.stderr.on("data", chunk => { diagnostics += chunk; });
let browser;
let page;
try {
	let ready = false;
	for (let attempt = 0; attempt < 100; ++attempt) {
		try {
			const response = await fetch("http://127.0.0.1:8080/", { headers: { Connection: "close" }, signal: AbortSignal.timeout(300) });
			if (response.ok) { ready = true; break; }
		} catch {}
		await pause(30);
	}
	assert.ok(ready, diagnostics);
	browser = await chromium.launch({
		headless: true,
		...(process.env.TATZEL_TEST_CHROMIUM ? {
			executablePath: process.env.TATZEL_TEST_CHROMIUM,
			args: ["--single-process", "--no-zygote", "--disable-dev-shm-usage"]
		} : {})
	});
	const context = await browser.newContext({ viewport: { width: 1440, height: 1000 } });
	page = await context.newPage();
	const peer = await context.newPage();
	const errors = [];
	page.on("pageerror", error => errors.push(error.message));
	peer.on("pageerror", error => errors.push(error.message));
	await page.goto("http://127.0.0.1:8080/");
	await page.getByRole("heading", { name: "Make room for good work." }).waitFor();
	await page.screenshot({ path: join(previews, "overview-desktop.png"), fullPage: true });
	await peer.goto("http://127.0.0.1:8080/#/tasks");
	await peer.getByLabel("Search tasks").waitFor();
	await peer.getByLabel("Search tasks").fill("Browser task");

	await page.getByRole("link", { name: "+ Create a task", exact: true }).click();
	await page.getByLabel("Task title").fill("Browser task");
	await page.getByLabel("Description", { exact: true }).fill("A real browser journey through Tatzel controls.");
	await page.getByLabel("Status", { exact: true }).selectOption("doing");
	await page.getByLabel("Priority", { exact: true }).selectOption("high");
	await page.getByRole("button", { name: "Save task", exact: true }).click();
	await page.waitForURL("**/#/tasks");
	await page.getByRole("link", { name: "Browser task", exact: true }).waitFor();
	await peer.getByRole("link", { name: "Browser task", exact: true }).waitFor();
	assert.equal(await peer.getByLabel("Search tasks").evaluate(element => document.activeElement === element), true, "external updates preserve search focus");
	await page.screenshot({ path: join(previews, "tasks-desktop.png"), fullPage: true });

	await page.getByLabel("Search tasks").fill("Browser task");
	await page.getByText("1 task", { exact: true }).waitFor();
	assert.equal(await page.getByLabel("Search tasks").evaluate(element => document.activeElement === element), true, "search preserves focus");
	await page.getByRole("link", { name: "Browser task", exact: true }).click();
	await page.getByLabel("Task title").waitFor();
	await page.goBack();
	await page.getByLabel("Search tasks").waitFor();
	assert.equal(await page.getByLabel("Search tasks").getAttribute("value"), "Browser task");
	await page.goForward();
	await page.getByLabel("Task title").waitFor();
	await peer.getByRole("link", { name: "Browser task", exact: true }).click();
	await peer.getByLabel("Task title").fill("A draft kept in the second window");
	await page.getByLabel("Task title").fill("Browser task updated");
	await page.getByRole("button", { name: "Save task", exact: true }).click();
	await page.getByRole("link", { name: "Browser task updated", exact: true }).waitFor();
	await peer.getByText("Workspace updated. Your open draft has been preserved.", { exact: true }).waitFor();
	assert.equal(await peer.getByLabel("Task title").inputValue(), "A draft kept in the second window");
	await peer.getByRole("button", { name: "Save task", exact: true }).click();
	await peer.getByText(/This item changed or was removed in another window/).waitFor();
	assert.equal(await peer.getByLabel("Task title").inputValue(), "A draft kept in the second window");

	await page.getByRole("link", { name: "Projects", exact: true }).click();
	await page.getByRole("link", { name: "+ Create a project", exact: true }).click();
	await page.getByLabel("Project name").fill("Browser project");
	await page.getByLabel("Description", { exact: true }).fill("Project creation, archive, and restore.");
	await page.getByRole("button", { name: "Save project", exact: true }).click();
	const card = page.locator("article").filter({ has: page.getByRole("heading", { name: "Browser project", exact: true }) });
	await card.getByRole("button", { name: "Archive project", exact: true }).click();
	const archivedCard = page.locator("article").filter({ has: page.getByRole("heading", { name: "Browser project · Archived", exact: true }) });
	await archivedCard.getByRole("button", { name: "Restore project", exact: true }).click();
	await card.getByRole("link", { name: "Edit project", exact: true }).click();
	await page.getByLabel("Project name").fill("Renamed browser project");
	await page.getByRole("button", { name: "Save project", exact: true }).click();
	await page.getByRole("heading", { name: "Renamed browser project", exact: true }).waitFor();

	await page.getByRole("link", { name: "Tasks", exact: true }).click();
	await page.getByRole("link", { name: "Browser task updated", exact: true }).click();
	await page.getByRole("button", { name: "Archive task", exact: true }).click();
	await page.waitForURL("**/#/tasks");
	await page.getByLabel("Show archived tasks and projects").check();
	await page.getByRole("link", { name: "Browser task updated", exact: true }).click();
	await page.getByRole("button", { name: "Restore task", exact: true }).click();
	await page.waitForURL("**/#/tasks");
	await page.getByRole("link", { name: "Browser task updated", exact: true }).click();
	await page.getByRole("button", { name: "Delete task", exact: true }).click();
	await page.getByRole("dialog").getByRole("button", { name: "Cancel", exact: true }).click();
	await page.getByRole("dialog").waitFor({ state: "hidden" });
	await page.getByRole("button", { name: "Delete task", exact: true }).click();
	await page.getByRole("dialog").getByRole("button", { name: "Confirm", exact: true }).click();
	await page.getByText("Task deleted.", { exact: true }).waitFor();
	await page.waitForURL("**/#/tasks");
	await page.getByText("0 tasks", { exact: true }).waitFor();

	await page.getByRole("link", { name: "Settings", exact: true }).click();
	await page.getByLabel("Use compact task rows").check();
	await page.getByLabel("Show completed tasks by default").uncheck();
	await page.getByRole("button", { name: "Save preferences", exact: true }).click();
	await page.getByText("Preferences saved.", { exact: true }).waitFor();
	await page.reload();
	await page.getByLabel("Use compact task rows").waitFor();
	assert.equal(await page.getByLabel("Use compact task rows").isChecked(), true);
	assert.equal(await page.getByLabel("Show completed tasks by default").isChecked(), false);
	await page.setViewportSize({ width: 390, height: 844 });
	await page.getByRole("link", { name: "Overview", exact: true }).click();
	await page.getByRole("heading", { name: "Make room for good work." }).waitFor();
	assert.ok(await page.evaluate(() => document.documentElement.scrollWidth <= window.innerWidth), "mobile overview fits viewport");
	await page.screenshot({ path: join(previews, "overview-mobile.png"), fullPage: true });
	await page.getByRole("link", { name: "Tasks", exact: true }).click();
	await page.getByRole("button", { name: "Clear filters", exact: true }).click();
	await page.getByText("7 tasks", { exact: true }).waitFor();
	assert.ok(await page.evaluate(() => document.documentElement.scrollWidth <= window.innerWidth), "mobile tasks fit viewport");
	await page.screenshot({ path: join(previews, "tasks-mobile.png"), fullPage: true });
	assert.deepEqual(errors, []);
	const stopped = once(server, "exit");
	server.kill("SIGTERM");
	await stopped;
	await page.getByRole("status").getByRole("button", { name: "Reload workspace", exact: true }).waitFor();
	assert.equal(await page.locator('main').evaluate(element => element.inert), true);
	console.log("Chromium UI journey passed: CRUD, live updates, draft conflicts, search focus, Back/Forward, modal actions, persisted settings, responsive layout, and disconnect feedback.");
} catch (error) {
	if (page) await page.screenshot({ path: join(previews, "failure.png"), fullPage: true }).catch(() => {});
	console.error(diagnostics);
	throw error;
} finally {
	await browser?.close();
	if (server.exitCode === null) { const stopped = once(server, "exit"); server.kill("SIGTERM"); await stopped; }
	await rm(directory, { recursive: true, force: true });
}
