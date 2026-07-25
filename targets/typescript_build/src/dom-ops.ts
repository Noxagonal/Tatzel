
import { stringToBoolean } from "./utilities.js";
import { DomPart } from "./protocol.js"

export function createElement(
	parts: Array<DomPart>
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


export function removeElement(id: string): void
{
	document.getElementById(id)?.remove();
}


export function setAttribute(
	id: string,
	attribute: string,
	value: string
): void
{
	if (attribute.toLowerCase().startsWith("on")) {
		console.warn("Blocked event-handler attribute:", attribute);
		return;
	}

	document.getElementById(id)?.setAttribute(attribute, value);
}


export function removeAttribute(
	id: string,
	attribute: string
): void
{
	document.getElementById(id)?.removeAttribute(attribute);
}


export function addClass(
	id: string,
	className: string
): void
{
	document.getElementById(id)?.classList.add(className);
}


export function removeClass(
	id: string,
	className: string
): void
{
	document.getElementById(id)?.classList.remove(className);
}


export function setText(
	id: string,
	text: string
): void
{
	const element = document.getElementById(id);
	if (element !== null) element.textContent = text;
}


export function setValue(
	id: string,
	value: string
): void
{
	const element = document.getElementById(id);

	if (element === null) {
		console.warn("setValue failed, element not found: ", id);
		return;
	}

	if (element instanceof HTMLInputElement) {
		if (element.type === "checkbox") {
			element.checked = stringToBoolean(value);
			return;
		}
		element.value = value;
		return;
	}

	if (element instanceof HTMLTextAreaElement) {
		element.value = value;
		return;
	}

	if (element instanceof HTMLSelectElement) {
		element.value = value;
		return;
	}

	console.warn("setValue called on unsupported element: ", element, " | While trying to assing value: ", value);
}


export function setModalOpen(
	id: string,
	open: boolean
): void
{
	const element = document.getElementById(id);

	if (!element) {
		console.warn("SetModalOpen failed, element not found:", id);
		return;
	}

	if (!(element instanceof HTMLDialogElement)) {
		console.warn("SetModalOpen failed, element is not a dialog:", id);
		return;
	}

	if (open) {
		if (!element.open) {
			element.showModal();
		}
	}
	else {
		if (element.open) {
			element.close();
		}
	}
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


export function setOnClick(id: string): void
{
	const element = document.getElementById(id);

	if (element === null)
	{
		console.error("Element not found:", id);
		return;
	}

	element.dataset.uiOnClick = "true";
}


export function setOnChange(id: string): void
{
	const element = document.getElementById(id);

	if (element === null)
	{
		console.error("Element not found:", id);
		return;
	}

	element.dataset.uiOnChange = "true";
}


export function setOnSubmit(id: string): void
{
	const element = document.getElementById(id);

	if (element === null)
	{
		console.error("Element not found:", id);
		return;
	}

	element.dataset.uiOnSubmit = "true";
}
