
import { registerField, applyValueUpdate, prepareChoiceUpdate } from "./field-revisions.js";
import { sendToServer } from "./websocket.js";
export { getElementValue } from "./field-revisions.js";
import type {
	ReceiveData_CreateElementPart,
	ReceiveData_RemoveElementPart,
	ReceiveData_SetAttribute,
	ReceiveData_RemoveAttribute,
	ReceiveData_AddClass,
	ReceiveData_RemoveClass,
	ReceiveData_SetText,
	ReceiveData_SetValue,
	ReceiveData_SetModalOpen,
	ReceiveData_SetOnClick,
	ReceiveData_SetOnChange,
	ReceiveData_SetOnSubmit,
	ReceiveData_SetSelectOptions,
	ReceiveData_SetRadioOptions,
} from "./protocol.js"



/**
 * @brief Resolves an operation target or identifies a desynchronized page.
 * @param id Exact DOM part identity.
 * @returns Existing target node.
 * @pre Called after validating the operation payload.
 * @post No state changes; missing targets throw to the fatal batch boundary.
 */
function requirePart(id: string): HTMLElement
{
	const element = document.getElementById(id);
	if (!element) throw new Error("UI operation references a missing DOM part");
	return element;
}

/**
 * @brief Creates concrete DOM nodes in parent-before-child order.
 * @param parts Validated DOM operation payloads using browser part IDs.
 * @pre Every parent exists before its child is processed.
 * @post Valid parts have corresponding DOM nodes; missing parents are reported.
 */
export function createElementPart(
	parts: Array<ReceiveData_CreateElementPart>
): void
{
	parts.forEach(part => {
		const domElementParent = document.getElementById(part.parent_id);
		if (domElementParent === null)
		{
			throw new Error("DOM creation references a missing parent");
		}

		const domElement = document.createElement(part.tag);

		domElement.id = part.part_id;
		domElement.className = part.classes.join(" ");
		for (const [key, value] of Object.entries(part.attributes))
		{
			if (!key.toLowerCase().startsWith("on")) domElement.setAttribute(key, value);
		}

		if (part.text_content !== "" ) domElement.textContent = part.text_content;

		if (document.getElementById(part.part_id)) throw new Error("DOM creation reuses an existing part identity");
		domElementParent.appendChild(domElement);
		if (domElement.hasAttribute("data-ui-form-field")) registerField(domElement);
	});

}


/**
 * @brief Removes concrete DOM nodes.
 * @param parts Validated DOM operation payloads using browser part IDs.
 * @pre None.
 * @post Open dialogs close silently, target subtrees are removed, and lost focus has a surviving target.
 */
export function removeElementPart(
	parts: Array<ReceiveData_RemoveElementPart>
): void
{
	const roots = parts.map(part => document.getElementById(part.part_id))
		.filter((element): element is HTMLElement => element !== null);
	const isRemoved = (element: Element) => roots.some(root => root.contains(element));
	const active = document.activeElement;
	const losesFocus = active instanceof Element && isRemoved(active);
	let fallback: Element | null = losesFocus ? active : null;
	while (fallback && isRemoved(fallback)) fallback = fallback.parentElement;

	const dialogs = new Set<HTMLDialogElement>();
	for (const root of roots) {
		if (root instanceof HTMLDialogElement && root.open) dialogs.add(root);
		for (const dialog of root.querySelectorAll<HTMLDialogElement>("dialog[open]")) dialogs.add(dialog);
	}
	for (const dialog of [...dialogs].reverse()) {
		// Disposal is a framework operation; its native close event must not request an application callback.
		dialog.dataset.uiModalOpen = "false";
		dialog.close();
	}
	for (const root of roots) root.remove();

	if (losesFocus && fallback instanceof HTMLElement && fallback.isConnected
		&& (document.activeElement === document.body || !document.activeElement?.isConnected)) {
		const target = fallback;
		const needsTabIndex = !target.hasAttribute("tabindex");
		if (needsTabIndex) {
			target.setAttribute("tabindex", "-1");
			target.addEventListener("blur", () => {
				if (target.getAttribute("tabindex") === "-1") target.removeAttribute("tabindex");
			}, { once: true });
		}
		target.focus({ preventScroll: true });
		if (needsTabIndex && document.activeElement !== target) target.removeAttribute("tabindex");
	}
}


/**
 * @brief Updates concrete HTML attributes.
 * @param parts Validated DOM operation payloads using browser part IDs.
 * @pre Target IDs refer to existing DOM nodes.
 * @post Allowed attributes are set; inline event-handler attributes are blocked.
 */
export function setAttribute(
	parts: Array<ReceiveData_SetAttribute>
): void
{
	parts.forEach(part => {
		if (part.attribute.toLowerCase().startsWith("on")) {
			console.warn("Blocked event-handler attribute:", part.attribute);
			return;
		}

		requirePart(part.part_id).setAttribute(part.attribute, part.attribute_value);
	});
}


/**
 * @brief Removes concrete HTML attributes.
 * @param parts Validated DOM operation payloads using browser part IDs.
 * @pre None.
 * @post The named attributes are absent from existing targets.
 */
export function removeAttribute(
	parts: Array<ReceiveData_RemoveAttribute>
): void
{
	parts.forEach(part => {
		requirePart(part.part_id).removeAttribute(part.attribute);
	});
}


/**
 * @brief Adds CSS tokens to concrete DOM nodes.
 * @param parts Validated DOM operation payloads using browser part IDs.
 * @pre Each class name is one nonempty CSS token.
 * @post Existing target nodes contain the supplied class.
 */
export function addClass(
	parts: Array<ReceiveData_AddClass>
): void
{
	parts.forEach(part => {
		requirePart(part.part_id).classList.add(part.class_name);
	});
}


/**
 * @brief Removes CSS tokens from concrete DOM nodes.
 * @param parts Validated DOM operation payloads using browser part IDs.
 * @pre Each class name is one nonempty CSS token.
 * @post Existing target nodes no longer contain the supplied class.
 */
export function removeClass(
	parts: Array<ReceiveData_RemoveClass>
): void
{
	parts.forEach(part => {
		requirePart(part.part_id).classList.remove(part.class_name);
	});
}


/**
 * @brief Replaces concrete DOM text content.
 * @param parts Validated DOM operation payloads using browser part IDs.
 * @pre Replacing target descendants is intended.
 * @post Existing targets contain the supplied plain text.
 */
export function setText(
	parts: Array<ReceiveData_SetText>
): void
{
	parts.forEach(part => {
		const element = document.getElementById(part.part_id);
		if (!element) throw new Error("Text update references a missing DOM part");
		element.textContent = part.text;
	});
}


/**
 * @brief Updates the value of concrete browser form controls.
 * @param parts Validated DOM operation payloads using browser part IDs.
 * @pre Targets are inputs, text areas, or selects.
 * @post Supported targets have the new value; checkbox strings become booleans.
 */
export function setValue(parts: Array<ReceiveData_SetValue>): void
{
	for (const part of parts) {
		const element = document.getElementById(part.part_id);
		if (!element) throw new Error("Value update references a missing editor");
		applyValueUpdate(element, part);
	}
}


/**
 * @brief Replaces a native selector's choices and current selection atomically.
 * @param parts Validated option payloads using select editor part IDs.
 * @pre Target select elements have already been created.
 * @post Choices contain plain text and stable values; editor identity and focus are preserved.
 */
export function setSelectOptions(parts: Array<ReceiveData_SetSelectOptions>): void
{
	for (const part of parts) {
		const element = document.getElementById(part.part_id);
		if (!(element instanceof HTMLSelectElement)) {
			throw new Error("Choice update references a missing select");
		}

		const selected = prepareChoiceUpdate(element, part, part.options);
		if (!selected) continue;
		const choices = [{ value: "", label: part.placeholder, disabled: false }, ...part.options];
		const options = choices.map(choice => {
			const option = document.createElement("option");
			option.value = choice.value;
			option.textContent = choice.label;
			option.disabled = choice.disabled;
			return option;
		});
		element.replaceChildren(...options);
		element.value = selected.value;
		if (selected.edit) sendToServer({ op: "on_change", data: { part_id: element.id, value: selected.value, ...selected.edit } });
	}
}


/**
 * @brief Opens or closes concrete HTML dialogs.
 * @param parts Validated DOM operation payloads using browser part IDs.
 * @pre Targets are connected HTML dialog nodes.
 * @post Supported targets have the requested modal state.
 */
export function setModalOpen(
	parts: Array<ReceiveData_SetModalOpen>
): void
{
	parts.forEach(part => {
		const element = document.getElementById(part.part_id);

		if (!element) {
			throw new Error("Dialog update references a missing part");
		}

		if (!(element instanceof HTMLDialogElement)) {
			throw new Error("Dialog update targets another element type");
		}

		element.dataset.uiModalOpen = String(part.open);

		if (part.open) {
			if (!element.open) {
				element.showModal();
			}
		}
		else {
			if (element.open) {
				element.close();
			}
		}
	});
}


/**
 * @brief Marks concrete DOM nodes as logical click event sources.
 * @param parts Validated DOM operation payloads using browser part IDs.
 * @pre Targets exist and their logical owners accept clicks.
 * @post Event delegation can identify the marked parts.
 */
export function setOnClick(
	parts: Array<ReceiveData_SetOnClick>
): void
{
	parts.forEach(part => {
		const element = document.getElementById(part.part_id);

		if (element === null)
		{
			throw new Error("Event marker references a missing DOM part");
		}

		element.dataset.uiOnClick = "true";
	});
}


/**
 * @brief Marks concrete DOM nodes as logical change event sources.
 * @param parts Validated DOM operation payloads using browser part IDs.
 * @pre Targets exist and their logical owners accept changes.
 * @post Event delegation can identify the marked parts.
 */
export function setOnChange(
	parts: Array<ReceiveData_SetOnChange>
): void
{
	parts.forEach(part => {
		const element = document.getElementById(part.part_id);

		if (element === null)
		{
			throw new Error("Event marker references a missing DOM part");
		}

		element.dataset.uiOnChange = "true";
	});
}


/**
 * @brief Marks concrete DOM nodes as logical submit event sources.
 * @param parts Validated DOM operation payloads using browser part IDs.
 * @pre Targets exist and their logical owners accept submissions.
 * @post Event delegation can identify the marked parts.
 */
export function setOnSubmit(
	parts: Array<ReceiveData_SetOnSubmit>
): void
{
	parts.forEach(part => {
		const element = document.getElementById(part.part_id);

		if (element === null)
		{
			throw new Error("Event marker references a missing DOM part");
		}

		element.dataset.uiOnSubmit = "true";
	});
}




/**
 * @brief Replaces native radio choices inside an existing registered group.
 * @param parts Validated group choice payloads.
 * @pre Group fieldsets and their options containers have been created.
 * @post Choices use plain labels and group-local names; a surviving focused choice regains focus.
 */
export function setRadioOptions(parts: Array<ReceiveData_SetRadioOptions>): void
{
	for (const part of parts) {
		const group = document.getElementById(part.part_id);
		if (!(group instanceof HTMLFieldSetElement) || !group.hasAttribute("data-ui-radio-group")) throw new Error("Radio update targets an invalid group");
		const container = group.querySelector<HTMLElement>("[data-ui-radio-options]");
		if (!container) throw new Error("Radio group is missing its choices container");

		const selected = prepareChoiceUpdate(group, part, part.options);
		if (!selected) continue;
		const active = document.activeElement;
		const hadFocus = active instanceof HTMLInputElement && container.contains(active);
		const focusedValue = hadFocus ? active.value : null;
		const radios: HTMLInputElement[] = [];
		const choices = part.options.map(choice => {
			const label = document.createElement("label");
			const radio = document.createElement("input");
			radio.type = "radio";
			radio.name = group.id;
			radio.value = choice.value;
			radio.disabled = choice.disabled;
			radio.checked = choice.value === selected.value;
			const text = document.createElement("span");
			text.textContent = choice.label;
			label.append(radio, text);
			radios.push(radio);
			return label;
		});
		container.replaceChildren(...choices);
		if (selected.edit) sendToServer({ op: "on_change", data: { part_id: group.id, value: selected.value, ...selected.edit } });
		if (hadFocus) {
			const next = radios.find(radio => radio.value === focusedValue && !radio.disabled)
				?? radios.find(radio => !radio.disabled);
			(next ?? group).focus({ preventScroll: true });
		}
	}
}
