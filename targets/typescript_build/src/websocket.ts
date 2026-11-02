import type { ReceiveServerMessage, ReceiveOperation, SendServerMessage } from "./protocol.js";
import { setConnectionState } from "./connection-state.js";

export const protocolVersion = 2;
const wsProtocol = window.location.protocol === "https:" ? "wss:" : "ws:";
const wsUrl = wsProtocol + "//" + window.location.host + "/ws";
/** @brief One page owns one transport; recovery reloads rather than replaying pending actions. */
export const socket = new WebSocket(wsUrl);

/**
 * @brief Checks whether a decoded value is an object payload.
 * @param value Decoded JSON value.
 * @returns Whether value is a nonnull, nonarray object.
 * @pre None.
 * @post The value is unchanged.
 */
function isRecord(value: unknown): value is Record<string, unknown>
{
	return typeof value === "object" && value !== null && !Array.isArray(value);
}

/**
 * @brief Checks a counter without accepting rounded or nonfinite values.
 * @param value Decoded counter.
 * @returns Whether value is an exactly representable nonnegative integer.
 * @pre None.
 * @post The value is unchanged.
 */
function isCounter(value: unknown): value is number { return typeof value === "number" && Number.isSafeInteger(value) && value >= 0; }

const domFields: Record<string, readonly string[]> = {
	create_element_part: ["part_id", "parent_id", "tag", "text_content"],
	remove_element_part: ["part_id"], set_attribute: ["part_id", "attribute", "attribute_value"],
	remove_attribute: ["part_id", "attribute"], add_class: ["part_id", "class_name"], remove_class: ["part_id", "class_name"],
	set_text: ["part_id", "text"], set_value: ["part_id", "value"], set_modal_open: ["part_id", "open"],
	set_select_options: ["part_id", "placeholder", "value"], set_radio_options: ["part_id", "value"],
	set_on_click: ["part_id"], set_on_change: ["part_id"], set_on_submit: ["part_id"]
};

/**
 * @brief Validates one operation without mutating any DOM state.
 * @param message Decoded operation from a batch.
 * @returns Typed, validated operation.
 * @pre None.
 * @post Malformed payloads fail before a batch can begin applying.
 * @throws Error If the operation or its payload is invalid.
 */
function validateOperation(message: unknown): ReceiveOperation
{
	if (!isRecord(message) || typeof message.op !== "string") throw new Error("Expected an op/data envelope");
	const { op, data } = message;
	if (op === "set_location") {
		if (!isRecord(data) || typeof data.hash !== "string" || data.hash.length > 2048
			|| (data.hash !== "" && !data.hash.startsWith("#"))) throw new Error("Invalid navigation payload");
		return message as ReceiveOperation;
	}
	if (!Object.hasOwn(domFields, op) || !Array.isArray(data)) throw new Error("Unknown operation or invalid data array");
	for (const part of data) {
		if (!isRecord(part) || !domFields[op].every(field => typeof part[field] === (field === "open" ? "boolean" : "string")))
			throw new Error("Invalid DOM operation payload");
		if (op === "create_element_part") {
			if (!Array.isArray(part.classes) || !part.classes.every(value => typeof value === "string")
				|| !isRecord(part.attributes) || !Object.values(part.attributes).every(value => typeof value === "string"))
				throw new Error("Invalid DOM classes or attributes");
		}
		if (op === "set_value" || op === "set_select_options" || op === "set_radio_options") {
			if (!isCounter(part.epoch) || !isCounter(part.base_epoch) || !isCounter(part.client_revision)
				|| part.epoch < part.base_epoch || typeof part.replace !== "boolean"
				|| (part.replace && part.epoch <= part.base_epoch)) throw new Error("Invalid field revision metadata");
		}
		if (op === "set_select_options" || op === "set_radio_options") {
			if (!Array.isArray(part.options)) throw new Error("Invalid choices");
			const ids = new Set<string>();
			for (const option of part.options) {
				if (!isRecord(option) || typeof option.value !== "string" || option.value === "" || ids.has(option.value)
					|| typeof option.label !== "string" || typeof option.disabled !== "boolean") throw new Error("Invalid choice");
				ids.add(option.value);
			}
			if (part.value !== "" && !part.options.some(option => option.value === part.value && !option.disabled))
				throw new Error("Unavailable selected choice");
		}
	}
	return message as ReceiveOperation;
}

/**
 * @brief Parses an isolated operation for adapter/runtime behavior checks.
 * @param event Serialized operation envelope.
 * @returns Validated operation; not a complete version-two transport frame.
 * @pre event contains text JSON.
 * @post No DOM state changes.
 */
export function parseServerOperation(event: MessageEvent): ReceiveOperation { return validateOperation(JSON.parse(event.data)); }

/**
 * @brief Validates a complete version-two frame, including every operation in a batch.
 * @param event Text WebSocket message event.
 * @returns Typed control or update-batch frame.
 * @pre None.
 * @post No DOM state changes, including on malformed later operations.
 * @throws Error If framing, version, counters, limits, or any operation is invalid.
 */
export function parseServerMessage(event: MessageEvent): ReceiveServerMessage
{
	if (typeof event.data !== "string" || new TextEncoder().encode(event.data).length > 1024 * 1024)
		throw new Error("Expected a bounded text WebSocket frame");
	const message: unknown = JSON.parse(event.data);
	if (!isRecord(message) || typeof message.op !== "string" || !isRecord(message.data)) throw new Error("Invalid server envelope");
	const { op, data } = message;
	if (op === "connected") {
		if (data.protocol_version !== protocolVersion || typeof data.message !== "string") throw new Error("Page and server protocol versions differ; reload the page");
	}
	else if (op === "page_ready") {
		if (!isCounter(data.sequence)) throw new Error("Invalid page-ready sequence");
	}
	else if (op === "error") {
		if (typeof data.code !== "string" || typeof data.message !== "string" || typeof data.fatal !== "boolean") throw new Error("Invalid error payload");
	}
	else if (op === "update_batch") {
		if (!isCounter(data.sequence) || data.sequence === 0 || !Array.isArray(data.operations)
			|| data.operations.length === 0 || data.operations.length > 128) throw new Error("Invalid update batch");
		for (const operation of data.operations) validateOperation(operation);
	}
	else throw new Error("Unknown server frame");
	return message as ReceiveServerMessage;
}

/**
 * @brief Sends a browser event without queuing actions for a future session.
 * @param message Complete typed event envelope.
 * @pre Field events include their current edit identity.
 * @post Open transports send the event; closed transports expose reload recovery.
 */
export function sendToServer(message: SendServerMessage): void
{
	if (socket.readyState !== WebSocket.OPEN) {
		setConnectionState("disconnected", "Connection lost. Reload to start a fresh page.", "connection_lost");
		return;
	}
	try { socket.send(JSON.stringify(message)); }
	catch (error) {
		console.error("Unable to send page event:", error);
		setConnectionState("disconnected", "The connection is unavailable. Reload to start a fresh page.", "send_failed");
	}
}
