import type { ClientEdit, FieldUpdate } from "./protocol.js";

/** @brief Per-node edit state; weak keys disappear with removed DOM subtrees. */
type State = {
	epoch: number;
	revision: number;
	observed: string | boolean | null;
	compositionEpoch?: number;
	replacement?: string;
};
const states = new WeakMap<Element, State>();

/**
 * @brief Reads native field state, preserving malformed numeric drafts.
 * @param element Concrete field or registered radio group.
 * @returns Typed value, or null for unsupported nodes.
 * @pre None.
 * @post The DOM is unchanged.
 */
export function getElementValue(element: Element): string | boolean | null
{
	if (element instanceof HTMLInputElement) {
		if (element.type === "checkbox") return element.checked;
		if (element.type === "number" && element.validity.badInput) return "NaN";
		return element.value;
	}
	if (element instanceof HTMLTextAreaElement || element instanceof HTMLSelectElement) return element.value;
	if (element instanceof HTMLFieldSetElement && element.hasAttribute("data-ui-radio-group")) {
		return element.querySelector<HTMLInputElement>('input[type="radio"]:checked')?.value ?? "";
	}
	return null;
}

/**
 * @brief Initializes one field's browser edit baseline.
 * @param element Existing field with its initial DOM attributes applied.
 * @returns Weakly stored state.
 * @pre Called on the browser thread.
 * @post The node has a generation-zero baseline if it had no previous state.
 */
function stateFor(element: Element): State
{
	let state = states.get(element);
	if (!state) {
		state = { epoch: 0, revision: 0, observed: getElementValue(element) };
		states.set(element, state);
	}
	return state;
}

/**
 * @brief Registers initial field state before the browser can edit its value.
 * @param element Newly created concrete field.
 * @pre Initial attributes are applied and the element is connected.
 * @post Later snapshots detect silent native value changes such as autofill.
 */
export function registerField(element: Element): void { stateFor(element); }

/**
 * @brief Captures a browser edit or current form snapshot revision.
 * @param element Registered native field.
 * @param event Whether a native edit event must advance the counter even for matching text.
 * @returns Current metadata, or null for cancelled old-generation composition events.
 * @pre Called before sending an edit or collecting a form snapshot.
 * @post Changed browser values advance a safe generation-local counter.
 */
export function captureEdit(element: Element, event = false): ClientEdit | null
{
	const state = stateFor(element);
	if (state.compositionEpoch !== undefined && state.compositionEpoch !== state.epoch) return null;
	const value = getElementValue(element);
	if (event || value !== state.observed) {
		if (state.revision === Number.MAX_SAFE_INTEGER) throw new Error("Field edit counter exhausted; reload the page");
		++state.revision;
		state.observed = value;
	}
	return { epoch: state.epoch, revision: state.revision };
}

/**
 * @brief Marks an IME composition as protected against ordinary server replacements.
 * @param element Native text editor.
 * @pre The node is an editable registered field.
 * @post The composition remembers its starting value generation.
 */
export function beginComposition(element: Element): void
{
	const state = stateFor(element);
	state.compositionEpoch = state.epoch;
}

/**
 * @brief Completes composition without resurrecting text superseded by an explicit reset.
 * @param element Native text editor.
 * @returns Whether its final draft should be sent as a browser edit.
 * @pre A composition may be active.
 * @post Cancelled composition restores the authoritative replacement; ordinary composition can submit.
 */
export function endComposition(element: Element): boolean
{
	const state = stateFor(element);
	const cancelled = state.compositionEpoch !== undefined && state.compositionEpoch !== state.epoch;
	delete state.compositionEpoch;
	if (cancelled && state.replacement !== undefined) writeValue(element, state.replacement);
	state.observed = getElementValue(element);
	return !cancelled;
}

/**
 * @brief Writes an explicit field value while avoiding unnecessary caret changes.
 * @param element Supported native editor or registered radio group.
 * @param value Serialized text, including true/false for checkboxes.
 * @pre The target is a supported field.
 * @post The native value matches value where its control type permits it.
 */
function writeValue(element: Element, value: string): void
{
	if (element instanceof HTMLInputElement) {
		if (element.type === "checkbox") element.checked = value === "true";
		else if (element.value !== value || (element.type === "number" && element.validity.badInput)) element.value = value;
	}
	else if (element instanceof HTMLTextAreaElement || element instanceof HTMLSelectElement) {
		if (element.value !== value) element.value = value;
	}
	else if (element instanceof HTMLFieldSetElement) {
		for (const radio of element.querySelectorAll<HTMLInputElement>('input[type="radio"]')) radio.checked = radio.value === value;
	}
	else throw new Error("Value update targets an unsupported editor");
}

/**
 * @brief Applies a guarded server value or deliberate generation replacement.
 * @param element Concrete field whose identity remains stable.
 * @param update Validated server metadata and new value.
 * @pre The operation has passed protocol shape validation.
 * @post Ordinary stale updates are ignored; deliberate replacements invalidate old-generation input.
 */
export function applyValueUpdate(element: Element, update: FieldUpdate & { value: string }): void
{
	const state = stateFor(element);
	if (update.epoch < state.epoch) return;
	if (update.replace) {
		if (update.epoch <= state.epoch) return;
		state.epoch = update.epoch;
		state.revision = 0;
		state.replacement = update.value;
		const focused = document.activeElement === element;
		if (state.compositionEpoch !== undefined && element instanceof HTMLElement) element.blur();
		writeValue(element, update.value);
		if (focused && element instanceof HTMLElement) element.focus({ preventScroll: true });
	}
	else {
		if (update.epoch !== state.epoch || update.base_epoch !== state.epoch) throw new Error("Field update generation is out of sync");
		if (state.compositionEpoch !== undefined || state.revision !== update.client_revision) return;
		writeValue(element, update.value);
	}
	state.observed = getElementValue(element);
}

/**
 * @brief Prepares a native-choice replacement while preserving an available newer selection.
 * @param element Existing select or registered radio group.
 * @param update Validated choice generation and selected value.
 * @param options New available choices.
 * @returns Selected value and optional rebased edit, or null for an obsolete operation.
 * @pre Choices will be installed before any returned edit is sent.
 * @post Old-generation edits become obsolete; surviving newer selections get a fresh generation-local identity.
 */
export function prepareChoiceUpdate(
	element: Element,
	update: FieldUpdate & { value: string },
	options: Array<{ value: string; disabled: boolean }>
): { value: string; edit?: ClientEdit } | null
{
	const state = stateFor(element);
	if (update.epoch < state.epoch) return null;
	if (update.base_epoch !== state.epoch && update.epoch !== state.epoch) throw new Error("Choice update generation is out of sync");
	const current = getElementValue(element);
	const preserve = !update.replace && state.revision > update.client_revision
		&& typeof current === "string" && (current === "" || options.some(option => option.value === current && !option.disabled));
	const changedGeneration = update.epoch > state.epoch;
	if (changedGeneration) {
		state.epoch = update.epoch;
		state.revision = preserve ? 1 : 0;
	}
	const value = preserve ? current as string : update.value;
	state.observed = value;
	return { value, ...(preserve && changedGeneration ? { edit: { epoch: state.epoch, revision: state.revision } } : {}) };
}
