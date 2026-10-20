import { global_styles } from "./styles.js";
import { Format, Media } from "./embed_media.js";
import { session } from "./session.js";
import { MultipleSelectorElement } from "./MultipleSelector.js";
import { FileUploadElement } from "./file_upload.js";
import { API } from "./API.js";

class MediaManagerElement extends HTMLElement {
	server_permissions = {};
	file_upload_component = null;
	multiple_selector_component = null;
	static upload_destination = "/api/upload";
	static select_all_boards = "(all)";

	constructor() {
		// Always call super first in constructor
		super();
		const template = document.getElementById("media-manager-template");
		const shadow = this.attachShadow({ mode: "open" });
		shadow.adoptedStyleSheets = [global_styles]; // imported at the top there
		shadow.appendChild(template.content.cloneNode(true));

		for (const element of shadow.querySelectorAll("[id]")) {
			this[element.id] = element;
		}

		this.file_upload_component_promise = MediaManagerElement.mountElementIfNotThere(this.file_upload_container, "file-upload"); // async. module FileUploadElement from file_upload.js
		this.file_upload_component_promise.then(component => {
			this.file_upload_component = component;
			// component.setFileCountLimit(4);
			component.file_upload.onchange = (event) => {
				console.log("file changed.");
				component.addFiles(component.file_upload.files, MediaManagerElement.upload_destination);
			}
		});
		customElements.whenDefined("multiple-selector").then(() => {
			const element = document.createElement("multiple-selector");
			this.filters_toolbar.appendChild(element);
			this.multiple_selector_component = element;
			element.noun = "file";
			element.setVerbs(["Delete"]);
			element.render();
			element.addEventListener("action", this.selectionActionHandler);
			element.addEventListener("selectionstart", this.onSelectionStartHandler);
			element.addEventListener("selectionclear", this.onSelectionClearHandler);
		});
		this.board_select.onchange = () => {this.render();}
		this.type_select.onchange = () => {
			this.render();
		}
	} // constructor()
	static async mountElementIfNotThere(container, element_name) {
		await customElements.whenDefined(element_name); // wait for component to be loaded and defined, since JS module deferred loading will process sooner than router resolution, or however the deer hits the fence
		// thread-list is custom element defined in board.js
		let element = view_container.querySelector(element_name);
		if (!element) {
			element = document.createElement(element_name);
			container.replaceChildren(element);
		}
		console.log(element);
		return element;
	}
	onclickHandler = (event) => {
		if (this.multiple_selector_component?.handleOnClick(event))
			return;
		Media.handleOnClick(event);
	}
	ondragoverhandler = (event) => { // on window
		FileUploadElement.onDragOverHandler();
	}
	// https://developer.mozilla.org/en-US/docs/Web/API/HTML_Drag_and_Drop_API/File_drag_and_drop
	ondrophandler = (event) => { // on window
		FileUploadElement.onDropHandler(this.file_upload_component, MediaManagerElement.upload_destination);
	}
	onSelectionStartHandler = () => { this.media_container.setAttribute("selectionactive", ""); }
	onSelectionClearHandler = () => {this.media_container.removeAttribute("selectionactive");}
	selectionActionHandler = async(event) => {
		console.log("selectionActionHandler fired");
		try {
			console.log(event.detail.elements);
			let filenames = [];
			for (let media_embed of event.detail.elements) {
				filenames.push(media_embed.getAttribute("filename"));
			}
			if (event.detail.verb === "Delete") {
				const response = await fetch("api/files?action=Delete", {method: "PATCH", body: JSON.stringify({files: filenames})});
				this.renderIfResponseOkay(response);
			}
			else if (event.detail.verb === "Restore") {
				const response = await fetch("api/files?action=Restore", {method: "PATCH", body: JSON.stringify({files: filenames})});
				this.renderIfResponseOkay(response);
			}
			else if (event.detail.verb === "Destroy") {
				this.confirm_destroy_dialog_number_of_files = filenames.length;
				this.confirm_destroy_dialog_confirm_button.onclick = async() => {
					const response = await fetch("api/files", {method: "DELETE", body: JSON.stringify({files: filenames})});
					this.renderIfResponseOkay(response);
					this.confirm_destroy_dialog.close();
				}
				this.confirm_destroy_dialog.showModal();
			}
			else {
				throw "Unrecognised verb " + event.detail.verb;
			}
		}
		catch (error) {
			console.error(error);
			error_message.textContent = error;
			error_dialog.showModal();
		}
	}
	renderIfResponseOkay(response) {
		try {
			if (response.ok) {
				this.render();
			}
			else {
				throw response.headers.get("message") ?? `Error occured when attempting to delete files\n${response.status} ${response.statusText}`;
			}
		}
		catch (error) {
			console.error(error);
			error_message.textContent = error;
			error_dialog.showModal();
		}
	}
	connectedCallback() {
		console.log("Custom element added to page.");
		session.addEventListener("change", () => { this.updateUploadContainer(session.server_permissions); });
		session.refresh();
		this.shadowRoot.addEventListener("click", this.onclickHandler);
		console.log(this);
		console.log(this.shadowRoot);
		window.addEventListener("drop", this.ondrophandler);
		window.addEventListener("dragover", this.ondragoverhandler);
	}

	disconnectedCallback() {
	console.log("Custom element removed from page.");
		this.shadowRoot.removeEventListener("click", this.onclickHandler);
		window.removeEventListener("drop", this.ondrophandler);
		window.removeEventListener("dragover", this.ondragoverhandler);
		// window.removeEventListener("selectionstart", this.onSelectionStartHandler);
		// window.removeEventListener("selectionstop", this.onSelectionStopHandler);
	}

	connectedMoveCallback() {
	console.log("Custom element moved with moveBefore()");
	}

	adoptedCallback() {
	console.log("Custom element moved to new page.");
	}

	closeWebsocket() {
		if (this.websocket) {
			this.websocket.close();
		}
		this.websocket = null;
	}

	// attributeChangedCallback(name, oldValue, newValue) {
	// console.log(`Attribute ${name} has changed.`);
	// }

	static valid_search_parameters = ["board"];
	async render(get_boards = false) {
		this._abort_controller?.abort();
		this._abort_controller = new AbortController();
		const signal = this._abort_controller.signal;
		// this.multiple_selector_component?.clearSelection();

		let params = new URLSearchParams(/*window.location.search*/);
		// params.forEach(param => {
		// 	if (MediaManagerElement.valid_search_parameters.indexOf(param) === -1) {
		// 		params.remove(param);
		// 	}
		// });
		if (this.board_select.value !== MediaManagerElement.select_all_boards) {
			params.set("board", this.board_select.value);
		}
		if (this.type_select.value !== "Active") {
			params.set("type", this.type_select.value);
		}
		if (params.get("type") == "deleted")
			this.multiple_selector_component?.setVerbs(["Restore", "Destroy"]);
		else
			this.multiple_selector_component?.setVerbs(["Delete"]);

		const params_string = params.toString();
		const url = new URL(window.location.href);
		url.search = params_string;
		history.pushState(null, null, url);
		console.log("params_string: " + params_string);
		if (get_boards) {
			await Promise.all([
				this.getAllBoards(signal),
				this.getAllFiles(params_string, signal)
			]);
		}
		else
			this.getAllFiles(params_string, signal)
	}

	async getAllBoards(signal) {
		const boards_json = await API.fetchJson("api/boards", {signal: signal}, "boards");
		if (signal.aborted)
			return;
		this.showBoardOptions(boards_json);
	}
	showBoardOptions(boards_json) {
		const fragment = new DocumentFragment();
		let option_element = document.createElement("option");
		option_element.value = MediaManagerElement.select_all_boards;
		option_element.textContent = MediaManagerElement.select_all_boards;
		fragment.appendChild(option_element);
		for (let board of boards_json.boards) {
			let option_element = document.createElement("option");
			option_element.value = board.slug;
			option_element.textContent = `${board.slug} - ${board.title}`;
			fragment.appendChild(option_element);
		}
		this.board_select.replaceChildren(fragment);
	}
	async getAllFiles(params_string, signal) {
		const files_json = await API.fetchJson(`api/files?${params_string}`, {signal: signal}, "files");
		if (signal.aborted)
			return;
		this.showAllFiles(files_json);
	}
	showAllFiles(files_json) {
		const fragment = new DocumentFragment();
		for (let file of files_json.files) {
			const embed = document.createElement("media-embed").render(file, false);
			embed.setAttribute("selectable", "");
			fragment.appendChild(embed);
		}
		if (fragment.children.length === 0)
			this.media_container.replaceChildren(document.createTextNode("None."));
		else
			this.media_container.replaceChildren(fragment);
	}
	updateUploadContainer(permissions) {
		console.log("updateUploadContainer called");
		console.log(permissions);
		if (window.url_params_display_as_embed) {
			this.upload_container.style["display"] = "none";
			return;
		}
		if (permissions["upload_file"]) {
			this.upload_container.style["display"] = "";
		}
		else {
			this.upload_container.style["display"] = "none";
		}
		this.file_upload_component_promise.then(element => {element.updateVisibilityByPermissions(permissions);});
	}
}
customElements.define("media-manager", MediaManagerElement);
