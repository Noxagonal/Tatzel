
import { stringToBoolean } from "./utilities.js";
import {
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
	SendData_OnSubmit
} from "./protocol.js"



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
			domElement.setAttribute(key, value);
		}

		if (part.text_content !== "" ) domElement.textContent = part.text_content;

		domElementParent.appendChild(domElement);
	});

}


export function removeElementPart(
	parts: Array<ReceiveData_RemoveElementPart>
): void
{
	parts.forEach(part => {
		document.getElementById(part.part_id)?.remove();
	});
}


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


export function removeAttribute(
	parts: Array<ReceiveData_RemoveAttribute>
): void
{
	parts.forEach(part => {
		document.getElementById(part.part_id)?.removeAttribute(part.attribute);
	});
}


export function addClass(
	parts: Array<ReceiveData_AddClass>
): void
{
	parts.forEach(part => {
		document.getElementById(part.part_id)?.classList.add(part.class_name);
	});
}


export function removeClass(
	parts: Array<ReceiveData_RemoveClass>
): void
{
	parts.forEach(part => {
		document.getElementById(part.part_id)?.classList.remove(part.class_name);
	});
}


export function setText(
	parts: Array<ReceiveData_SetText>
): void
{
	parts.forEach(part => {
		const element = document.getElementById(part.part_id);
		if (element !== null) element.textContent = part.text;
	});
}


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

		console.warn("setValue called on unsupported element: ", element, " | While trying to assing value: ", part.value);
	});
}


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
