
import { ReceiveServerMessage, SendServerMessage } from "./protocol.js";


const wsProtocol = window.location.protocol === "https:" ? "wss:" : "ws:";
const wsUrl = `${wsProtocol}//${window.location.host}/ws`;

export const socket = new WebSocket(wsUrl);


export function parseServerMessage(event: MessageEvent): ReceiveServerMessage
{
	if (typeof event.data !== "string") {
		throw new Error("Expected text WebSocket message");
	}

	console.log(event.data);

	return JSON.parse(event.data) as ReceiveServerMessage;
}


export function sendToServer(message: SendServerMessage): void
{
	if (socket.readyState !== WebSocket.OPEN) {
		console.warn("WebSocket is not open. Message not sent: ", message);
		return;
	}

	socket.send(JSON.stringify(message));
}
