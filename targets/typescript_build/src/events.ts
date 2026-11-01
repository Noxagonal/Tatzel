import { sendToServer } from "./websocket.js";
import { getElementValue } from "./dom-ops.js";

/**
 * @brief Sends a non-composing edit from a registered, editable text field.
 * @param target Concrete event target.
 * @pre The browser document and WebSocket event handlers exist.
 * @post A current text edit is sent when target is a registered enabled editor.
 */
function sendInput(target: EventTarget | null): void
{
	if (!(target instanceof HTMLInputElement || target instanceof HTMLTextAreaElement)
		|| target.matches(":disabled") || target.readOnly) return;
	if (!target.hasAttribute("data-ui-on-input")) return;

	sendToServer({ op: "on_input", data: { part_id: target.id, value: getElementValue(target) as string } });
}

/**
 * @brief Sends a registered logical dismissal request.
 * @param element Concrete part belonging to a modal.
 * @pre element has a data-ui-on-dismiss marker.
 * @post A dismissal request is sent; the server controls the logical transition.
 */
function dismiss(element: Element): void
{
	sendToServer({ op: "on_dismiss", data: { part_id: element.id } });
}

/**
 * @brief Registers delegated browser events for exact adapter-selected part IDs.
 * @pre Called once after the browser document exists.
 * @post Click, input, change, submit, and modal dismissal events use op/data messages.
 */
export function registerBrowserEventHandlers(): void
{
	document.addEventListener("click", event => {
		const target = event.target;
		if (!(target instanceof Element)) return;

		const dismissal = target.closest("[data-ui-on-dismiss]");
		if (dismissal instanceof HTMLDialogElement) {
			// The dialog's article contains the content; only its surrounding surface dismisses.
			if (target === dismissal && dismissal.open) dismiss(dismissal);
		}
		else if (dismissal) dismiss(dismissal);

		const element = target.closest("[data-ui-on-click]");
		if (!element || (element instanceof HTMLButtonElement && element.matches(":disabled"))) return;

		sendToServer({ op: "on_click", data: { part_id: element.id } });
	});

	document.addEventListener("input", event => {
		if (event instanceof InputEvent && event.isComposing) return;
		sendInput(event.target);
	});

	document.addEventListener("compositionend", event => sendInput(event.target));

	document.addEventListener("change", event => {
		const target = event.target;
		if (!(target instanceof Element)) return;

		const element = target.closest("[data-ui-on-change]");
		if (!element || target.matches(":disabled") || element.matches(":disabled")
			|| ((element instanceof HTMLInputElement || element instanceof HTMLTextAreaElement) && element.readOnly)) return;

		const value = getElementValue(element);
		if (value === null) return;

		sendToServer({ op: "on_change", data: { part_id: element.id, value } });
	});

	document.addEventListener("keydown", event => {
		if (event.key !== "Enter" || event.isComposing || event.keyCode === 229) return;
		const target = event.target;
		if (!(target instanceof HTMLInputElement || target instanceof HTMLTextAreaElement)
			|| target.matches(":disabled") || !target.hasAttribute("data-ui-on-submit")) return;
		if (target instanceof HTMLTextAreaElement && !event.ctrlKey && !event.metaKey) return;

		event.preventDefault();
		if (target.form?.hasAttribute("data-ui-on-submit")) {
			target.form.requestSubmit();
			return;
		}

		// Standalone editors retain their own submission callback.
		sendInput(target);
		sendToServer({ op: "on_submit", data: { part_id: target.id } });
	});

	document.addEventListener("submit", event => {
		const target = event.target;
		if (!(target instanceof Element)) return;
		const element = target.closest("[data-ui-on-submit]");
		if (!element) return;

		event.preventDefault();
		if (element instanceof HTMLFormElement) {
			const fields: Array<{ part_id: string; value: string | boolean; }> = [];
			for (const field of Array.from(element.elements)) {
				if (!(field instanceof HTMLInputElement || field instanceof HTMLTextAreaElement || field instanceof HTMLSelectElement || field instanceof HTMLFieldSetElement)
					|| field.form !== element || !field.hasAttribute("data-ui-form-field") || field.matches(":disabled")) continue;

				const value = getElementValue(field);
				if (value !== null) fields.push({ part_id: field.id, value });
			}
			sendToServer({ op: "on_submit", data: { part_id: element.id, fields } });
		}
		else sendToServer({ op: "on_submit", data: { part_id: element.id } });
	});

	// Dialog cancel and close events do not bubble, so delegate in the capture phase.
	document.addEventListener("cancel", event => {
		const dialog = event.target;
		if (!(dialog instanceof HTMLDialogElement) || !dialog.hasAttribute("data-ui-modal")) return;

		event.preventDefault();
		dismiss(dialog);
	}, true);

	document.addEventListener("close", event => {
		const dialog = event.target;
		if (!(dialog instanceof HTMLDialogElement) || !dialog.hasAttribute("data-ui-modal")) return;
		if (!dialog.open && dialog.dataset.uiModalOpen === "true") dismiss(dialog);
	}, true);
}
