
import { ReceiveServerMessage } from "./protocol.js"
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
} from "./dom-ops.js";


registerBrowserEventHandlers();


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
		console.error("Invalid JSON from server:", event.data, error);
		return;
	}

	switch (message.op) {
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
