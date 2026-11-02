import type { ReceiveOperation } from "./protocol.js";
import { socket, parseServerMessage, sendToServer, protocolVersion } from "./websocket.js";
import { registerBrowserEventHandlers } from "./events.js";
import { getConnectionState, setConnectionState } from "./connection-state.js";
import {
	addClass, createElementPart, removeElementPart, setAttribute, removeAttribute, removeClass,
	setModalOpen, setOnChange, setOnClick, setOnSubmit, setText, setValue, setSelectOptions, setRadioOptions
} from "./dom-ops.js";

let sequence = 0;
let acknowledged = false;
let sentHash = "";
setConnectionState("connecting", "Connecting…");
const deadline = window.setTimeout(() => failPage("The page did not become ready. Reload to try again.", "initialization_timeout"), 15000);
registerBrowserEventHandlers();

/**
 * @brief Blocks a failed page and closes its transport without automatic replay.
 * @param message Public reload explanation.
 * @param code Stable local or server failure code.
 * @pre Called on the browser thread.
 * @post The page cannot accept further edits until reload.
 */
function failPage(message: string, code: string): void
{
	window.clearTimeout(deadline);
	setConnectionState("failed", message, code);
	socket.close();
}

/**
 * @brief Applies one validated operation in its original server order.
 * @param operation One operation from a fully validated batch.
 * @pre All preceding operations in the batch have completed.
 * @post DOM or same-page history is updated, or a failure escapes to the fatal boundary.
 */
function applyOperation(operation: ReceiveOperation): void
{
	switch (operation.op) {
		case "set_location":
			if (window.location.hash !== operation.data.hash)
				window.history.pushState(null, "", window.location.pathname + window.location.search + operation.data.hash);
			sentHash = operation.data.hash;
			break;
		case "create_element_part": createElementPart(operation.data); break;
		case "remove_element_part": removeElementPart(operation.data); break;
		case "set_attribute": setAttribute(operation.data); break;
		case "remove_attribute": removeAttribute(operation.data); break;
		case "add_class": addClass(operation.data); break;
		case "remove_class": removeClass(operation.data); break;
		case "set_text": setText(operation.data); break;
		case "set_value": setValue(operation.data); break;
		case "set_radio_options": setRadioOptions(operation.data); break;
		case "set_select_options": setSelectOptions(operation.data); break;
		case "set_modal_open": setModalOpen(operation.data); break;
		case "set_on_click": setOnClick(operation.data); break;
		case "set_on_change": setOnChange(operation.data); break;
		case "set_on_submit": setOnSubmit(operation.data); break;
	}
}

window.addEventListener("hashchange", () => {
	if (getConnectionState().phase !== "ready") return;
	sentHash = window.location.hash;
	sendToServer({ op: "on_navigate", data: { hash: sentHash } });
});
window.addEventListener("offline", () => {
	window.clearTimeout(deadline);
	setConnectionState("disconnected", "The connection is offline. Reload when it is available again.", "offline");
	socket.close();
});
socket.addEventListener("close", () => {
	window.clearTimeout(deadline);
	setConnectionState("disconnected", "Connection lost. Reload to start a fresh page.", "connection_lost");
});
socket.addEventListener("error", () => {
	window.clearTimeout(deadline);
	setConnectionState("disconnected", "Unable to connect. Reload to try again.", "connection_failed");
});
socket.addEventListener("open", () => {
	sentHash = window.location.hash;
	sendToServer({ op: "connect", data: {
		protocol_version: protocolVersion, route_path: window.location.pathname,
		query: window.location.search, hash: sentHash
	} });
});
socket.addEventListener("message", event => {
	if (getConnectionState().phase === "failed" || getConnectionState().phase === "disconnected") return;
	try {
		const message = parseServerMessage(event);
		if (message.op === "connected") {
			if (acknowledged) throw new Error("Duplicate transport acknowledgment");
			acknowledged = true;
			setConnectionState("initializing", "Preparing page…");
		}
		else if (message.op === "update_batch") {
			if (!acknowledged || message.data.sequence !== sequence + 1) throw new Error("Update batches are out of order");
			for (const operation of message.data.operations) applyOperation(operation);
			sequence = message.data.sequence;
		}
		else if (message.op === "page_ready") {
			if (!acknowledged || getConnectionState().phase !== "initializing" || message.data.sequence !== sequence)
				throw new Error("Page readiness does not match applied updates");
			window.clearTimeout(deadline);
			setConnectionState("ready", "");
			if (window.location.hash !== sentHash) {
				sentHash = window.location.hash;
				sendToServer({ op: "on_navigate", data: { hash: sentHash } });
			}
		}
		else if (message.op === "error") {
			if (message.data.fatal) failPage(message.data.message, message.data.code);
			else setConnectionState(getConnectionState().phase, message.data.message, message.data.code);
		}
	}
	catch (error) {
		console.error("Unable to apply server updates:", error);
		failPage("The page is out of sync with the server. Reload to try again.", "protocol_error");
	}
});
