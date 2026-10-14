import { global_styles } from "./styles.js";
import { session } from "./session.js";
import { UploadedFile } from "./file_upload.js";

class Format {
	static IMAGE = 0;
	static AUDIO = 1;
	static VIDEO = 2;
	static OTHER = -1;
	static image_formats = ["avif", "gif", "jpe", "jpeg", "jpg", "jxl", "png", "heic", "webp"];
	static audio_formats = ["aac", "flac", "m4a", "mid", "midi", "mp3", "oga", "ogg", "ogx", "opus", "wav"];
	static video_formats = ["mp4", "webm"];
	static fromFileName(file_name) {
		let dot_index = file_name.lastIndexOf('.');
		if (dot_index !== -1) {
			let file_extension = file_name.substring(dot_index+1).toLowerCase();
			if (Format.image_formats.indexOf(file_extension) !== -1) {
				return Format.IMAGE;
			}
			else if (Format.audio_formats.indexOf(file_extension) !== -1) {
				return Format.AUDIO;
			}
			else if (Format.video_formats.indexOf(file_extension) !== -1) {
				return Format.VIDEO;
			}
			else {
				return Format.OTHER;
			}
		}
		else {
			return Format.OTHER;
		}
	}
}


// https://developer.mozilla.org/en-US/docs/Web/API/Web_components/Using_custom_elements#custom_element_lifecycle_callbacks
// Create a class for the element
class ThreadListElement extends HTMLElement {
	// static observedAttributes = ["color", "size"];
	// board_slug = null;
	// thread_id = null;
	selected_board = {
		slug: null,
		title: null,
		permissions: {}
	};
	selected_thread = {
		id: -1,
		permissions: {}
	};
	server_permissions = {};

	thumbnail_url = "./media/thumbnails/";
	websocket_url = (window.location.protocol === "http:" ? "ws://" : "wss://") + window.location.host + window.location.pathname;
	media_uuid_len = 36;
	thumbnail_size = CONFIG_thumbnail_size;
	// localDate = new Date();
	// timezone_offset_seconds = localDate.getTimezoneOffset() * 60;
	// flex_separator = document.createElement("div").classList.add("FlexSeparator");
	websocket = null; // set in getThread()
	websocket_connected = false;
	content_field_text_encoder = new TextEncoder();
	lock_scroll_to_bottom = false;
	scrolled_to_bottom_observer = null;
	file_upload_component = null;
	static FIGURE_EMBED = 0; // Main <img> or <video> is child 0 of <figure>, thumbnail is child 1.
	static FIGURE_THUMBNAIL = 1;

	constructor() {
		// Always call super first in constructor
		super();
		const template = document.getElementById("thread-list-template");
		const shadow = this.attachShadow({ mode: "open" });
		shadow.adoptedStyleSheets = [global_styles]; // imported at the top there
		shadow.appendChild(template.content.cloneNode(true));

		// TODO explicitly map elements post_container etc
		for (const element of shadow.querySelectorAll("[id]")) {
			this[element.id] = element;
		}


		// The value in the name field is set to the name used in the most recent post, if any.
		this.name_field.value = localStorage.getItem("name");

		this.content_field.oninput = () => {
			// Remove leftover <br> tag from contenteditable so the 'Content' placeholder appears again
			if (this.content_field.childElementCount === 1 && this.content_field.children[0].tagName === "BR" && this.content_field.textContent.length === 0) {
				this.content_field.removeChild(this.content_field.children[0]);
			}

			let content_field_length = (this.content_field_text_encoder.encode(this.content_field.innerText)).length;
			if (content_field_length > Math.round(CONFIG_post_max_content * 0.8)) {
				if (this.content_length_indicator.style["display"] === "none") {
					this.content_length_indicator.style["display"] = "inherit";
				}
				this.content_length_indicator.textContent = `${content_field_length}/${CONFIG_post_max_content}`;
				if (content_field_length > CONFIG_post_max_content) {
					if (!this.content_length_indicator.classList.contains("LimitExceeded")) {
						this.content_length_indicator.classList.add("LimitExceeded");
					}
				}
				else {
					if (this.content_length_indicator.classList.contains("LimitExceeded")) {
						this.content_length_indicator.classList.remove("LimitExceeded");
					}
				}
			}
			else {
				if (this.content_length_indicator.style["display"] !== "none") {
					this.content_length_indicator.style["display"] = "none";
				}
			}
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

		// "Scroll-lock" feature - when a user is scrolled to the bottom of the messages list, adding content in the reply container will push the messages up so that the latest messages are not obstructed.
		const reply_container_resize_observer = new ResizeObserver(() => {
			if (this.lock_scroll_to_bottom === true) {
				this.bottom_marker.scrollIntoView(false);
			}
		});
		reply_container_resize_observer.observe(this.reply_container);
		const options = {
			// root: document.body,
			rootMargin: "5px",
			scrollMargin: "5px",
			threshold: 0.5,
		};
		const callback = (entries, observer) => {
			for (let entry of entries) {
				if (entry.target === this.bottom_marker && this.selected_thread.id > -1) {
					this.lock_scroll_to_bottom = entry.isIntersecting;
				}
			}

			console.log(entries);
		}
		this.scrolled_to_bottom_observer = new IntersectionObserver(callback, options);

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
		figure_element.children[ThreadListElement.FIGURE_EMBED].setAttribute("src", UploadedFile.media_url + figure_element.getAttribute("filename"));
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

		this.selected_board.slug = board_slug;
		this.selected_thread.id = thread_id;
		this.closeWebsocket();
		if (thread_id === -1) {
			// could use Promise.allSettled
			// https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Promise/allSettled
			await Promise.all([
				this.getBoardInfoAndUpdateReplyContainer(board_slug, signal),
				this.getThreadList(board_slug, signal)
			]);
		}
		else {
			const thread_json = await this.fetchJson(`api/board/${board_slug}/thread/${thread_id}`, {signal: signal}, "thread");
			if (signal.aborted)
				return;
			this.showThread(board_slug, thread_id, thread_json);
			this.websocket = this.wsListenToThread(board_slug, thread_id, signal);
		}
	}
	async getBoardInfoAndUpdateReplyContainer(board_slug, signal) {
		const board_json = await this.fetchJson(`api/board/${board_slug}`, {signal: signal}, "board info");
		if (signal.aborted)
			return;
		this.selected_board.permissions = board_json["client_permissions"];
		this.updateReplyContainer(-1, this.selected_board.permissions);
		this.selected_board.title = board_json["title"];
		this.updateLocationIndicatorAndBackButton(board_slug, this.selected_board.title, this.selected_thread.id);
	}
	updateLocationIndicatorAndBackButton(board_slug, board_title, thread_id) {
		if (thread_id === -1) {
			this.location_indicator.textContent = `Threads in ${board_slug} - ${this.selected_board.title}`;
			this.location_indicator.style["display"] = "inherit";
			this.back_to_threads_button.style["display"] = "none";
		}
		else {
			this.back_to_threads_button.href = ThreadListElement.makeLink(board_slug);
			this.back_to_threads_button.style["display"] = "inherit";
			this.location_indicator.style["display"] = "none";
		}
	}

	async getThreadList(board_slug, signal) {
		const threads_json = await this.fetchJson(`api/board/${board_slug}/threads`, {signal: signal}, "threads")
		console.log(threads_json);
		if (signal.aborted)
			return;
		this.showAllThreads(board_slug, threads_json);
	}

	showAllThreads(board_slug, message_json) {
		// threads_button.disabled = true;

		const fragment = new DocumentFragment();
		for (let thread of message_json.threads) {
			// console.log(thread.id);
			// console.log(thread.post_zero);
			let post_zero_element = this.createPost(board_slug, -1, thread.post_zero, {/*...this.selected_board.permissions,*/ ...thread.client_permissions});
			if (window.url_params_display_as_embed) {
				fragment.appendChild(post_zero_element);
			}
			else {
				let thread_element = document.createElement("div");
				thread_element.classList.add("Thread");
				thread_element.appendChild(post_zero_element);
				// Toolbar at the bottom of each thread
				let thread_toolbar = document.createElement("div");
				thread_toolbar.classList.add("ToolBar", "WithBorders");

				const reply_count_container = document.createElement("div");
				const reply_count = document.createElement("span");
				reply_count.textContent = (thread["reply_count"] > 0 ? thread["reply_count"] : "No") + " repl" + (thread["reply_count"] === 1 ? "y" : "ies");
				reply_count_container.appendChild(reply_count);

				thread_toolbar.append(reply_count_container, window.flex_separator.cloneNode());

				if (thread.client_permissions["manage_permissions"]) {
					let manage_thread_permissions_button = document.createElement("a");
					manage_thread_permissions_button.classList.add("Button", "ToolBarButton");
					manage_thread_permissions_button.textContent = "Manage permissions";
					manage_thread_permissions_button.setAttribute("href", `./manage_permissions.html?location=/board/${board_slug}/thread/${thread.id}/`);
					thread_toolbar.append(manage_thread_permissions_button);
				}

				let open_thread_link = document.createElement("a");
				open_thread_link.classList.add("Button", "ToolBarButton");
				open_thread_link.textContent = "\u21E8 Open thread";
				open_thread_link.href = ThreadListElement.makeLink(board_slug, thread.id);

				thread_toolbar.append(open_thread_link);
				thread_element.appendChild(thread_toolbar);
				fragment.appendChild(thread_element);
			}
		}
		if (fragment.children.length === 0)
			this.post_container.replaceChildren(document.createTextNode("None."));
		else
			this.post_container.replaceChildren(fragment);
	}

	static makeLink(board_slug, thread_id = -1) {
		if (thread_id === -1)
			return `#/board/${board_slug}`;
		else
			return `#/board/${board_slug}/thread/${thread_id}`;
	}
	openThread(board_slug, thread_id) {
		this.router.navigate(`#/board/${board_slug}/thread/${thread_id}`);
		// this.selected_thread.id = thread_id;
		// TODO use url_params.toString() if need bee
		// history.pushState(state, "", window.location.pathname + `?board=${board_slug}&thread=${thread_id}`);
		// this.getThread(board_slug, thread_id);
	}

	showThread(board_slug, thread_id, thread_json) {
		// this.location_indicator.textContent = `Viewing thread ${thread_id}`;
		// threads_button.disabled = false;
		this.selected_thread.permissions = thread_json["client_permissions"]; // Needed for websocket posts coming in

		this.updateLocationIndicatorAndBackButton(board_slug, this.selected_board.title, thread_id);
		this.updateReplyContainer(thread_id, thread_json["client_permissions"]);
		// this.selected_thread.client_permissions = thread_json["client_permissions"];
		// header_bar.scrollIntoView(false); // Scroll back to top TODO find another way!
		const fragment = new DocumentFragment();
		for (let message of thread_json["messages"]) {
			fragment.appendChild(this.createPost(board_slug, thread_id, message, thread_json["client_permissions"]));
		}
		this.post_container.replaceChildren(fragment);
		this.scrolled_to_bottom_observer.observe(this.bottom_marker);
	}

	createPost(board_slug, selected_thread_id, data, client_permissions) {
		console.log(data);
		let post_box_element = document.createElement("article");
		post_box_element.classList.add("Post");

		let post_header_element
		if (!window.url_params_display_as_embed) {
			post_header_element = document.createElement("div");
			// post_header_element.classList.add("PostHeader");
			post_header_element.classList.add("ToolBar");

			let post_name_element = document.createElement("b");
			post_name_element.classList.add("Username");
			post_name_element.textContent = data.name;
			post_header_element.appendChild(post_name_element);

			if (data.highest_ranked_group_name) {
				let user_rank_element = document.createElement("span");
				user_rank_element.classList.add("UserRank");
				user_rank_element.textContent = data.highest_ranked_group_name;
				post_header_element.appendChild(user_rank_element);
			}

			let post_timestamp_element = document.createElement("time");
			let post_date = new Date(data["created_at"] * 1000 - window.timezone_offset_seconds * 1000).toISOString();;
			post_date = post_date.substring(0, 10) + " " + post_date.substring(11, 19);
			post_timestamp_element.textContent = post_date;
			post_timestamp_element.classList.add("Timestamp");

			let post_id_element = document.createElement("span");
			post_id_element.classList.add("ID");
			let post_id;
			if (selected_thread_id > -1) {
				post_id = `#${data.thread_id}/${data.id_in_thread}`;
			}
			else {
				post_id = "#" + data.thread_id;
			}
			post_id_element.textContent = post_id;

			const right_aligned_toolbar_group = document.createElement("div");
			right_aligned_toolbar_group.classList.add("ToolBar", "RightAligned");

			// Delete button
			if (client_permissions["delete_post"] || data.is_author) {
				let delete_button_element = document.createElement("button");
				delete_button_element.textContent = "\u{1F5D1} Delete";
				delete_button_element.onclick = async() => {
					let server_response = await this.deleteMessage(board_slug, data.thread_id, data.id_in_thread);
					console.log(server_response);
					if (server_response.ok) {
						post_box_element.classList.add("Deleted");
						delete_button_element.disabled = true;
					}
				}
				// post_header_element.appendChild(delete_button_element);
				right_aligned_toolbar_group.appendChild(delete_button_element);
			}
			// Reply button
			if (selected_thread_id > -1 && client_permissions["send_message"]) {
				let reply_button = document.createElement("button");
				reply_button.style["margin-left"] = "0.5em";
				reply_button.textContent = "\u{2BA9} Reply";
				reply_button.onclick = () => {
					this.reply_container.open = true;

					let reply_link_node = document.createTextNode(post_id + " ");
					// Add reply tag at caret position, if focused on content_field
					let range, selection;
					selection = window.getSelection();
					if (this.content_field.contains(selection.focusNode)) {
						range = selection.getRangeAt(0);
						// console.log(range);
						range.insertNode(reply_link_node);
					}
					else
						this.content_field.append(reply_link_node);

					// Move caret position to end of reply tag
					range = document.createRange();
					// console.log(reply_link_node);
					range.setStartAfter(reply_link_node);
					range.collapse(true);
					selection.removeAllRanges();
					selection.addRange(range);
					// this.content_field.focus();
				}
				right_aligned_toolbar_group.appendChild(reply_button);
			}
			right_aligned_toolbar_group.append(post_id_element, post_timestamp_element);
			post_header_element.append(right_aligned_toolbar_group);
		}

		let post_content_element = document.createElement("div");
		post_content_element.classList.add("Content");
		if (data.contains_html) {
			const doc = new DOMParser().parseFromString(data.content, 'text/html');
			console.log(doc.body.childNodes);
			post_content_element.append(...doc.body.childNodes);
		}
		else
			post_content_element.appendChild(document.createTextNode(data.content));
		// Highlight reply links by wrapping them in a <span> element
		// const message_content_split = data.content.split(/#\d*\/(\d+)\b/g);
		// for (let i = 0; i < message_content_split.length; i+=2) {
		// 	post_content_element.appendChild(document.createTextNode(message_content_split[i]));
		// 	if (i+1 !== message_content_split.length) {
		// 		let reply_link = document.createElement("span");
		// 		reply_link.classList.add("PostLink");
		// 		reply_link.textContent = `#${data.thread_id}/${message_content_split[i+1]}`;
		// 		post_content_element.appendChild(reply_link);
		// 	}
		// }

		let post_files_element = document.createElement("div");
		post_files_element.classList.add("Files");
		for (const file of data.files) {
			let figure_element = document.createElement("figure");
			figure_element.classList.add("EmbedFigure");
			figure_element.setAttribute("filename", file["filename"].replace(" ", "%20"));
			let filename_extension_i = file["filename"].lastIndexOf(".");
			let file_format = Format.fromFileName(file["filename"]); // Format enum
			if (file.thumbnail_file_extension) {
				if (file_format === Format.IMAGE) {
					let img_element = document.createElement("img");
					img_element.classList.add("EmbedImage");
					if (file["width"] && file["height"]) {
						img_element.width = file["width"];
						img_element.height = file["height"];
					}
					img_element.style["display"] = "none";
					figure_element.appendChild(img_element);
				}
				else if (file_format === Format.VIDEO) {
					let video_element = document.createElement("video");
					video_element.classList.add("EmbedVideo");
					video_element.controls = true;
					video_element.disablePictureInPicture = true;
					video_element.setAttribute("controlslist", "nofullscreen nodownload");
					video_element.style["display"] = "none";
					figure_element.appendChild(video_element);
				}

				// Thumbnail
				let thumbnail_element = document.createElement("img");
				thumbnail_element.classList.add("EmbedThumbnail");
				let thumbnail_filename = file.thumbnail_file_extension ? `THUMBNAIL_${file.filename}.${file.thumbnail_file_extension}` : "THUMBNAIL_" + file["filename"] + ".CONFIG_thumbnail_file_extension";
				thumbnail_element.setAttribute("src", this.thumbnail_url + thumbnail_filename.replace(" ", "%20"));
				if (file["width"] && file["height"]) {
					if (file["width"] < file["height"]) {
						thumbnail_element.width = file["width"] < file["height"]>>1 ? Math.ceil(this.thumbnail_size / 2) : Math.ceil(this.thumbnail_size * (file["width"]/file["height"]));
						thumbnail_element.height = this.thumbnail_size;
					}
					else if (file["height"] < file["width"]) {
						thumbnail_element.width = this.thumbnail_size;
						thumbnail_element.height = file["height"] < file["width"]>>1 ? Math.ceil(this.thumbnail_size / 2) : Math.ceil(this.thumbnail_size * (file["height"]/file["width"]));
					}
					else {
						thumbnail_element.width = this.thumbnail_size;
						thumbnail_element.height = this.thumbnail_size;
					}
				}
				else { // Legacy: images uploaded before 0.1.1 don't have width and height saved
					thumbnail_element.width = thumbnail_element.height = this.thumbnail_size;
				}
				thumbnail_element.setAttribute("loading", "lazy");
				figure_element.append(thumbnail_element);
			}
			else if (file_format === Format.AUDIO) {
				let audio_element = document.createElement("audio");
				audio_element.controls = true;
				audio_element.setAttribute("controlslist", "nofullscreen nodownload");
				audio_element.setAttribute("src", UploadedFile.media_url + file["filename"].replace(" ", "%20"));
				figure_element.appendChild(audio_element);
			}
			let download_filename, caption_filename;
			if (filename_extension_i == -1) {
				download_filename = file["filename"].slice(0, file["filename"].length - this.media_uuid_len);
				caption_filename = download_filename.length < 32 ? download_filename : (download_filename.substr(0, 28) + "(...)");
			}
			else {
				download_filename = file["filename"].slice(0, filename_extension_i - this.media_uuid_len);
				caption_filename = download_filename.length < 32 ? download_filename : (download_filename.substr(0, 28) + "(...)");
				download_filename += file["filename"].slice(filename_extension_i, file["filename"].length);
				caption_filename += file["filename"].slice(filename_extension_i, file["filename"].length);
			}
			let figcaption_element = document.createElement("figcaption");
			let media_src = UploadedFile.media_url + file["filename"].replace(" ", "%20");
			let figcaption_element_a = document.createElement("a");
			figcaption_element_a.setAttribute("href", media_src);
			figcaption_element_a.setAttribute("target", "_blank");
			figcaption_element_a.textContent = caption_filename;
			if (caption_filename.length !== download_filename.length)
				figcaption_element_a.title = download_filename;

			let file_download_button = document.createElement("a");
			file_download_button.setAttribute("href", media_src);
			file_download_button.setAttribute("download", download_filename);
			file_download_button.setAttribute("title", "Download");
			file_download_button.innerText = "\ud83d\udce5\ufe0e";

			figcaption_element.append(figcaption_element_a, document.createTextNode("\u00A0"), file_download_button);
			figure_element.appendChild(figcaption_element);
			post_files_element.appendChild(figure_element);
		}

		if (!window.url_params_display_as_embed) {
			post_box_element.appendChild(post_header_element);
		}
		post_box_element.appendChild(post_content_element);
		post_box_element.appendChild(post_files_element);
		// this.post_container_element.appendChild(post_box_element);
		return post_box_element;
	}

	/*
	updateReplyContainerAuto() {
		if (this.selected_thread.id === -1 && this.selected_board.permissions !== {}) {
			this.updateReplyContainer(-1, this.selected_board.permissions);
		}
		else if (this.selected_thread.permissions !== {}) {
			this.updateReplyContainer(this.selected_thread.id, this.selected_thread.permissions);
		}
	}
	*/
	updateReplyContainer(selected_thread_id, permissions) {
		console.log("updateReplyContainer called");
		console.log("with selected thread ID " + selected_thread_id + " and permissions below");
		console.log(permissions);
		if (window.url_params_display_as_embed) {
			reply_container.style["display"] = "none";
			return;
		}
		if (selected_thread_id != -1) {
			if (permissions["send_message"] ) {
				this.reply_heading_text.textContent = "Leave a reply";
				this.reply_container.style["display"] = "";
			}
			else {
				this.reply_container.style["display"] = "none";
			}
		}
		else {
			if (permissions["create_thread"]) {
				this.reply_heading_text.textContent = "Create a thread";
				this.reply_container.style["display"] = "";
			}
			else {
				this.reply_container.style["display"] = "none";
			}
		}
		if (session.highest_ranked_group) {
			this.highest_ranked_group_container.style["display"] = "";
			this.highest_ranked_group_label.textContent = session.highest_ranked_group.name;
		}
		else {
			this.highest_ranked_group_container.style["display"] = "none";
			this.highest_ranked_group_checkbox.checked = false;
		}
		this.file_upload_component_promise.then(element => {element.updateVisibilityByPermissions(permissions);});
	}

	async postAction() {
		try {
			let name_length_bytes = (new TextEncoder().encode(this.name_field.value)).length;
			let content_length_bytes = (this.content_field_text_encoder.encode(this.content_field.innerText)).length;
			console.log("Name length: " + name_length_bytes + "\nContent length: " + content_length_bytes);
			if (this.content_field.textContent.length === 0 && this.file_upload_component?.numberOfFiles() === 0) {
				post_content_empty_dialog.showModal();
			}
			else if (name_length_bytes > CONFIG_post_max_name) {
				post_name_too_long_dialog_name_length.textContent = name_length_bytes;
				post_name_too_long_dialog.showModal();
			}
			else if (content_length_bytes > CONFIG_post_max_content) {
				post_content_too_long_dialog_content_length.textContent = content_length_bytes;
				post_content_too_long_dialog.showModal();
			}
			else if (this.file_upload_component?.allUploadsAreCompleted()) {
				post_file_upload_not_complete_dialog.showModal();
			}
			else {
				let response;
				if (this.selected_thread.id === -1) {
					response = await this.postThread();
				}
				else {
					response = await this.postMessage();
				}
				if (response.ok) {
					this.content_field.textContent = "";
					this.file_upload_component?.clear();
					this.content_length_indicator.style["display"] = "none";
					if (this.selected_thread.id === -1) {
						let new_thread_id = Number(response.headers.get("new-thread-id"));
						// thread_posted_dialog_open_thread_button.onclick = () => { openThread(new_thread_id); thread_posted_dialog.close(); }
						this.thread_posted_dialog_open_thread_link.href = ThreadListElement.makeLink(this.selected_board.slug, new_thread_id);
						this.thread_posted_dialog_open_thread_link.onclick = () => { this.thread_posted_dialog.close();}
						this.thread_posted_dialog.showModal();
					}
				}
				else {
					let error_message = response.headers.get("message");
					error_message ??= "An unknown error occured when attempting to post message.";
					throw error_message;
				}
				localStorage.setItem("name", this.name_field.value);
			}
		}
		catch (error) {
			console.error(error);
			error_message.textContent = error;
			error_dialog.showModal();
		}
	}

	gatherPostData(thread_id = null) {
		let data = {
			name: this.name_field.value,
			content: this.content_field.innerText.trim(),
			files: this.file_upload_component?.getFiles()
		};
		if (thread_id !== null) data.thread_id = thread_id;
		if (this.highest_ranked_group_checkbox.checked && session.highest_ranked_group)
			data.highest_ranked_group_id = session.highest_ranked_group.id;
		return data;
	}

	async postMessage() {
		console.log("posting message...");
		const data = this.gatherPostData(this.selected_thread.id);
		console.log(data);
		const fetch_response = await fetch(`api/board/${this.selected_board.slug}/thread/${this.selected_thread.id}/message`, {method: "POST", body: JSON.stringify(data)});

		return fetch_response;
	}

	async deleteMessage(board_slug, thread_id, message_id) {
		console.log("deleting message...");
		const fetch_response = await fetch(`api/board/${board_slug}/thread/${thread_id}/message/${message_id}`, {method: "DELETE"});
		return fetch_response;
	}

	async postThread() {
		console.log("posting thread...");
		const data = {
			thread: {
				post_zero: this.gatherPostData()
			}
		};
		console.log(data)
		const fetch_response = await fetch(`api/board/${this.selected_board.slug}/thread`, {method: "POST", body: JSON.stringify(data)});
		return fetch_response;
	}

	// websocket = connectToServer();
	/*
	async getClientInfo() {
		try {
			const response = await fetch("api/user/client");
			console.log(response);
			if (response.ok) {
				await response.json().then(response_json => {
					console.log(response_json);
					if (!response_json["has_cookie"] && localStorage.getItem("logged_in") === "true") {
						displayClientAsLoggedOut();
					}
					this.server_permissions = response_json["server_permissions"];
					this.highest_ranked_group = response_json["highest_ranked_group"];
					window.history.replaceState(state, null, ""); // Initial state
					// manage_server_button.style["display"] = response_json["server_permissions"]["manage_permissions"] === true ? "inherit" : "none";
					console.log(state);
					// if (!(this.selected_thread.id >= 0)) {
					// 	this.updateReplyContainer(this.selected_thread.id, this.server_permissions);
					// }
					// reply_container.style["display"] = response_json["server_permissions"]["create_thread"] === true ? "inherit" : "none";
				});
			}
			else {
				let error_message = response.headers.get("message");
				error_message ??= "An unknown error occured when attempting to fetch client.";
				throw error_message;
			}
		}
		catch (error) {
			console.error(error);
			error_message.textContent = error;
			error_dialog.showModal();
		}
	}
	*/
	// Websocket
	wsListenToThread(board_slug, thread_id, signal) {
		if (thread_id === -1)
			throw "Attempted to connect websocket to thread list";
		let listening_to_thread = thread_id;
		let sock;
		if (!this.websocket) {
			sock = new WebSocket(this.websocket_url);
		}
		else {
			return this.websocket;
		}
		sock.onopen = (event) => {
			// connection_status.textContent = "Connected 🙂";
			console.log("Websocket connected.");
			// this.websocket_connected = true;
			// clearInterval(reconnect_websocket_interval);
			// reconnect_websocket_interval = null;
			if (signal.aborted)
				return;

			sock.send(JSON.stringify({
				"type": "listen_to_thread",
				"board": board_slug,
				"thread_id": thread_id
			}));
		}

		sock.onmessage = (event) => {
			console.log("Message received: " + event.data);
			let message_json = JSON.parse(event.data);
			if (message_json.type === "post") {
				// auto-scroll to new message if window was scrolled near the bottom already
				// would be nice if window.scrollMaxY was standard! document.body.scrollHeight - document.body.clientHeight will be used instead
				let do_scroll = window.scrollY >= document.body.scrollHeight - document.body.clientHeight - 10;

				// A little dodgy using selected_thread since its not proven correct, worst case scenario you get live messages from the wrong thread
				this.post_container.appendChild(this.createPost(this.selected_board.slug, this.selected_thread.id, JSON.parse(event.data), this.selected_thread.permissions));

				if (do_scroll) {
					window.scrollTo(0, this.post_container.scrollHeight);
				}
			}
			else {
				console.error("message_json type " + message_json.type + " not recognised.");
			}
		}

		// sock.onerror = (event) => {
			// connection_status.textContent = "Errored 😤"
		// }

		sock.onclose = (event) => {
			// connection_status.textContent = "Closed \u{1F634}"
			// this.websocket_connected = false;
			// if (this.selected_thread.id === listening_to_thread) {
			// 	console.log("Websocket connection closed. Re-connecting will be attempted in 30 seconds.");
			// 	setTimeout(() => {
			// 		if (this.selected_thread.id === listening_to_thread) {
			// 			this.websocket = this.connectToServer();
			// 		}
			// 	}, 30000);
			// }
		}
		return sock;
	}
}

customElements.define("thread-list", ThreadListElement);
