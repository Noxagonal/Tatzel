/**
 * @brief Protocol-v2 transport adapter for the existing control behavior suites.
 * @details Exposes operations individually to their existing DOM observers while
 * retaining raw frames for batch assertions. Native-wire tests use WebSocket directly.
 */
export class ProtocolSocket extends EventTarget {
	/**
	 * @brief Opens a native transport and tracks field generations.
	 * @param url WebSocket endpoint.
	 * @pre The example server is running.
	 * @post Control events are forwarded and ordered batch operations are observed individually.
	 */
	constructor(url) {
		super();
		this.frames = [];
		this.fields = new Map();
		this.socket = new WebSocket(url);
		for (const type of ["open", "close", "error"]) this.socket.addEventListener(type, () => this.dispatchEvent(new Event(type)));
		this.socket.addEventListener("message", event => {
			const frame = JSON.parse(event.data);
			this.frames.push(frame);
			const operations = frame.op === "update_batch" ? frame.data.operations : [frame];
			for (const operation of operations) {
				if (operation.op === "create_element_part") {
					for (const part of operation.data) if (part.attributes["data-ui-form-field"]) this.fields.set(part.part_id, { epoch: 0, revision: 0 });
				}
				if (["set_value", "set_select_options", "set_radio_options"].includes(operation.op)) {
					for (const part of operation.data) {
						const state = this.fields.get(part.part_id) ?? { epoch: 0, revision: 0 };
						if (part.epoch > state.epoch) this.fields.set(part.part_id, { epoch: part.epoch, revision: 0 });
					}
				}
				this.dispatchEvent(new MessageEvent("message", { data: JSON.stringify(operation) }));
			}
		});
	}
	/** @brief Reads the native connection state. @returns WebSocket readyState. @pre None. @post State is unchanged. */
	get readyState() { return this.socket.readyState; }
	/** @brief Closes the native transport. @pre None. @post Closure is requested. */
	close() { this.socket.close(); }
	/**
	 * @brief Supplies explicit current revisions to synthetic behavior-test events.
	 * @param text Serialized event or intentionally malformed JSON.
	 * @pre The native transport is open.
	 * @post One complete event is sent; explicitly provided metadata is preserved.
	 */
	send(text) {
		let message;
		try { message = JSON.parse(text); } catch { this.socket.send(text); return; }
		if (message.op === "connect") message.data.protocol_version ??= 2;
		const stamp = field => {
			if (field.epoch !== undefined || field.revision !== undefined) return;
			const state = this.fields.get(field.part_id) ?? { epoch: 0, revision: 0 };
			++state.revision;
			this.fields.set(field.part_id, state);
			Object.assign(field, state);
		};
		if (message.op === "on_input" || message.op === "on_change") stamp(message.data);
		if (message.op === "on_submit" && Array.isArray(message.data.fields)) for (const field of message.data.fields) stamp(field);
		this.socket.send(JSON.stringify(message));
	}
}
