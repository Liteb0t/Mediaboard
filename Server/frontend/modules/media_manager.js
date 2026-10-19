import { global_styles } from "./styles.js";
import { Format, Media } from "./embed_media.js";
import { session } from "./session.js";
import { MultipleSelectorElement } from "./MultipleSelector.js";

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
			component.setFileCountLimit(4);
			component.file_upload.onchange = (event) => {
				console.log("file changed.");
				component.addFiles(component.file_upload.files, MediaManagerElement.upload_destination);
			}
		});
		MediaManagerElement.mountElementIfNotThere(this.multiple_selector_container, "multiple-selector").then(element => {
			this.multiple_selector_component = element;
			// element.registerSelectableQuery("[selectable]");
			element.noun = "file";
			element.registerVerb("Delete");
			element.render();
			element.addEventListener("action", this.selectionActionHandler);
			element.addEventListener("selectionstart", this.onSelectionStartHandler);
			element.addEventListener("selectionclear", this.onSelectionClearHandler);
		});
		this.getAllBoards();
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
		console.log(event.target);
		console.log(event.target.closest("select"));
		if (event.target.closest("select"))
			return this.render();
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
		try {
		console.log(event.detail.elements);
		if (event.detail.verb === "Delete") {
			let filenames = [];
			for (let media_embed of event.detail.elements) {
				filenames.push(media_embed.getAttribute("filename"));
				const response = await fetch("api/files", {method: "DELETE", body: JSON.stringify({files: filenames})});
				if (response.ok) {
					this.render();
				}
				else {
					throw response.headers.get("message") ?? `Error occured when attempting to delete files\n${response.status} ${response.statusText}`;
				}
			}
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
	async getAllBoards() { // DUPLICATED from board_list.js
		let response = await fetch("api/boards");
		console.log(response);
		if (response.ok) {
			await response.json().then(response_obj => {
				console.log(response_obj);
				this.showBoardOptions(response_obj);
			});
		}
		else {
			throw response.headers.get("message") ?? `An unknown error occured when attempting to fetch boards.\n${response.status} ${response.statusText}`;
		}
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

	async fetchJson(location, args, thing = "something") {
		const response = await fetch(location, args);
		if (response.ok) {
			const response_json = await response.json();
			return response_json;
		}
		else {
			const error_message = response.headers.get("message") ?? `An unknown error occured when attempting to fetch ${thing}.`;
			throw error_message;
		}
	}

	static valid_search_parameters = ["board"];
	async render() {
		this._abort_controller?.abort();
		this._abort_controller = new AbortController();
		const signal = this._abort_controller.signal;
		this.multiple_selector_component?.clearSelection();

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

		const params_string = params.toString();
		console.log("params_string: " + params_string);
		const files_json = await this.fetchJson(`api/files?${params_string}`, {signal: signal}, "files");
		if (signal.aborted)
			return;
		console.log(files_json);
		this.showAllFiles(files_json);
	}

	showAllFiles(files_json) {
		const fragment = new DocumentFragment();
		for (let file of files_json.files) {
			const embed = Media.createMediaEmbed(file);
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
