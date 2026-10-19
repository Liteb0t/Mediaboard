export class Format {
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

export class Media {
	static media_url = "./media/";
	static thumbnail_url = "./media/thumbnails/";
	static thumbnail_placeholder_url = "./static/file_icon.png";
	static media_uuid_len = 36;
	static thumbnail_size = CONFIG_thumbnail_size;
	static FIGURE_EMBED = 0; // Main <img> or <video> is child 0 of <figure>, thumbnail is child 1.
	static FIGURE_THUMBNAIL = 1;
	static handleOnClick = (event) => {
		let target = event.target;
		// console.log(target);
		let element = event.target.closest("media-embed");
		if (element)
			element.whenClicked(event.target);
	}
}

class MediaEmbed extends HTMLElement {
	constructor() {
		super();
		// const template = document.getElementById("thread-list-template");
		// const shadow = this.attachShadow({ mode: "open" });
		// shadow.appendChild(template.content.cloneNode(true));

		// for (const element of shadow.querySelectorAll("[id]")) {
		// 	this[element.id] = element;
		// }
	}
	render(file, hide_deleted = false) {
		const show_file_as_deleted = hide_deleted && file["deleted"];
		this.classList.add("EmbedFigure");
		// let figure_element = document.createElement("figure");
		const filename = file["filename"]/*.replace(" ", "%20")*/;
		this.setAttribute("filename", filename);
		let filename_extension_i = file["filename"].lastIndexOf(".");
		let file_format = Format.fromFileName(file["filename"]); // Format enum
		if (show_file_as_deleted) {
			this.thumbnail_element = document.createElement("img");
			this.thumbnail_element.classList.add("EmbedThumbnail");
			this.thumbnail_element.setAttribute("src", Media.thumbnail_placeholder_url);
			this.thumbnail_element.width = this.thumbnail_element.height = 64;
			this.thumbnail_element.setAttribute("loading", "lazy");
			this.append(this.thumbnail_element);
		}
		else if (file_format === Format.AUDIO) {
			this.embed_element = document.createElement("audio");
			this.embed_element.controls = true;
			this.embed_element.setAttribute("controlslist", "nofullscreen nodownload");
			this.embed_element.setAttribute("src", Media.media_url + filename);
			this.appendChild(this.embed_element);
		}
		else {
			let expandable;
			if (file_format === Format.IMAGE) {
				expandable = true;
				this.embed_element = document.createElement("img");
				this.embed_element.classList.add("EmbedImage");
				if (file["width"] && file["height"]) {
					this.embed_element.width = file["width"];
					this.embed_element.height = file["height"];
				}
				this.embed_element.style["display"] = "none";
				this.appendChild(this.embed_element);
			}
			else if (file_format === Format.VIDEO) {
				expandable = true;
				this.embed_element = document.createElement("video");
				this.embed_element.classList.add("EmbedVideo");
				this.embed_element.controls = true;
				this.embed_element.disablePictureInPicture = true;
				this.embed_element.setAttribute("controlslist", "nofullscreen nodownload");
				this.embed_element.style["display"] = "none";
				this.appendChild(this.embed_element);
			}
			else {
				expandable = false;
			}
			if (expandable)
				this.setAttribute("expandable", "");

			// Thumbnail
			this.thumbnail_element = document.createElement("img");
			this.thumbnail_element.classList.add("EmbedThumbnail");
			if (file.thumbnail_file_extension) {
				let thumbnail_filename = file.thumbnail_file_extension ? `THUMBNAIL_${file.filename}.${file.thumbnail_file_extension}` : "THUMBNAIL_" + file["filename"] + ".CONFIG_thumbnail_file_extension";
				this.thumbnail_element.setAttribute("src", Media.thumbnail_url + thumbnail_filename/*.replace(" ", "%20")*/);
				if (file["width"] && file["height"]) {
					if (file["width"] < file["height"]) {
						this.thumbnail_element.width = file["width"] < file["height"]>>1 ? Math.ceil(Media.thumbnail_size / 2) : Math.ceil(Media.thumbnail_size * (file["width"]/file["height"]));
						this.thumbnail_element.height = Media.thumbnail_size;
					}
					else if (file["height"] < file["width"]) {
						this.thumbnail_element.width = Media.thumbnail_size;
						this.thumbnail_element.height = file["height"] < file["width"]>>1 ? Math.ceil(Media.thumbnail_size / 2) : Math.ceil(Media.thumbnail_size * (file["height"]/file["width"]));
					}
					else {
						this.thumbnail_element.width = Media.thumbnail_size;
						this.thumbnail_element.height = Media.thumbnail_size;
					}
				}
				else { // Legacy: images uploaded before 0.1.1 don't have width and height saved
					this.thumbnail_element.width = this.thumbnail_element.height = Media.thumbnail_size;
				}
			}
			else {
				this.thumbnail_element.setAttribute("src", Media.thumbnail_placeholder_url);
				this.thumbnail_element.width = this.thumbnail_element.height = 64;
			}
			this.thumbnail_element.setAttribute("loading", "lazy");
			this.append(this.thumbnail_element);

		}
		let download_filename, caption_filename;
		if (filename_extension_i == -1) {
			download_filename = file["filename"].slice(0, file["filename"].length - Media.media_uuid_len);
			caption_filename = download_filename.length < 32 ? download_filename : (download_filename.substr(0, 28) + "(...)");
		}
		else {
			download_filename = file["filename"].slice(0, filename_extension_i - Media.media_uuid_len);
			caption_filename = download_filename.length < 32 ? download_filename : (download_filename.substr(0, 28) + "(...)");
			download_filename += file["filename"].slice(filename_extension_i, file["filename"].length);
			caption_filename += file["filename"].slice(filename_extension_i, file["filename"].length);
		}
		let figcaption_element = document.createElement("figcaption");
		let media_src = Media.media_url + file["filename"]/*.replace(" ", "%20")*/;
		let figcaption_element_a = document.createElement("a");
		figcaption_element_a.textContent = caption_filename;
		if (caption_filename.length !== download_filename.length)
			figcaption_element_a.title = download_filename;
		figcaption_element.appendChild(figcaption_element_a);
		if (show_file_as_deleted) {
			figcaption_element_a.textContent = "[DELETED] " + figcaption_element_a.textContent;
		}
		else {
			figcaption_element_a.setAttribute("href", media_src);
			figcaption_element_a.setAttribute("target", "_blank");

			let file_download_button = document.createElement("a");
			file_download_button.setAttribute("href", media_src);
			file_download_button.setAttribute("download", download_filename);
			file_download_button.setAttribute("title", "Download");
			file_download_button.innerText = "\ud83d\udce5\ufe0e";

			figcaption_element.append(document.createTextNode("\u00A0"), file_download_button);
		}
		this.appendChild(figcaption_element);
		return this;
	}
	whenClicked(target) {
		if (target.classList.contains("EmbedThumbnail"))
			this.expandEmbedFigure();
		else if (target.classList.contains("EmbedImage"))
			this.contractEmbedFigure();
		else if (target.classList.contains("EmbedIframe"))
			this.toggleEmbedIframe();
	}
	toggleEmbedIframe() {
		let iframe_element = this.querySelector("iframe");
		if (this.getAttribute("expanded") === "") {
			iframe_element.style["display"]="none";
			this.removeAttribute("expanded");
			iframe_element.src = "";
		}
		else {
			iframe_element.style["display"]="inherit";
			this.setAttribute("expanded", "");
			iframe_element.src = this.getAttribute("data-iframe-src");
		}
	}
	expandEmbedFigure() {
		if (this.embed_element) {
			this.embed_element.setAttribute("src", Media.media_url + this.getAttribute("filename"));
			this.embed_element.style["display"] = "";
			this.thumbnail_element.style["display"] = "none";
		}
	}
	contractEmbedFigure() {
		if (this.embed_element) {
			this.embed_element.style["display"] = "none";
			this.thumbnail_element.style["display"] = "";
		}
	}
}
customElements.define("media-embed", MediaEmbed)
