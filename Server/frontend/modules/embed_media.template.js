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
	static media_uuid_len = 36;
	static thumbnail_size = CONFIG_thumbnail_size;
	static FIGURE_EMBED = 0; // Main <img> or <video> is child 0 of <figure>, thumbnail is child 1.
	static FIGURE_THUMBNAIL = 1;
	static handleOnClick = (event) => {
		let target = event.target;
		console.log(target);
		if (target.classList.contains("EmbedThumbnail"))
			Media.expandEmbedFigure(target.parentNode);
		else if (target.classList.contains("EmbedImage"))
			Media.contractEmbedFigure(target.parentNode);
		else if (target.classList.contains("EmbedIframe"))
			Media.toggleEmbedIframe(target.parentNode);
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
		figure_element.children[Media.FIGURE_EMBED].setAttribute("src", Media.media_url + figure_element.getAttribute("filename"));
		figure_element.children[Media.FIGURE_EMBED].style["display"] = "inherit";
		figure_element.children[Media.FIGURE_THUMBNAIL].style["display"] = "none";
	}
	static contractEmbedFigure(figure_element) {
		figure_element.children[Media.FIGURE_EMBED].style["display"] = "none";
		figure_element.children[Media.FIGURE_THUMBNAIL].style["display"] = "inherit";
	}
	static createMediaEmbed(file) {
		return document.createElement("media-embed").render(file);
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
	render(file) {
		this.classList.add("EmbedFigure");
		// let figure_element = document.createElement("figure");
		this.setAttribute("filename", file["filename"].replace(" ", "%20"));
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
				this.appendChild(img_element);
			}
			else if (file_format === Format.VIDEO) {
				let video_element = document.createElement("video");
				video_element.classList.add("EmbedVideo");
				video_element.controls = true;
				video_element.disablePictureInPicture = true;
				video_element.setAttribute("controlslist", "nofullscreen nodownload");
				video_element.style["display"] = "none";
				this.appendChild(video_element);
			}

			// Thumbnail
			let thumbnail_element = document.createElement("img");
			thumbnail_element.classList.add("EmbedThumbnail");
			let thumbnail_filename = file.thumbnail_file_extension ? `THUMBNAIL_${file.filename}.${file.thumbnail_file_extension}` : "THUMBNAIL_" + file["filename"] + ".CONFIG_thumbnail_file_extension";
			thumbnail_element.setAttribute("src", Media.thumbnail_url + thumbnail_filename.replace(" ", "%20"));
			if (file["width"] && file["height"]) {
				if (file["width"] < file["height"]) {
					thumbnail_element.width = file["width"] < file["height"]>>1 ? Math.ceil(Media.thumbnail_size / 2) : Math.ceil(Media.thumbnail_size * (file["width"]/file["height"]));
					thumbnail_element.height = Media.thumbnail_size;
				}
				else if (file["height"] < file["width"]) {
					thumbnail_element.width = Media.thumbnail_size;
					thumbnail_element.height = file["height"] < file["width"]>>1 ? Math.ceil(Media.thumbnail_size / 2) : Math.ceil(Media.thumbnail_size * (file["height"]/file["width"]));
				}
				else {
					thumbnail_element.width = Media.thumbnail_size;
					thumbnail_element.height = Media.thumbnail_size;
				}
			}
			else { // Legacy: images uploaded before 0.1.1 don't have width and height saved
				thumbnail_element.width = thumbnail_element.height = Media.thumbnail_size;
			}
			thumbnail_element.setAttribute("loading", "lazy");
			this.append(thumbnail_element);
		}
		else if (file_format === Format.AUDIO) {
			let audio_element = document.createElement("audio");
			audio_element.controls = true;
			audio_element.setAttribute("controlslist", "nofullscreen nodownload");
			audio_element.setAttribute("src", Media.media_url + file["filename"].replace(" ", "%20"));
			this.appendChild(audio_element);
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
		let media_src = Media.media_url + file["filename"].replace(" ", "%20");
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
		this.appendChild(figcaption_element);
		return this;
	}
}
customElements.define("media-embed", MediaEmbed)
