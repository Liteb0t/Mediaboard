import { global_styles } from "./styles.js";


export class UploadedFile {
	file_size_limit = CONFIG_file_size_limit_mb*1024*1024;
	static media_url = "./media/";

	static decode_utf8(s) {
		return decodeURIComponent(escape(s));
	}

	constructor(file_upload_element, file_object) {
		this.file_upload = file_upload_element;
		this.file = file_object; // derived from this.file_upload.files
		this.container_element = null;
		this.thumbnail_element = null;
		this.information_element = null;
		this.progress_element = null;
		this.filename = file_object.name;
		if (file_object.size > this.file_size_limit) {
			this.file_upload.value = "";
			throw new Error(`The file ${file_object.name} exceeds the CONFIG_this.file_size_limit_mb megabyte limit. <br>
				<code>${file_object.size}</code> - this file (bytes)<br>
				<code>${this.file_size_limit}</code> - limit (bytes)`);
		}
		const file_name_size = (new TextEncoder().encode(file_object.name)).length;
		if (file_name_size > CONFIG_post_max_file_name) {
			throw new Error(`File name "${file_object.name}" exceeds the limit of CONFIG_post_max_file_name bytes.<br>
				This file name: ${file_name_size} bytes.`);
		}
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
			this.file_upload.disabled = false;
		}
	}
	upload(board_slug, thread_id) {
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
		request.open("POST", `./api/board/${board_slug}/` + (thread_id > -1 ? `thread/${thread_id}/upload` : "upload/"));
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
					let decoded_filename = UploadedFile.decode_utf8(server_filename);
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
						this.thumbnail_element.setAttribute("src", UploadedFile.media_url + `thumbnails/THUMBNAIL_${decoded_filename}.${thumbnail_file_extension}`);
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

class FileUploadElement extends HTMLElement {
	constructor() {
		// Always call super first in constructor
		super();
		const template = document.getElementById("file-upload-template");
		const shadow = this.attachShadow({ mode: "open" });
		shadow.adoptedStyleSheets = [global_styles]; // imported at the top there
		shadow.appendChild(template.content.cloneNode(true));

		for (const element of shadow.querySelectorAll(["#file_upload", "#file_post_list", "#file_upload_fieldset"])) {
			this[element.id] = element;
		}

		this.file_upload.onchange = (event) => {
			console.log("file changed.");
			if (this.file_upload.files.length + this.file_post_list.childNodes.length > 4) {
				error_message.textContent = "Cannot attach more than 4 files.";
				error_dialog.showModal();
				this.file_upload.value = "";
			}
			else {
				for (let chosen_file of this.file_upload.files) {
					this.addFile(chosen_file);
				}
			}
		}
	}
	addFile(file) { // must be File object
		try {
			let new_file = new UploadedFile(this.file_upload, file);
			new_file.addFileToList(this.file_post_list);
			new_file.upload(this.selected_board.slug, this.selected_thread.id); // TODO find way to get upload destination
		}
		catch (error) {
			console.error(error);
			error_message.innerHTML = error.message ? error.message : "An unknown error occured when attempting to upload file.";
			error_dialog.showModal();
		}
	}
	updateVisibilityByPermissions(permissions) {
		this.file_upload_fieldset.style["display"] = permissions["upload_file"] === true ? "inherit" : "none";
	}
}

customElements.define("file-upload", FileUploadElement);

