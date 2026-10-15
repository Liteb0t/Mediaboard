import { Format, Media } from "./embed_media.js";

class MediaManagerElement extends HTMLElement {
	server_permissions = {};
	file_upload_component = null;

	constructor() {
		// Always call super first in constructor
		super();
		const template = document.getElementById("media-manager-template");
		const shadow = this.attachShadow({ mode: "open" });
		shadow.adoptedStyleSheets = [global_styles]; // imported at the top there
		shadow.appendChild(template.content.cloneNode(true));

		// TODO explicitly map elements post_container etc
		for (const element of shadow.querySelectorAll("[id]")) {
			this[element.id] = element;
		}

		this.file_upload_component_promise = this.mountFileUploadIfNotThere(); // async. module FileUploadElement from file_upload.js
		this.file_upload_component_promise.then(component => {
			this.file_upload_component = component;
			component.setFileCountLimit(4);
			component.file_upload.onchange = (event) => {
				console.log("file changed.");
				component.addFiles(component.file_upload.files, this.buildUploadDestination(this.selected_board.slug, this.selected_thread.id));
			}
		});
	} // constructor()
	async mountFileUploadIfNotThere(data) {
		await customElements.whenDefined("file-upload"); // wait for component to be loaded and defined, since JS module deferred loading will process sooner than router resolution, or however the deer hits the fence
		// thread-list is custom element defined in board.js
		let element = view_container.querySelector("file-upload");
		if (!element) {
			element = document.createElement("file-upload");
			this.file_upload_container.replaceChildren(element);
		}
		console.log(element);
		return element;
	}
	onclickHandler = (event) => {
		let target = event.target;
		console.log(target);
		if (target.classList.contains("EmbedThumbnail"))
			ThreadListElement.expandEmbedFigure(target.parentNode);
		else if (target.classList.contains("EmbedImage"))
			ThreadListElement.contractEmbedFigure(target.parentNode);
		else if (target.classList.contains("EmbedIframe"))
			ThreadListElement.toggleEmbedIframe(target.parentNode);
	}
	ondragoverhandler = (event) => { // on window
		if ([...event.dataTransfer.items].some((item) => item.kind === "file")) {
			event.preventDefault();
		}
	}
	// https://developer.mozilla.org/en-US/docs/Web/API/HTML_Drag_and_Drop_API/File_drag_and_drop
	ondrophandler = (event) => { // on window
		if ([...event.dataTransfer.items].some((item) => item.kind === "file")) {
			event.preventDefault();
			const files = [...event.dataTransfer.items]
				.map((item) => item.getAsFile()) // https://developer.mozilla.org/en-US/docs/Web/API/DataTransferItem/getAsFile
				.filter((file) => file);
			this.file_upload_component?.addFiles(files, this.buildUploadDestination(this.selected_board.slug, this.selected_thread.id));
			console.log(files);
		}
	}
	buildUploadDestination(board_slug, thread_id = -1) {
		return `./api/board/${board_slug}/` + (thread_id > -1 ? `thread/${thread_id}/` : "") + "upload/";
	}
	static toggleEmbedIframe(figure_element) {
		let iframe_element = figure_element.querySelector("iframe");
		if (figure_element.getAttribute("expanded") === "") {
			iframe_element.style["display"]="none";
			figure_element.removeAttribute("expanded");
			iframe_element.src = "";
		}
		else {
			iframe_element.style["display"]="inherit";
			figure_element.setAttribute("expanded", "");
			iframe_element.src = figure_element.getAttribute("data-iframe-src");
		}
	}
	static expandEmbedFigure(figure_element) {
		figure_element.children[ThreadListElement.FIGURE_EMBED].setAttribute("src", Media.media_url + figure_element.getAttribute("filename"));
		figure_element.children[ThreadListElement.FIGURE_EMBED].style["display"] = "inherit";
		figure_element.children[ThreadListElement.FIGURE_THUMBNAIL].style["display"] = "none";
	}
	static contractEmbedFigure(figure_element) {
		figure_element.children[ThreadListElement.FIGURE_EMBED].style["display"] = "none";
		figure_element.children[ThreadListElement.FIGURE_THUMBNAIL].style["display"] = "inherit";
	}
	connectedCallback() {
		console.log("Custom element added to page.");
		this.post_button.onclick = () => this.postAction();
		// session.addEventListener("change", this.updateReplyContainerAuto);
		session.refresh(); // fetches client info asynchronously
// threads_button.onclick = () => { openThreadList(); } // included in navbar.html
// refresh_button.onclick = () => {refresh_send();} // imported from navbar.html
		this.shadowRoot.addEventListener("click", this.onclickHandler);
		console.log(this);
		console.log(this.shadowRoot);
		window.addEventListener("drop", this.ondrophandler);
		window.addEventListener("dragover", this.ondragoverhandler);
	}

	disconnectedCallback() {
	console.log("Custom element removed from page.");
		this.selected_thread.id = -1;
		this.closeWebsocket();
		// session.removeEventListener("change", this.updateReplyContainerAuto);
		this.shadowRoot.removeEventListener("click", this.onclickHandler);
		window.removeEventListener("drop", this.ondrophandler);
		window.removeEventListener("dragover", this.ondragoverhandler);
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

	async render(board_slug, thread_id = -1) {
		this._abort_controller?.abort();
		this._abort_controller = new AbortController();
		const signal = this._abort_controller.signal;

		const files_json = await this.fetchJson(`api/files`, {signal: signal}, "files");
		if (signal.aborted)
			return;
		this.showAllFiles(files_json);
	}

	showAllFiles(board_slug, files_json) {
		const fragment = new DocumentFragment();
		for (let file of files_json.files) {
			fragment.appendChild(Media.createMediaEmbed(file));
		}
		if (fragment.children.length === 0)
			this.post_container.replaceChildren(document.createTextNode("None."));
		else
			this.post_container.replaceChildren(fragment);
	}
}
customElements.define("media-manager", MediaManagerElement);
