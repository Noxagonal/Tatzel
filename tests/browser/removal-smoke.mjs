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
	throw new Error("Timed out waiting for removal integration state");
}

/**
 * @brief Builds a lifecycle page over a real browser transport.
 * @returns Transport, messages, and exact part-identity lookup helpers.
 * @pre The example server is listening on port 8080.
 * @post The complete lifecycle page is initialized.
 */
async function connectLifecycle()
{
	const socket = new ProtocolSocket("ws://127.0.0.1:8080/ws");
	const messages = [];
	socket.addEventListener("message", event => messages.push(JSON.parse(event.data)));
	await once(socket, "open", { signal: AbortSignal.timeout(5000) });
	socket.send(JSON.stringify({ op: "connect", data: { route_path: "/lifecycle" } }));
	const parts = () => messages.filter(message => message.op === "create_element_part").flatMap(message => message.data);
	await waitFor(() => {
		const last = parts().find(part => part.text_content === "Remove then throw");
		return last && messages.some(message => message.op === "set_on_click" && message.data[0].part_id === last.part_id);
	});
	return { socket, messages, parts,
		button: text => parts().find(part => part.tag === "button" && part.text_content === text).part_id,
		status: parts().find(part => part.text_content === "Ready").part_id };
}

test("logical removal invalidates identities, preserves parents, and survives callback errors", { timeout: 30000 }, async () => {
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
				const response = await fetch("http://127.0.0.1:8080/lifecycle", { headers: { Connection: "close" }, signal: AbortSignal.timeout(500) });
				if (response.ok) { ready = true; break; }
			} catch {}
			await pause(30);
		}
		assert.ok(ready, diagnostics);
		first = await connectLifecycle();
		second = await connectLifecycle();
		const send = (op, part_id, extra = {}) => first.socket.send(JSON.stringify({ op, data: { part_id, ...extra } }));
		const click = name => send("on_click", first.button(name));
		const status = text => waitFor(() => first.messages.some(message => message.op === "set_text"
			&& message.data.some(part => part.part_id === first.status && part.text === text)));
		const removed = id => first.messages.some(message => message.op === "remove_element_part" && message.data.some(part => part.part_id === id));
		const errors = () => first.messages.filter(message => message.op === "error");
		const part = id => first.parts().find(part => part.part_id === id);
		const initialOther = second.messages.filter(message => ["set_text", "remove_element_part"].includes(message.op)).length;

		click("Add item"); await status("Added item 1");
		click("Add item"); await status("Added item 2");
		const firstRow = first.parts().find(part => part.text_content === "Item 1").parent_id;
		const secondRow = first.parts().find(part => part.text_content === "Item 2").parent_id;
		const list = part(firstRow).parent_id;
		const retiredButton = first.button("Remove item 1");
		click("Remove item 1"); await status("Item removed");
		assert.ok(removed(firstRow));
		assert.ok(!removed(retiredButton), "removing a rendered root covers its descendants");
		send("on_click", retiredButton);
		click("Try retired builder"); await status("Retired builder rejected");
		assert.equal(errors().length, 0);
		await waitFor(() => diagnostics.includes("Builder parent has been removed or destroyed"));
		assert.match(diagnostics, /Exception: tatzel::Exception/);
		const beforeStandardFailure = diagnostics.length;
		click("Throw standard exception");
		await waitFor(() => errors().length === 1);
		await waitFor(() => diagnostics.slice(beforeStandardFailure).includes("Exception: std::out_of_range"));
		const standardReport = diagnostics.slice(beforeStandardFailure);
		assert.ok(standardReport.includes("Message:") && !standardReport.includes("Thrown at:") && !standardReport.includes("Stack trace:"));
		assert.equal(errors()[0].data.code, "handler_failed");
		click("Clear items"); await status("Items cleared");
		assert.ok(removed(secondRow) && !removed(list));
		click("Add item"); await status("Added item 3");
		const thirdRow = first.parts().find(part => part.text_content === "Item 3").parent_id;
		assert.equal(part(thirdRow).parent_id, list);

		const cardContent = first.parts().find(part => part.text_content === "Disposable card content").part_id;
		const cardTitle = first.parts().find(part => part.text_content === "Preserved card").part_id;
		const clearContent = first.button("Clear card content");
		click("Clear card content"); await waitFor(() => removed(cardContent));
		assert.ok(!removed(cardTitle) && !removed(clearContent));

		const group = first.parts().find(part => part.text_content === "Disposable group").parent_id;
		click("Remove group"); await status("Group removed");
		assert.ok(removed(group));

		const label = first.parts().find(part => part.text_content === "Temporary required field");
		click("Remove field"); await waitFor(() => removed(label.parent_id));
		const form = first.parts().find(part => part.tag === "form").part_id;
		send("on_change", label.attributes.for, { value: "Late browser edit" });
		send("on_submit", form, { fields: [] }); await status("Submitted without field");
		assert.equal(errors().length, 1);

		const modal = first.parts().find(part => part.tag === "dialog").part_id;
		click("Open disposable modal");
		await waitFor(() => first.messages.some(message => message.op === "set_modal_open" && message.data[0].open));
		click("Remove this modal"); await status("Modal removed");
		assert.ok(removed(modal));
		send("on_dismiss", modal);
		click("Open disposable modal"); await status("Modal already removed");
		assert.ok(!first.messages.some(message => message.op === "set_text" && message.data.some(part => part.text === "Modal closed")));

		const throwing = first.button("Remove then throw");
		click("Remove then throw");
		await waitFor(() => errors().length === 2);
		await waitFor(() => diagnostics.includes("application callback invocation boundary"));
		await waitFor(() => diagnostics.includes("Lifecycle example callback failed after removal"));
		assert.match(diagnostics, /Route: \/lifecycle\n  Operation: on_click\n  Element: ui-/);
		assert.match(diagnostics, /Exception: tatzel::Exception\n  Message: Lifecycle example callback failed after removal/);
		assert.match(diagnostics, /Thrown at: .*Main\.cpp:\d+\n  Function: void FailLifecycleAction/);
		assert.match(diagnostics, /Callback: Button click/);
		assert.ok(!diagnostics.includes("__libc_start_main") && !diagnostics.includes("WsController::DispatchMessage"));
		assert.ok(!diagnostics.includes("/bits/invoke.h") && !diagnostics.includes("/bits/std_function.h"));
		assert.doesNotMatch(diagnostics, /#\d+ _Z/);
		assert.ok(removed(throwing));
		send("on_click", throwing);
		click("Add item"); await status("Added item 4");
		assert.equal(errors().length, 2);

		for (let index = 5; index <= 34; index++) click("Add item");
		await status("Added item 34");
		const beforeClear = first.messages.length;
		click("Clear items");
		await waitFor(() => first.messages.slice(beforeClear).some(message => message.op === "remove_element_part"));
		const batch = first.messages.slice(beforeClear).find(message => message.op === "remove_element_part");
		assert.equal(batch.data.length, 32);
		assert.equal(second.messages.filter(message => ["set_text", "remove_element_part"].includes(message.op)).length, initialOther);
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
