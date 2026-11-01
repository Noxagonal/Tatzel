
import type { ReceiveServerMessage } from "./protocol.js"
import { socket, parseServerMessage, sendToServer } from "./websocket.js";
import { registerBrowserEventHandlers } from "./events.js";
import {
	addClass,
	createElementPart,
	removeElementPart,
	setAttribute,
	removeAttribute,
	removeClass,
	setModalOpen,
	setOnChange,
	setOnClick,
	setOnSubmit,
	setText,
	setValue,
	setSelectOptions,
	setRadioOptions,
} from "./dom-ops.js";


/**
 * @brief Displays browser-owned transport feedback when the template provides a status surface.
 * @param message User-visible text, empty to hide the status.
 * @param disconnected Whether controls must stop accepting input until reload.
 * @pre The page document is loaded.
 * @post The status is updated without depending on a live server connection.
 */
function showConnectionStatus(message: string, disconnected = false): void
{
	const status = document.getElementById("connection-status");
	if (!status) return;
	status.hidden = message === "";
	status.replaceChildren(document.createTextNode(message));
	if (disconnected) {
		const root = document.getElementById("root|main");
		if (root) root.inert = true;
		const reload = document.createElement("button");
		reload.textContent = "Reload workspace";
		reload.addEventListener("click", () => window.location.reload());
		status.append(reload);
	}
}

registerBrowserEventHandlers();
socket.addEventListener("close", () => showConnectionStatus("Connection lost. Reload to reconnect; saved changes are kept.", true));
socket.addEventListener("error", () => showConnectionStatus("Unable to connect to the workspace. Reload to retry.", true));

window.addEventListener("hashchange", () => {
	sendToServer({ op: "on_navigate", data: { hash: window.location.hash } });
});


socket.addEventListener(
	"open",
	() => {
	sendToServer({
		op: "connect",
		data: {
			route_path: window.location.pathname,
			query: window.location.search,
			hash: window.location.hash
		}
	});
});


socket.addEventListener(
	"message",
	(event) => {
	let message: ReceiveServerMessage;
	try {
		message = parseServerMessage(event);
	}
	catch (error) {
		console.error("Invalid server message:", event.data, error);
		return;
	}

	switch (message.op) {
		case "set_location":
			if (window.location.hash !== message.data.hash) {
				window.history.pushState(null, "", window.location.pathname + window.location.search + message.data.hash);
			}
			break;

		case "connected":
			showConnectionStatus("");
			break;

		case "error":
			console.error("Tatzel server error:", message.data.code, message.data.message);
			showConnectionStatus(message.data.message);
			break;

		case "create_element_part":
			createElementPart(message.data);
			break;

		case "remove_element_part":
			removeElementPart(message.data);
			break;

		case "set_attribute":
			setAttribute(message.data);
			break;

		case "remove_attribute":
			removeAttribute(message.data);
			break;

		case "add_class":
			addClass(message.data);
			break;

		case "remove_class":
			removeClass(message.data);
			break;

		case "set_text":
			setText(message.data);
			break;

		case "set_value":
			setValue(message.data);
			break;

		case "set_radio_options":
			setRadioOptions(message.data);
			break;

		case "set_select_options":
			setSelectOptions(message.data);
			break;

		case "set_modal_open":
			setModalOpen(message.data);
			break;

		case "set_on_click":
			setOnClick(message.data);
			break;

		case "set_on_change":
			setOnChange(message.data);
			break;

		case "set_on_submit":
			setOnSubmit(message.data);
			break;

		default:
			console.warn("Unknown message:", message);
			break;
	}
});
