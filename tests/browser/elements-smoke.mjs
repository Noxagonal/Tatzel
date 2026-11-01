import assert from "node:assert/strict";
import { spawn } from "node:child_process";
import { once } from "node:events";
import { setTimeout as pause } from "node:timers/promises";
import { test } from "node:test";

/**
 * @brief Waits for a bounded integration state.
 * @param predicate State probe.
 * @returns First truthy probe result.
 * @pre The server is processing events.
 * @post The state is observed or a timeout is thrown.
 */
async function waitFor(predicate)
{
	const deadline = Date.now() + 5000;
	while (Date.now() < deadline) {
		const value = predicate();
		if (value) return value;
		await pause(20);
	}
	throw new Error("Timed out waiting for element integration state");
}

/**
 * @brief Compares numerical feedback independently of standard-library float formatting.
 * @param text Example status text.
 * @returns Status with its numerical amount in JavaScript's canonical representation.
 * @pre None.
 * @post Other status text and choice identifiers are unchanged.
 */
function normalizeNumericStatus(text)
{
	return text.replace(/^(Editing|Ordered): (-?\d+(?:\.\d+)?(?:e[+-]?\d+)?)/i,
		(_, prefix, amount) => `${prefix}: ${Number(amount)}`);
}

test("numeric and radio fields validate complete snapshots and synchronize resources", { timeout: 30000 }, async () => {
	const server = spawn(process.argv[2], [], { stdio: ["ignore", "pipe", "pipe"] });
	let diagnostics = "";
	server.stdout.on("data", chunk => { diagnostics += chunk; });
	server.stderr.on("data", chunk => { diagnostics += chunk; });
	let socket;
	try {
		let ready = false;
		for (let attempt = 0; attempt < 100; attempt++) {
			try {
				const response = await fetch("http://127.0.0.1:8080/elements", { headers: { Connection: "close" }, signal: AbortSignal.timeout(500) });
				if (response.ok) { ready = true; break; }
			} catch {}
			await pause(30);
		}
		assert.ok(ready, diagnostics);
		const asset = await fetch("http://127.0.0.1:8080/framework/example-image.svg", { headers: { Connection: "close" } });
		assert.equal(asset.status, 200);
		socket = new WebSocket("ws://127.0.0.1:8080/ws");
		const messages = [];
		socket.addEventListener("message", event => messages.push(JSON.parse(event.data)));
		await once(socket, "open", { signal: AbortSignal.timeout(5000) });
		socket.send(JSON.stringify({ op: "connect", data: { route_path: "/elements" } }));
		const payloads = op => messages.filter(message => message.op === op).flatMap(message => message.data);
		await waitFor(() => payloads("set_on_click").length === 4);
		const parts = payloads("create_element_part");
		const part = id => parts.find(part => part.part_id === id);
		const label = text => parts.find(part => part.text_content === text);
		const editor = text => label(text).attributes.for ?? label(text).parent_id;
		const amount = editor("Amount");
		const delivery = editor("Delivery");
		const packaging = editor("Packaging");
		const form = parts.find(part => part.tag === "form").part_id;
		const status = label("Ready").part_id;
		const link = parts.find(part => part.tag === "a");
		const image = parts.find(part => part.tag === "img");
		const send = (op, part_id, extra = {}) => socket.send(JSON.stringify({ op, data: { part_id, ...extra } }));
		const click = text => send("on_click", label(text).part_id);
		const submit = (value, choice = "pickup", wrap = "box") => send("on_submit", form, { fields: [
			{ part_id: amount, value }, { part_id: delivery, value: choice }, { part_id: packaging, value: wrap }
		] });
		const statusIs = text => waitFor(() => payloads("set_text").some(part => part.part_id === status && normalizeNumericStatus(part.text) === normalizeNumericStatus(text)));
		const invalidMessage = async action => {
			const count = messages.filter(message => message.op === "error").length;
			action();
			await waitFor(() => messages.filter(message => message.op === "error").length > count);
			assert.equal(messages.filter(message => message.op === "error").at(-1).data.code, "invalid_message");
		};
		assert.equal(part(amount).attributes.type, "number");
		assert.equal(part(delivery).tag, "fieldset");
		assert.equal(part(delivery).attributes["data-ui-form-field"], "true");
		assert.equal(link.attributes.target, "_blank");
		assert.equal(link.attributes.rel, "noopener noreferrer");
		assert.equal(image.attributes.alt, "Tatzel mountain landscape");
		assert.ok(payloads("set_attribute").some(part => part.part_id === image.part_id && part.attribute === "width" && part.attribute_value === "320"));
		assert.equal(payloads("set_radio_options").length, 2);

		submit("2.5");
		await statusIs("Ordered: 2.500000 | pickup");
		const initialReplacements = payloads("set_value").length;
		send("on_input", amount, { value: "3.5" });
		await statusIs("Editing: 3.500000");
		send("on_change", delivery, { value: "courier" });
		await statusIs("Choice: courier");
		assert.equal(payloads("set_value").length, initialReplacements, "accepted edits must not echo values");
		await invalidMessage(() => submit("4", "post"));
		await invalidMessage(() => submit(true));
		await invalidMessage(() => send("on_submit", form, { fields: [{ part_id: amount, value: "4" }] }));
		for (const [value, error] of [
			["NaN", "Enter a valid finite number."], ["1e999", "Enter a valid finite number."],
			["-1", "Value is below the minimum."], ["1.25", "Value does not match the step."], ["", "This field is required."]
		]) {
			const count = payloads("set_text").length;
			submit(value);
			await waitFor(() => payloads("set_text").slice(count).some(part => part.text === error));
			await statusIs("Check highlighted fields");
			assert.equal(payloads("set_text").filter(part => part.part_id === status).at(-1).text, "Check highlighted fields");
		}
		submit("0");
		await statusIs("Ordered: 0.000000 | pickup");
		click("Reset elements");
		await waitFor(() => payloads("set_attribute").some(part => part.part_id === image.part_id && part.attribute === "loading" && part.attribute_value === "eager"));
		assert.ok(payloads("set_value").some(part => part.part_id === amount && part.value === ""));
		assert.ok(payloads("set_attribute").some(part => part.part_id === link.part_id && part.attribute === "href" && part.attribute_value === "/forms"));
		assert.ok(payloads("remove_attribute").some(part => part.part_id === link.part_id && part.attribute === "rel"));
		assert.ok(payloads("remove_attribute").some(part => part.part_id === image.part_id && part.attribute === "height"));
		assert.ok(payloads("set_attribute").some(part => part.part_id === image.part_id && part.attribute === "alt" && part.attribute_value === ""));
		click("Refresh radios");
		await waitFor(() => payloads("set_radio_options").some(part => part.part_id === delivery && part.options.some(option => option.value === "bike")));
		await invalidMessage(() => submit("2", "courier"));
		submit("2", "bike");
		await statusIs("Ordered: 2.000000 | bike");
		click("Toggle read only");
		await waitFor(() => payloads("set_attribute").some(part => part.part_id === amount && part.attribute === "readonly"));
		send("on_change", delivery, { value: "bike" });
		await statusIs("Choice: bike");
		submit("9", "bike");
		await waitFor(() => payloads("set_text").filter(part => part.part_id === status && normalizeNumericStatus(part.text) === "Ordered: 2 | bike").length === 2);
		click("Toggle fields");
		await waitFor(() => payloads("set_attribute").some(part => part.part_id === delivery && part.attribute === "disabled"));
		const beforeDisabledSubmit = messages.filter(message => message.op === "error").length;
		send("on_submit", form, { fields: [{ part_id: packaging, value: "bag" }] });
		// An ordered error response is a barrier proving the preceding snapshot was processed.
		send("test_barrier", form);
		await waitFor(() => messages.filter(message => message.op === "error").length > beforeDisabledSubmit);
		const newErrors = messages.filter(message => message.op === "error").slice(beforeDisabledSubmit);
		assert.deepEqual(newErrors.map(message => message.data.code), ["unknown_operation"]);
	} catch (error) {
		console.error(diagnostics);
		throw error;
	} finally {
		socket?.close();
		server.kill("SIGTERM");
		await once(server, "exit");
	}
});
