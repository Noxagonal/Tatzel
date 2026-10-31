import type { ReceiveServerMessage, SendServerMessage } from "./protocol.js";

const wsProtocol = window.location.protocol === "https:" ? "wss:" : "ws:";
const wsUrl = wsProtocol + "//" + window.location.host + "/ws";

/** Browser transport for this page; reconnect currently requires reloading. */
export const socket = new WebSocket(wsUrl);

/**
 * @brief Checks whether a decoded JSON value is an object payload.
 * @param value Decoded value to inspect.
 * @returns Whether value is a nonnull, nonarray object.
 * @pre None.
 * @post The inspected value is unchanged.
 */
function isRecord(value: unknown): value is Record<string, unknown>
{
	return typeof value === "object" && value !== null && !Array.isArray(value);
}

const domFields: Record<string, readonly string[]> = {
	create_element_part: ["part_id", "parent_id", "tag", "text_content"],
	remove_element_part: ["part_id"],
	set_attribute: ["part_id", "attribute", "attribute_value"],
	remove_attribute: ["part_id", "attribute"],
	add_class: ["part_id", "class_name"],
	remove_class: ["part_id", "class_name"],
	set_text: ["part_id", "text"],
	set_value: ["part_id", "value"],
	set_modal_open: ["part_id", "open"],
	set_on_click: ["part_id"],
	set_on_change: ["part_id"],
	set_on_submit: ["part_id"]
};

/**
 * @brief Parses and validates the server's op/data envelope and DOM payloads.
 * @param event Text WebSocket message event.
 * @returns Validated server message.
 * @pre event contains a server text frame.
 * @post No DOM state is changed.
 * @throws Error If the frame is not valid JSON or does not match the protocol.
 */
export function parseServerMessage(event: MessageEvent): ReceiveServerMessage
{
	if (typeof event.data !== "string") throw new Error("Expected a text WebSocket frame");
	const message: unknown = JSON.parse(event.data);
	if (!isRecord(message) || typeof message.op !== "string") throw new Error("Expected an op/data envelope");

	const { op, data } = message;
	if (op === "connected" || op === "error") {
		if (!isRecord(data) || typeof data.message !== "string"
			|| (op === "error" && typeof data.code !== "string")) throw new Error("Invalid status payload");
	}
	else {
		if (!Object.hasOwn(domFields, op)) throw new Error("Unknown server operation");
		const fields = domFields[op];
		if (!Array.isArray(data)) throw new Error("DOM operations require data arrays");
		for (const part of data) {
			if (!isRecord(part) || !fields.every(field =>
				typeof part[field] === (field === "open" ? "boolean" : "string"))) {
				throw new Error("Invalid DOM operation payload");
			}
			if (op === "create_element_part") {
				if (!Array.isArray(part.classes) || !part.classes.every(value => typeof value === "string")
					|| !isRecord(part.attributes)
					|| !Object.values(part.attributes).every(value => typeof value === "string")) {
					throw new Error("Invalid DOM classes or attributes");
				}
			}
		}
	}
	return message as ReceiveServerMessage;
}

/**
 * @brief Sends an application event using concrete browser part IDs.
 * @param message Complete browser op/data message.
 * @pre DOM event sources use the exact part IDs assigned by the server.
 * @post The message is sent if the transport is open; otherwise a warning is logged.
 */
export function sendToServer(message: SendServerMessage): void
{
	if (socket.readyState !== WebSocket.OPEN) {
		console.warn("WebSocket is not open. Message not sent:", message);
		return;
	}
	socket.send(JSON.stringify(message));
}
