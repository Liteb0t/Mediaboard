"use strict";

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

	constructor() {
		// Always call super first in constructor
		super();
		this.selected_board_slug = null;
		const template = document.getElementById("thread-list-template");
		const shadow = this.attachShadow({ mode: "open" });
		shadow.appendChild(template.content.cloneNode(true));

		// TODO explicitly map elements post_container etc
		for (const element of shadow.querySelectorAll("[id]")) {
			this[element.id] = element;
		}
	}

	connectedCallback() {
		console.log("Custom element added to page.");
		this.post_button.onclick = () => this.postAction();
// threads_button.onclick = () => { openThreadList(); } // included in navbar.html
// refresh_button.onclick = () => {refresh_send();} // imported from navbar.html
	}

	disconnectedCallback() {
	console.log("Custom element removed from page.");
	}

	connectedMoveCallback() {
	console.log("Custom element moved with moveBefore()");
	}

	adoptedCallback() {
	console.log("Custom element moved to new page.");
	}

	// attributeChangedCallback(name, oldValue, newValue) {
	// console.log(`Attribute ${name} has changed.`);
	// }

	/*
	async refresh_send() {
		loading_dialog.showModal();
		lock_scroll_to_bottom = false;
		if (typeof websocket !== "undefined") {
			websocket.close();
		}
		try {
			if (state.selected_thread.id === -1) {
				await getAllThreads(); // TODO scroll to previously viewed thread
			}
			else {
				await getThread();
				lock_scroll_to_bottom = true;
			}
			loading_dialog.close();
		}
		catch (error) {
			loading_dialog.close();
			console.error(error);
			error_message.textContent = error;
			error_dialog.showModal();
		}
	}
	*/

	async loadBoard(board_slug) {
		this.selected_board_slug = board_slug;
		// let response = await fetch(`./api/board/${state.selected_board.slug}/threads`);
		let response = await fetch(`./api/board/${board_slug}/threads`);
		console.log(response);
		if (response.ok) {
			await response.json().then(response_obj => {
				console.log(response_obj);
				this.showAllThreads(board_slug, response_obj);
			});
		}
		else {
			let error_message = response.headers.get("message");
			error_message ??= "An unknown error occured when attempting to fetch threads.";
			throw error_message;
		}
	}

	showAllThreads(board_slug, message_json) {
		location_indicator.textContent = "Threads in " + board_slug;
		// threads_button.disabled = true;
		// if (state.selected_board.permissions) {
			// this.updateReplyContainer(state.selected_thread.id, state.selected_board.permissions);
		// }

		const fragment = new DocumentFragment();
		for (let thread of message_json.threads) {
			// console.log(thread.id);
			// console.log(thread.post_zero);
			let post_zero_element = this.createPost(board_slug, -1, thread.post_zero, {/*...state.selected_board.permissions,*/ ...thread.client_permissions});
			if (url_params_display_as_embed) {
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
				reply_count.textContent = "Replies: " + (thread["reply_count"]);
				reply_count_container.appendChild(reply_count);

				thread_toolbar.append(reply_count_container, flex_separator.cloneNode());

				if (thread.client_permissions["manage_permissions"]) {
					let manage_thread_permissions_button = document.createElement("a");
					manage_thread_permissions_button.classList.add("Button", "ToolBarButton");
					manage_thread_permissions_button.textContent = "Manage permissions";
					manage_thread_permissions_button.setAttribute("href", `./manage_permissions.html?location=/board/${board_slug}/thread/${thread.id}/`);
					thread_toolbar.append(manage_thread_permissions_button);
				}

				let open_thread_button = document.createElement("button");
				open_thread_button.classList.add("ToolBarButton");
				open_thread_button.textContent = "\u21E8 Open thread";
				open_thread_button.onclick = (event) => {
					this.openThread(board_slug, thread.id);
				}

				thread_toolbar.append(open_thread_button);
				// thread_toolbar.appendChild(open_thread_button);
				thread_element.appendChild(thread_toolbar);
				fragment.appendChild(thread_element);
			}
		}
		if (fragment.children.length === 0)
			this.post_container.replaceChildren(document.createTextNode("None."));
		else
			this.post_container.replaceChildren(fragment);
	}

	openThread(board_slug, thread_id) {
		// state.selected_thread.id = thread_id;
		// TODO use url_params.toString() if need bee
		history.pushState(state, "", window.location.pathname + `?board=${board_slug}&thread=${thread_id}`);
		this.getThread(board_slug, thread_id);
	}

	async getThread(board_slug, thread_id) {
		let response = await fetch(`./api/board/${board_slug}/thread/${thread_id}`);
		if (response.ok) {
			await response.json().then(response_obj => {
				console.log(response_obj);
				this.showThread(board_slug, thread_id, response_obj);
			});
			websocket = connectToServer();
		}
		else {
			let error_message = response.headers.get("message");
			error_message ??= "An unknown error occured when attempting to load thread.";
			throw error_message;
		}
	}

	showThread(board_slug, thread_id, thread_json) {
		location_indicator.textContent = `Viewing thread ${thread_id}`;
		// threads_button.disabled = false;
		this.updateReplyContainer(thread_id, thread_json["client_permissions"]);
		// state.selected_thread.client_permissions = thread_json["client_permissions"]; // Needed for websocket posts coming in
		// header_bar.scrollIntoView(false); // Scroll back to top
		const fragment = new DocumentFragment();
		for (let message of thread_json["messages"]) {
			fragment.appendChild(this.createPost(board_slug, thread_id, message, thread_json["client_permissions"]));
		}
		this.post_container.replaceChildren(fragment);
	}

	createPost(board_slug, selected_thread_id, data, client_permissions) {
		console.log(data);
		let post_box_element = document.createElement("article");
		post_box_element.classList.add("Post");

		let post_header_element
		if (!url_params_display_as_embed) {
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
			let post_date = new Date(data["created_at"] * 1000 - timezone_offset_seconds * 1000).toISOString();;
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
					let server_response = await this.deleteMessage(selected_board_slug, data.thread_id, data.id_in_thread);
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
					reply_container.open = true;

					let reply_link_node = document.createTextNode(post_id + " ");
					// Add reply tag at caret position, if focused on content_field
					let range, selection;
					selection = window.getSelection();
					if (content_field.contains(selection.focusNode)) {
						range = selection.getRangeAt(0);
						// console.log(range);
						range.insertNode(reply_link_node);
					}
					else
						content_field.append(reply_link_node);

					// Move caret position to end of reply tag
					range = document.createRange();
					// console.log(reply_link_node);
					range.setStartAfter(reply_link_node);
					range.collapse(true);
					selection.removeAllRanges();
					selection.addRange(range);
					// content_field.focus();
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
				thumbnail_element.setAttribute("src", thumbnail_url + thumbnail_filename.replace(" ", "%20"));
				if (file["width"] && file["height"]) {
					if (file["width"] < file["height"]) {
						thumbnail_element.width = file["width"] < file["height"]>>1 ? Math.ceil(thumbnail_size / 2) : Math.ceil(thumbnail_size * (file["width"]/file["height"]));
						thumbnail_element.height = thumbnail_size;
					}
					else if (file["height"] < file["width"]) {
						thumbnail_element.width = thumbnail_size;
						thumbnail_element.height = file["height"] < file["width"]>>1 ? Math.ceil(thumbnail_size / 2) : Math.ceil(thumbnail_size * (file["height"]/file["width"]));
					}
					else {
						thumbnail_element.width = thumbnail_size;
						thumbnail_element.height = thumbnail_size;
					}
				}
				else { // Legacy: images uploaded before 0.1.1 don't have width and height saved
					thumbnail_element.width = thumbnail_element.height = thumbnail_size;
				}
				thumbnail_element.setAttribute("loading", "lazy");
				figure_element.append(thumbnail_element);
			}
			else if (file_format === Format.AUDIO) {
				let audio_element = document.createElement("audio");
				audio_element.controls = true;
				audio_element.setAttribute("controlslist", "nofullscreen nodownload");
				audio_element.setAttribute("src", media_url + file["filename"].replace(" ", "%20"));
				figure_element.appendChild(audio_element);
			}
			let download_filename, caption_filename;
			if (filename_extension_i == -1) {
				download_filename = file["filename"].slice(0, file["filename"].length - media_uuid_len);
				caption_filename = download_filename.length < 32 ? download_filename : (download_filename.substr(0, 28) + "(...)");
			}
			else {
				download_filename = file["filename"].slice(0, filename_extension_i - media_uuid_len);
				caption_filename = download_filename.length < 32 ? download_filename : (download_filename.substr(0, 28) + "(...)");
				download_filename += file["filename"].slice(filename_extension_i, file["filename"].length);
				caption_filename += file["filename"].slice(filename_extension_i, file["filename"].length);
			}
			let figcaption_element = document.createElement("figcaption");
			let media_src = media_url + file["filename"].replace(" ", "%20");
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

		if (!url_params_display_as_embed) {
			post_box_element.appendChild(post_header_element);
		}
		post_box_element.appendChild(post_content_element);
		post_box_element.appendChild(post_files_element);
		// this.post_container_element.appendChild(post_box_element);
		return post_box_element;
	}

	updateReplyContainer(selected_thread_id, permissions) {
		if (url_params_display_as_embed) {
			reply_container.style["display"] = "none";
			return;
		}
		if (selected_thread_id >= 0) {
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
		if (state.highest_ranked_group) {
			this.highest_ranked_group_container.style["display"] = "inherit";
			this.highest_ranked_group_label.textContent = state.highest_ranked_group.name;
		}
		else {
			this.highest_ranked_group_container.style["display"] = "none";
			this.highest_ranked_group_checkbox.checked = false;
		}
		this.file_upload_fieldset.style["display"] = permissions["upload_file"] === true ? "inherit" : "none";
	}
	getFiles() {
		let files = [];
		for (let file_element of file_post_list.children) {
			if (!file_element.classList.contains("Errored")) {
				let file_data = {"filename": file_element.id.slice(5, file_element.id.length)};
				const width = file_element.getAttribute("data-image-width");
				if (width) {
					const height = file_element.getAttribute("data-image-height");
					if (height) {
						file_data["width"] = Number(width);
						file_data["height"] = Number(height);
					}
				}
				const thumbnail_file_extension = file_element.getAttribute("data-thumbnail-file-extension");
				if (thumbnail_file_extension)
					file_data["thumbnail_file_extension"] = thumbnail_file_extension;
				files.push(file_data);
			}
		}
		return files;
	}

	async postAction() {
		try {
			let name_length_bytes = (new TextEncoder().encode(name_field.value)).length;
			let content_length_bytes = (content_field_text_encoder.encode(content_field.innerText)).length;
			console.log("Name length: " + name_length_bytes + "\nContent length: " + content_length_bytes);
			if (content_field.textContent.length === 0 && file_post_list.childElementCount === 0) {
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
			else if (file_post_list.querySelector("progress")) {
				post_file_upload_not_complete_dialog.showModal();
			}
			else {
				let response;
				if (state.selected_thread.id === -1) {
					response = await postThread();
				}
				else {
					response = await postMessage();
				}
				if (response.ok) {
					content_field.textContent = "";
					file_upload.value = "";
					file_post_list.textContent = "";
					content_length_indicator.style["display"] = "none";
					if (state.selected_thread.id === -1) {
						let new_thread_id = Number(response.headers.get("new-thread-id"));
						thread_posted_dialog_open_thread_button.onclick = () => { openThread(new_thread_id); thread_posted_dialog.close(); }
						thread_posted_dialog.showModal();
					}
				}
				else {
					let error_message = response.headers.get("message");
					error_message ??= "An unknown error occured when attempting to post message.";
					throw error_message;
				}
				localStorage.setItem("name", name_field.value);
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
			name: name_field.value,
			content: content_field.innerText.trim(),
			files: getFiles()
		};
		if (thread_id !== null) data.thread_id = thread_id;
		if (highest_ranked_group_checkbox.checked && state.highest_ranked_group)
			data.highest_ranked_group_id = state.highest_ranked_group.id;
		return data;
	}

	async postMessage() {
		console.log("posting message...");
		const data = gatherPostData(state.selected_thread.id);
		console.log(data);
		const fetch_response = await fetch(`api/board/${state.selected_board.slug}/thread/${state.selected_thread.id}/message`, {method: "POST", body: JSON.stringify(data)});

		return fetch_response;
	}

	async deleteMessage(board_slug, thread_id, message_id) {
		console.log("deleting message...");
		const fetch_response = await fetch(`api/board/${board_slug}/thread/${thread_id}/message/${message_id}`, {method: "DELETE"});
		return fetch_response;
	}

	async postThread() {
		console.log("posting thread...");
		let files = getFiles();
		const data = {
			thread: {
				post_zero: gatherPostData()
			}
		};
		console.log(data)
		const fetch_response = await fetch(`api/board/${state.selected_board.slug}/thread`, {method: "POST", body: JSON.stringify(data)});
		return fetch_response;
	}

	// websocket = connectToServer();
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
					state.server_permissions = response_json["server_permissions"];
					state.highest_ranked_group = response_json["highest_ranked_group"];
					window.history.replaceState(state, null, ""); // Initial state
					// manage_server_button.style["display"] = response_json["server_permissions"]["manage_permissions"] === true ? "inherit" : "none";
					console.log(state);
					// if (!(state.selected_thread.id >= 0)) {
					// 	this.updateReplyContainer(state.selected_thread.id, state.server_permissions);
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
	async getBoardInfo() {
		try {
			const response = await fetch(`api/board/${state.selected_board.slug}`);
			console.log(response);
			if (response.ok) {
				await response.json().then(response_json => {
					console.log(response_json);
					state.selected_board.permissions = response_json["client_permissions"];
					window.history.replaceState(state, null, ""); // Initial state
					console.log(state);
					if (!(state.selected_thread.id >= 0)) {
						this.updateReplyContainer(state.selected_thread.id, state.selected_board.permissions);
					}
				});
			}
			else {
				let error_message = response.headers.get("message");
				error_message ??= "An unknown error occured when attempting to fetch board info.";
				throw error_message;
			}
		}
		catch (error) {
			console.error(error);
			error_message.textContent = error;
			error_dialog.showModal();
		}
	}

	decode_utf8(s) {
		return decodeURIComponent(escape(s));
	}
}

customElements.define("thread-list", ThreadListElement);


// loading_dialog.showModal();
/*
const media_url = "./media/";
const thumbnail_url = "./media/thumbnails/";
const websocket_url = (window.location.protocol === "http:" ? "ws://" : "wss://") + window.location.host + window.location.pathname;
const thumbnail_size = CONFIG_thumbnail_size;
const file_size_limit = CONFIG_file_size_limit_mb*1024*1024;
const media_uuid_len = 36;
const localDate = new Date();
const timezone_offset_seconds = localDate.getTimezoneOffset() * 60;
const flex_separator = document.createElement("div");
flex_separator.classList.add("FlexSeparator");
let state = {
	selected_board: {
		slug: null,
		permissions: {}
	},
	selected_thread: {
		id: null,
		client_permissions: {}
	},
	server_permissions: {}
};
let websocket;
let websocket_connected = false;
let reconnect_websocket_interval;
const content_field_text_encoder = new TextEncoder();
*/

// For embedding Mediaboard within a webpage using an <iframe>.
// Hides everything except messages.
/*
const url_params = new URLSearchParams(location.search);
let url_params_display_as_embed;
if (url_params.get("display") === "embed") {
	header_bar.style["display"] = "none";
	location_indicator.style["display"] = "none";
	reply_container.style["display"] = "none";
	url_params_display_as_embed = true;
}
else url_params_display_as_embed = false;
state.selected_board.slug = url_params.get("board") ? url_params.get("board") : -1;
state.selected_thread.id = url_params.get("thread") ? Number(url_params.get("thread")) : -1;
#ifdef WITH_WEBRTC
navlink_live.href = `live.html?board=${state.selected_board.slug}`;
navlink_live.style["display"] = "inherit";
#endif
window.onpopstate = function(event) {
	if (event.state) {
		state = event.state;
	}
	refresh_send();
}
*/


// The value in the name field is set to the name used in the most recent post, if any.
// name_field.value = localStorage.getItem("name");
//
// document.addEventListener("logout", () => {
// 	state.server_permissions = undefined;
// 	refresh_send();
// 	getClientInfo();
// })
//
// document.body.onclick = (event) => {
// 	let target = event.target;
// 	// console.log(target);
// 	if (target.classList.contains("EmbedThumbnail"))
// 		expandEmbedFigure(target.parentNode);
// 	else if (target.classList.contains("EmbedImage"))
// 		contractEmbedFigure(target.parentNode);
// 	else if (target.classList.contains("EmbedIframe"))
// 		toggleEmbedIframe(target.parentNode);
// }

// function toggleEmbedIframe(figure_element) {
// 	let iframe_element = figure_element.querySelector("iframe");
// 	if (figure_element.getAttribute("expanded") === "") {
// 		iframe_element.style["display"]="none";
// 		figure_element.removeAttribute("expanded");
// 		iframe_element.src = "";
// 	}
// 	else {
// 		iframe_element.style["display"]="inherit";
// 		figure_element.setAttribute("expanded", "");
// 		iframe_element.src = figure_element.getAttribute("data-iframe-src");
// 	}
// }

// const FIGURE_EMBED = 0; // Main <img> or <video> is child 0 of <figure>, thumbnail is child 1.
// const FIGURE_THUMBNAIL = 1;
// function expandEmbedFigure(figure_element) {
// 	figure_element.children[FIGURE_EMBED].setAttribute("src", media_url + figure_element.getAttribute("filename"));
// 	figure_element.children[FIGURE_EMBED].style["display"] = "inherit";
// 	figure_element.children[FIGURE_THUMBNAIL].style["display"] = "none";
// }
// function contractEmbedFigure(figure_element) {
// 	figure_element.children[FIGURE_EMBED].style["display"] = "none";
// 	figure_element.children[FIGURE_THUMBNAIL].style["display"] = "inherit";
// }

// function openThreadList() {
// 	state.selected_thread.id = -1;
// 	history.pushState(state, "", window.location.pathname + `?board=${state.selected_board.slug}`);
// 	refresh_send();
// }

/*
content_field.oninput = () => {
	// Remove leftover <br> tag from contenteditable so the 'Content' placeholder appears again
	if (content_field.childElementCount === 1 && content_field.children[0].tagName === "BR" && content_field.textContent.length === 0) {
		content_field.removeChild(content_field.children[0]);
	}

	let content_field_length = (content_field_text_encoder.encode(content_field.innerText)).length;
	if (content_field_length > Math.round(CONFIG_post_max_content * 0.8)) {
		if (content_length_indicator.style["display"] === "none") {
			content_length_indicator.style["display"] = "inherit";
		}
		content_length_indicator.textContent = `${content_field_length}/${CONFIG_post_max_content}`;
		if (content_field_length > CONFIG_post_max_content) {
			if (!content_length_indicator.classList.contains("LimitExceeded")) {
				content_length_indicator.classList.add("LimitExceeded");
			}
		}
		else {
			if (content_length_indicator.classList.contains("LimitExceeded")) {
				content_length_indicator.classList.remove("LimitExceeded");
			}
		}
	}
	else {
		if (content_length_indicator.style["display"] !== "none") {
			content_length_indicator.style["display"] = "none";
		}
	}
}

// "Scroll-lock" feature - when a user is scrolled to the bottom of the messages list, adding content in the reply container will push the messages up so that the latest messages are not obstructed.
let lock_scroll_to_bottom = false;
const reply_container_resize_observer = new ResizeObserver(() => {
	if (lock_scroll_to_bottom === true) {
		bottom_marker.scrollIntoView(false);
	}
});
reply_container_resize_observer.observe(reply_container);
const options = {
	// root: document.body,
	rootMargin: "5px",
	scrollMargin: "5px",
	threshold: 0.5,
};
const callback = (entries, observer) => {
	for (let entry of entries) {
		if (entry.target === bottom_marker && state.selected_thread.id > -1) {
			lock_scroll_to_bottom = entry.isIntersecting;
		}
	}

	console.log(entries);
}
const scrolled_to_bottom_observer = new IntersectionObserver(callback, options);
*/

/*
file_upload.onchange = (event) => {
	console.log("file changed.");
	if (file_upload.files.length + file_post_list.childNodes.length > 4) {
		error_message.textContent = "Cannot attach more than 4 files.";
		error_dialog.showModal();
		file_upload.value = "";
	}
	else {
		for (let chosen_file of file_upload.files) {
			try {
				let new_file = new UploadedFile(chosen_file);
				new_file.addFileToList(file_post_list);
				new_file.upload(state.selected_thread.id);
			}
			catch (error) {
				console.error(error);
				error_message.innerHTML = error.message ? error.message : "An unknown error occured when attempting to upload file.";
				error_dialog.showModal();
			}
		}
	}
}

class UploadedFile {
	constructor(file_object) {
		this.file = file_object; // derived from file_upload.files
		this.container_element = null;
		this.thumbnail_element = null;
		this.information_element = null;
		this.progress_element = null;
		this.filename = file_object.name;
		if (file_object.size > file_size_limit) {
			file_upload.value = "";
			throw new Error(`The file ${file_object.name} exceeds the CONFIG_file_size_limit_mb megabyte limit. <br>
				<code>${file_object.size}</code> - this file (bytes)<br>
				<code>${file_size_limit}</code> - limit (bytes)`);
		}
		const file_name_size = (new TextEncoder().encode(file_object.name)).length;
		if (file_name_size > CONFIG_post_max_file_name) {
			throw new Error(`File name "${file_object.name}" exceeds the limit of CONFIG_post_max_file_name bytes.<br>
				This file name: ${file_name_size} bytes.`);
		}
		console.log("UploadedFile test passed...");
		// uploadFile(chosen_file);
	}
	addFileToList(file_upload_list_element) {
		this.container_element = document.createElement("div");
		this.thumbnail_element = document.createElement("img");
		this.thumbnail_element.setAttribute("src", "static/placeholder.png");
		this.thumbnail_element.setAttribute("width", "100px");
		this.thumbnail_element.setAttribute("height", "100px");
		this.information_element = document.createElement("div");
		this.information_element.classList.add("FileInformation");
		this.file_title_element = document.createElement("span");
		this.status_element = document.createElement("span");
		this.status_element.textContent = "Uploading...";
		// file_title.classList.add("FileTitle");
		this.file_title_element.textContent = this.filename;
		let file_delete_button = document.createElement("button");
		file_delete_button.textContent = "\u{1F5D1} Delete"; // 1F5D1 = смітник
		this.progress_element = document.createElement("progress");
		this.progress_element.value = 0;
		this.information_element.append(this.file_title_element, document.createElement("br"), this.status_element, this.progress_element, document.createElement("br"), file_delete_button);
		this.container_element.append(this.thumbnail_element, this.information_element);
		file_upload_list_element.appendChild(this.container_element);
		file_delete_button.onclick = (event) => {
			// Add delete to server later
			// file_upload_list_element.removeChild(document.getElementById("file_" + server_filename));
			file_upload_list_element.removeChild(this.container_element);
			file_upload.disabled = false;
		}
	}
	upload(selected_thread_id) {
		let formdata = new FormData();
		formdata.append("File_1", this.file);
		let received_error = false;
		let request = new XMLHttpRequest();
		this.progress_element.max = this.file.size;
		console.log(this.file);
		request.upload.addEventListener("progress", (event) => {
			this.progress_element.value = event.loaded;
			if (event.loaded === event.total) {
				this.status_element.textContent = "Processing...";
			}
		});
		request.responseType = "text";
		request.open("POST", `./api/board/${state.selected_board.slug}/` + (selected_thread_id > -1 ? `thread/${selected_thread_id}/upload` : "upload/"));
		request.timeout = 60*60*1000; // 1 hour
		request.onreadystatechange = () => {
			console.log(`Ready state changed. Status: ${request.status}\nreadyState: ${request.readyState}`);
			if (!(request.status >= 200 && request.status < 300) && request.status != 0 && !received_error) {
				received_error = true;
				request.abort();
				this.showHttpError(request);
			}
		}
		request.send(formdata);
		request.onload = () => {
			console.log("Request onLoad fired");
			if (request.readyState === request.DONE) {
				console.log("Received response");
				if (request.status >= 200 && request.status < 300) {
					const server_filename = request.getResponseHeader("File-Name");
					console.log("server_filename: " + server_filename);
					let decoded_filename = decode_utf8(server_filename);
					console.log("server_filename decoded: " + decoded_filename);
					// const utf8array = new TextEncoder().encode(server_filename);
					// var newdecoded = new TextDecoder().decode(utf8array);
					// console.log("server_filename dec new: " + newdecoded);
					this.container_element.setAttribute("id", "file_" + decoded_filename);
					// this.file_title_element.textContent = decoded_filename;
					const width = request.getResponseHeader("Image-Width");
					const height = request.getResponseHeader("Image-Height");
					if (width && height) {
						this.container_element.setAttribute("data-image-width", width);
						this.container_element.setAttribute("data-image-height", height);
					}
					const thumbnail_file_extension = request.getResponseHeader("Thumbnail-File-Extension");
					if (thumbnail_file_extension) {
						this.container_element.setAttribute("data-thumbnail-file-extension", thumbnail_file_extension);
						this.thumbnail_element.setAttribute("src", media_url + `thumbnails/THUMBNAIL_${decoded_filename}.${thumbnail_file_extension}`);
					}
					this.status_element.textContent = "Upload complete";
					this.status_element.classList.add("UploadCompleteIndicator");
					this.information_element.removeChild(this.progress_element);
				}
				else {
					this.showHttpError(request);
				}
			}
		}
	}
	showHttpError(request) {
		const error_message = request.getResponseHeader("message");
		this.container_element.classList.add("Errored");
		file_upload_error_dialog_message.innerText =
			`Status: ${request.status} ${request.statusText}\n` +
			(error_message || "An unknown error occured.");
		file_upload_error_dialog.showModal();
		const upload_failed_indicator = document.createElement("span");
		upload_failed_indicator.classList.add("UploadFailedIndicator");
		upload_failed_indicator.textContent = "Upload failed";
		this.information_element.replaceChild(upload_failed_indicator, this.progress_element);
		this.thumbnail_element.setAttribute("src", "./static/x_icon.png");
	}
}
*/
/*
function connectToServer() {
	let listening_to_thread = state.selected_thread.id;
	let sock;
	if (!websocket_connected) {
		sock = new WebSocket(websocket_url);
		websocket_connected = true;
	}
	else {
		return websocket;
	}
	sock.onopen = (event) => {
		// connection_status.textContent = "Connected 🙂";
		console.log("Websocket connected.");
		websocket_connected = true;
		clearInterval(reconnect_websocket_interval);
		reconnect_websocket_interval = null;
		if (state.selected_thread.id > -1) {
			sock.send(JSON.stringify({
				"type": "listen_to_thread",
				"board": state.selected_board.slug,
				"thread_id": state.selected_thread.id
			}));
		}
	}

	sock.onmessage = (event) => {
		console.log("Message received: " + event.data);
		let message_json = JSON.parse(event.data);
		if (message_json.type === "post") {
			// auto-scroll to new message if window was scrolled near the bottom already
			// would be nice if window.scrollMaxY was standard! document.body.scrollHeight - document.body.clientHeight will be used instead
			let do_scroll = window.scrollY >= document.body.scrollHeight - document.body.clientHeight - 10;

			this.post_container.appendChild(createPost(JSON.parse(event.data), state.selected_thread.client_permissions));

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
		websocket_connected = false;
		if (state.selected_thread.id === listening_to_thread) {
			console.log("Websocket connection closed. Re-connecting will be attempted in 30 seconds.");
			setTimeout(() => {
				if (state.selected_thread.id === listening_to_thread) {
					websocket = connectToServer();
				}
			}, 30000);;
		}
	}
	return sock
}
*/

// getClientInfo(); // runs asynchronously
// getBoardInfo();
// refresh_send().then(() => {
// 	loading_dialog.close();
// 	scrolled_to_bottom_observer.observe(bottom_marker);
// });
