
import { stringToBoolean } from "./utilities.js";
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
} from "./protocol.js"



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
			console.error("Parent not found: ", part.parent_id);
			return;
		}

		const domElement = document.createElement(part.tag);

		domElement.id = part.part_id;
		domElement.className = part.classes.join(" ");
		for (const [key, value] of Object.entries(part.attributes))
		{
			if (!key.toLowerCase().startsWith("on")) domElement.setAttribute(key, value);
		}

		if (part.text_content !== "" ) domElement.textContent = part.text_content;

		domElementParent.appendChild(domElement);
	});

}


/**
 * @brief Removes concrete DOM nodes.
 * @param parts Validated DOM operation payloads using browser part IDs.
 * @pre None.
 * @post Existing target nodes and their DOM descendants are removed.
 */
export function removeElementPart(
	parts: Array<ReceiveData_RemoveElementPart>
): void
{
	parts.forEach(part => {
		document.getElementById(part.part_id)?.remove();
	});
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

		document.getElementById(part.part_id)?.setAttribute(part.attribute, part.attribute_value);
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
		document.getElementById(part.part_id)?.removeAttribute(part.attribute);
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
		document.getElementById(part.part_id)?.classList.add(part.class_name);
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
		document.getElementById(part.part_id)?.classList.remove(part.class_name);
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
		if (element !== null) element.textContent = part.text;
	});
}


/**
 * @brief Updates the value of concrete browser form controls.
 * @param parts Validated DOM operation payloads using browser part IDs.
 * @pre Targets are inputs, text areas, or selects.
 * @post Supported targets have the new value; checkbox strings become booleans.
 */
export function setValue(
	parts: Array<ReceiveData_SetValue>
): void
{
	parts.forEach(part => {
		const element = document.getElementById(part.part_id);

		if (element === null) {
			console.warn("setValue failed, element not found: ", part.part_id);
			return;
		}

		if (element instanceof HTMLInputElement) {
		if (element.type === "checkbox") {
			element.checked = stringToBoolean(part.value);
			return;
		}
		element.value = part.value;
		return;
		}

		if (element instanceof HTMLTextAreaElement) {
			element.value = part.value;
			return;
		}

		if (element instanceof HTMLSelectElement) {
			element.value = part.value;
			return;
		}

		console.warn("setValue called on unsupported element: ", element, " | While trying to assign value: ", part.value);
	});
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
			console.warn("SetModalOpen failed, element not found:", part.part_id);
			return;
		}

		if (!(element instanceof HTMLDialogElement)) {
			console.warn("SetModalOpen failed, element is not a dialog:", part.part_id);
			return;
		}

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
			console.error("Element not found:", part.part_id);
			return;
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
			console.error("Element not found:", part.part_id);
			return;
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
			console.error("Element not found:", part.part_id);
			return;
		}

		element.dataset.uiOnSubmit = "true";
	});
}



/**
 * @brief Reads a concrete browser form control value.
 * @param element Concrete DOM form control.
 * @returns Control value, or null for unsupported nodes.
 * @pre None.
 * @post The DOM is unchanged.
 */
export function getElementValue(element: Element): null | string | boolean
{
	if (element instanceof HTMLInputElement) {
		if (element.type === "checkbox") return element.checked;

		return element.value;
	}

	if (element instanceof HTMLTextAreaElement) {
		return element.value;
	}

	if (element instanceof HTMLSelectElement) {
		return element.value;
	}

	console.warn("getElementValue called on unsupported element: ", element);
	return null;
}
