
import { ReceiveServerMessage } from "./protocol.js"
import { socket, parseServerMessage, sendToServer } from "./websocket.js";
import { registerBrowserEventHandlers } from "./events.js";
import {
	addClass,
	createElement,
	removeElement,
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
		route_path: window.location.pathname,
		query: window.location.search,
		hash: window.location.hash
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
		case "create_element":
			createElement(message.parts);
			break;

		case "delete_element":
			removeElement(message.id);
			break;

		case "set_attribute":
			setAttribute(message.id, message.attribute, message.attribute_value);
			break;

		case "remove_attribute":
			removeAttribute(message.id, message.attribute);
			break;

		case "add_class":
			addClass(message.id, message.class_name);
			break;

		case "remove_class":
			removeClass(message.id, message.class_name);
			break;

		case "set_text":
			setText(message.id, message.text);
			break;

		case "set_value":
			setValue(message.id, message.value);
			break;

		case "set_modal_open":
			setModalOpen(message.id, message.open);
			break;

		case "set_on_click":
			setOnClick(message.id);
			break;

		case "set_on_change":
			setOnChange(message.id);
			break;

		case "set_on_submit":
			setOnSubmit(message.id);
			break;

		default:
			console.warn("Unknown message:", message);
			break;
	}
});
