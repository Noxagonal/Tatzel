/** @brief Public connection phases; recovery creates a fresh logical page. */
export type ConnectionPhase = "connecting" | "initializing" | "ready" | "disconnected" | "failed";
/** @brief Immutable snapshot delivered to browser-side connection observers. */
export type ConnectionState = Readonly<{ phase: ConnectionPhase; message: string; code?: string }>;
let current: ConnectionState = Object.freeze({ phase: "connecting", message: "Connecting…" });
const listeners = new Set<(state: ConnectionState) => void>();

/**
 * @brief Reads the last published connection state.
 * @returns Immutable phase, public message, and optional failure code.
 * @pre None.
 * @post State is unchanged.
 */
export function getConnectionState(): ConnectionState { return current; }

/**
 * @brief Subscribes to state changes, immediately receiving the current snapshot.
 * @param listener Observer; its exceptions cannot interrupt transport processing.
 * @returns Unsubscribe function releasing the observer.
 * @pre Called on the browser thread.
 * @post The observer remains registered until explicitly removed.
 */
export function onConnectionStateChanged(listener: (state: ConnectionState) => void): () => void
{
	listeners.add(listener);
	try { listener(current); }
	catch (error) { console.error("Connection observer failed:", error); }
	return () => { listeners.delete(listener); };
}

/**
 * @brief Publishes connection state and renders an accessible template or fallback surface.
 * @param phase New phase; failed and disconnected pages cannot resume through this API.
 * @param message Public message, empty to hide ready-state feedback.
 * @param code Optional stable protocol error code.
 * @pre The document body and logical root exist.
 * @post Interaction is enabled only for ready pages; terminal states offer reload without replaying actions.
 */
export function setConnectionState(phase: ConnectionPhase, message: string, code: string | undefined = undefined): void
{
	if ((current.phase === "failed" || current.phase === "disconnected") && phase !== current.phase) return;
	current = Object.freeze({ phase, message, ...(code ? { code } : {}) });
	const root = document.getElementById("root|main");
	if (root) root.inert = phase !== "ready";
	document.documentElement.dataset.tatzelConnection = phase;
	let surface = document.getElementById("connection-status");
	if (!surface) {
		surface = document.createElement("div");
		surface.id = "connection-status";
		document.body.prepend(surface);
	}
	surface.setAttribute("role", phase === "failed" || (phase === "ready" && code) ? "alert" : "status");
	surface.setAttribute("aria-live", phase === "failed" || (phase === "ready" && code) ? "assertive" : "polite");
	surface.hidden = phase === "ready" && message === "";
	surface.replaceChildren(document.createTextNode(message));
	if (phase === "failed" || phase === "disconnected") {
		const reload = document.createElement("button");
		reload.textContent = surface.dataset.reloadLabel ?? "Reload page";
		reload.addEventListener("click", () => window.location.reload());
		surface.append(reload);
	}
	else if (phase === "ready" && code) {
		const dismiss = document.createElement("button");
		dismiss.textContent = "Dismiss";
		dismiss.addEventListener("click", () => setConnectionState("ready", ""));
		surface.append(dismiss);
	}
	for (const listener of [...listeners]) {
		try { listener(current); }
		catch (error) { console.error("Connection observer failed:", error); }
	}
	window.dispatchEvent(new CustomEvent("tatzel:connection-state", { detail: current }));
}
