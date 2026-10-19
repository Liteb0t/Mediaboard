import { global_styles } from "./styles.js";

export class MultipleSelectorElement extends HTMLElement {
	noun = "noun";
	is_active = false;
	static selectable_query = "[selectable]";
	selected_elements = [];
	_verb_to_event = {};
	selection_start_event = new Event("selectionstart");
	selection_clear_event = new Event("selectionclear");
	constructor() {
		// Always call super first in constructor
		super();
		// const template = document.getElementById("file-upload-template");
		// const shadow = this.attachShadow({ mode: "open" });
		// shadow.adoptedStyleSheets = [global_styles]; // imported at the top there
		// shadow.appendChild(template.content.cloneNode(true));
  //
		// for (const element of shadow.querySelectorAll(["[id]"])) {
		// 	this[element.id] = element;
		// }
	}
	selectElement(element) {
		const element_index = this.selected_elements.indexOf(element);
		if (element_index === -1) {
			element.classList.add("Selected");
			this.selected_elements.push(element);
		}
		else {
			element.classList.remove("Selected");
			this.selected_elements.splice(element_index, 1);
		}
		this.updateSelectionIndicator();
		// if ([...event.dataTransfer.items].some((item) => item.kind === "file")) {
		// 	event.preventDefault();
		// }
	}
	render() {
		this.select_button = document.createElement("button");
		this.select_button.textContent = "Select";
		this.select_button.onclick = () => { this.beginSelection(); }
		this.appendChild(this.select_button);

		this.selection_indicator = document.createElement("span");
		this.selection_indicator.style["display"] = "none";
		this.appendChild(this.selection_indicator);

		this.action_button_container = document.createElement("div");
		this.action_button_container.style["display"] = "none";
		this.renderActionButtons();
		this.appendChild(this.action_button_container);

		this.clear_button = document.createElement("button");
		this.clear_button.textContent = "Cancel";
		this.clear_button.onclick = () => { this.clearSelection(); }
		this.clear_button.style["display"] = "none";
		this.appendChild(this.clear_button);

		return this;
	}
	renderActionButtons() {
		console.log("renderActionButtons called");
		console.log(Object.entries(this._verb_to_event));
		let fragment = new DocumentFragment();
		Object.entries(this._verb_to_event)
			.filter(([verb, detail]) => {return detail.enabled;})
			.forEach(([verb, detail]) => {
			console.log(verb);
			const verb_button = document.createElement("button");
			verb_button.textContent = verb;
			verb_button.onclick = () => { this.dispatchEventFromVerb(verb); }
			fragment.appendChild(verb_button);
		});
		this.action_button_container.replaceChildren(fragment);
	}
	updateSelectionIndicator() {
		this.selection_indicator.textContent = `${this.selected_elements.length} ${this.noun}` + (this.selected_elements.length !== 1 ? "s" : "") + " selected";
	}
	beginSelection() {
		this.updateSelectionIndicator();
		this.select_button.style["display"] = "none";
		this.selection_indicator.style["display"] = "";
		this.clear_button.style["display"] = "";
		this.action_button_container.style["display"] = "";
		this.is_active = true;
		this.dispatchEvent(this.selection_start_event);
	}
	clearSelection() {
		for (let element of this.selected_elements) {
			element.classList.remove("Selected");
		}
		this.selection_indicator.style["display"] = "none";
		this.clear_button.style["display"] = "none";
		this.select_button.style["display"] = "";
		this.selected_elements = [];
		this.action_button_container.style["display"] = "none";
		this.is_active = false;
		this.dispatchEvent(this.selection_clear_event);
	}
	setVerbs(verbs) {
		for (let verb of Object.keys(this._verb_to_event)) {
			if (verbs.indexOf(verb) === -1)
				this._verb_to_event[verb].enabled = false;
		}
		for (let verb of verbs) {
			if (!this._verb_to_event[verb]) {
				this.registerVerb(verb);
			}
			else {
				this._verb_to_event[verb].enabled = true;
			}
		}
		if (this.action_button_container) {
			this.clearSelection();
			this.renderActionButtons();
		}
	}
	registerVerb(verb) {
		this._verb_to_event[verb] = {
			enabled: true
		};
	}
	dispatchEventFromVerb(verb) {
		const doAction = new CustomEvent("action", {
			detail: {
				verb: verb
				,elements: [...this.selected_elements]
			}
		});
		this.dispatchEvent(doAction);
		this.clearSelection();
	}
	handleOnClick = (event) => {
		if (!this.is_active)
			return false;
		let selected_maybe = event.target.closest(MultipleSelectorElement.selectable_query);
		console.log(selected_maybe);
		if (selected_maybe) {
			this.selectElement(selected_maybe);
			event.preventDefault();
			return true;
		}
		else
			return false;
	}
// 	onClickEvent = (event) => {
// 		console.log(event.target);
//
// 	}
}

customElements.define("multiple-selector", MultipleSelectorElement);

