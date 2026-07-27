
import { sendToServer } from "./websocket.js";
import { getElementValue } from "./dom-ops.js";


export function registerBrowserEventHandlers(): void
{
	document.addEventListener("click", (event) => {
		const target = event.target;
		if (!(target instanceof Element)) return;

		const element = target.closest("[data-ui-on-click]");
		if (!element) return;

		sendToServer({
			op: "on_click",
			data: {
				part_id: element.id
			}
		});
	});


	document.addEventListener(
		"change",
		(event) => {
		const target = event.target;
		if (!(target instanceof Element)) return;

		const element = target.closest("[data-ui-on-change]");
		if (!element) return;

		const elementValue = getElementValue(element);
		if (elementValue === null)
		{
			console.warn("Element has no value");
			return;
		}

		sendToServer({
			op: "on_change",
			data: {
				part_id: element.id,
				value: elementValue
			}
		});
	});


	document.addEventListener(
		"submit",
		(event) => {
		const target = event.target;
		if (!(target instanceof Element)) return;

		const element = target.closest("[data-ui-on-submit]");
		if (!element) return;

		event.preventDefault();

		sendToServer({
			op: "on_submit",
			data: {
				part_id: element.id
			}
		});
	});
}
