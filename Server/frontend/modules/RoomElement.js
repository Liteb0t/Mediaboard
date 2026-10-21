import { call_manager } from "./RoomConnection.js";
import { global_styles } from "./styles.js";

class PeerMedia {
	constructor() {
		this.audio_elements = {}; // keyed by MID because sender can send multiple audio tracks
		this.video_element = null;
		this.video_track = null;
	}
	setAudio(event, mid) {
		const audio_element = document.createElement("audio");
		audio_element.autoplay = true;
		// audio_element.controls = true; // element is hidden otherwise
		audio_element.srcObject = new MediaStream([event.track]);
		this.audio_elements[mid] = audio_element;
		return this.audio_elements[mid];
	}
	showVideo(on_track_event = null) {
		if (this.video_element)
			return;
		if (on_track_event) {
			this.video_track = on_track_event.track;
		}
		if (this.video_track) {
			this.video_element = document.createElement("video");
			this.video_element.autoplay = true;
			this.video_element.playsInline = true;
			this.video_element.srcObject = new MediaStream([this.video_track]);
			video_container.appendChild(this.video_element);
		}
		else {
			console.log("[PeerMedia.showVideo()] There is no video_track from which to create a MediaStream.");
		}
		// this.video_element.dataset.senderConnectionId = sender_id;
		// return this.video_element;
	}
	showVideoIfSharing(peer_info) {
		peer_info.video_sharing_enabled ? this.showVideo() : this.hideVideo();
	}
	hideVideo() {
		this.video_element?.parentNode.removeChild(this.video_element);
		this.video_element = null;
	}
	onLeave() {
		this.video_element?.parentNode.removeChild(this.video_element);
		this.audio_element?.parentNode.removeChild(this.audio_element);
	}
}

class RoomElementPeerElements {
	video = null;
	constructor(id) {
		this.video = document.createElement("video");
		this.video.autoplay = true;
		this.video.playsInline = true;
		this.table_entry = {row: document.createElement("tr")};
	}
	update(peer) {
		if (peer.streams.video && !this.video.srcObject) {
			this.video.srcObject = peer.streams.video;
		}
		let row_element = document.createElement("tr");
		["id", "username"].forEach(column_name => {
			if (!this.table_entry[column_name]) {
				this.table_entry[column_name] = document.createElement("td");
				this.table_entry.row.appendChild(this.table_entry[column_name]);
			}
			this.table_entry[column_name].textContent = peer[column_name];
		});
		["mic", "video", "desktop_audio"].forEach(stream_name => {
			if (!this.table_entry[stream_name]) {
				this.table_entry[stream_name] = document.createElement("td");
				this.table_entry.row.appendChild(this.table_entry[stream_name]);
			}
			this.table_entry[stream_name].textContent = peer.trackEnabled(stream_name);
		});
		// else if (this.video.srcObject && !peer.streams.video)
	}
	unset() {
		this.video.srcObject = null;
		this.video.remove();
		this.table_entry.row.remove();
	}
}

class RoomElement extends HTMLElement {
	peer_elements = new Map();
	constructor() {
		// Always call super first in constructor
		super();
		const template = document.getElementById("room-template");
		const shadow = this.attachShadow({ mode: "open" });
		shadow.adoptedStyleSheets = [global_styles]; // imported at the top there
		shadow.appendChild(template.content.cloneNode(true));

		for (const element of shadow.querySelectorAll("[id]")) {
			this[element.id] = element;
		}
	} // constructor()
	connectedCallback() {
		console.log("Custom element added to page.");
		call_manager.addEventListener("roomcreated", this.onRoomCreation);
		call_manager.addEventListener("connectionchange", this.onConnectionChange);
		call_manager.addEventListener("connectionpeersupdated", this.onConnectionPeersUpdate);
	}
	disconnectedCallback() {
		console.log("Custom element removed from page.");
		call_manager.removeEventListener("roomcreated", this.onRoomCreation);
		call_manager.removeEventListener("connectionchange", this.onConnectionChange);
		call_manager.removeEventListener("connectionpeersupdated", this.onConnectionPeersUpdate);
	}
	render(room_id) {
		this.setPeerDisplays(call_manager.getPeers());
		call_manager.join(room_id);
	}
	onRoomCreation = () => {
		const url = new URL(window.location.href);
		url.hash = `#/room/${call_manager.getRoomId()}`;
		history.replaceState(null, null, url);
	}
	onConnectionPeersUpdate = () => {
		console.log("onConnectionPeersUpdate called");
		this.setPeerDisplays(call_manager.getPeers());
		/*
		updateMediaElements(payload) {
			if (payload.action.type == "join") {
				this.peer_media[payload.action.peer] = new PeerMedia();
			}
			else if (payload.action.type == "update") {
				this.peer_media[payload.action.peer]?.showVideoIfSharing(payload.peers[payload.action.peer]);
			}
			else if (payload.action.type == "leave") {
				this.peer_media[payload.action.peer]?.onLeave();
			}
		}*/
	}
	setPeerDisplays(peers) {
		for (const [id, peer] of peers) {
			if (!this.peer_elements.has(id)) {
				this.peer_elements.set(id, new RoomElementPeerElements());
			}
			this.peer_elements.get(id).update(peer);

			if (!this.video_container.contains(this.peer_elements.get(id).video))
				this.video_container.appendChild(this.peer_elements.get(id).video);
			if (!this.peers_list.contains(this.peer_elements.get(id).table_entry.row))
				this.peers_list.appendChild(this.peer_elements.get(id).table_entry.row);
		}
		for (const [id, peer] of this.peer_elements) {
			if (!peers.has(id)) {
				this.peer_elements.get(id).unset();
				this.peer_elements.delete(id);
			}
		}
	}
	onConnectionChange = () => {
		this.room_share_button.disabled = !call_manager.canShareMedia();
		if (!call_manager.isConnected()) {
			this.setPeerDisplays(call_manager.getPeers());
			// for (const element of this.peer_elements.values())
			// 	element.ParentNode().removeChild(element);
			// this.peer_elements.clear();
			// this.video_container.innerHTML = "";
			this.connection_status_indicator.textContent = "Not connected.";
		}
		else if (call_manager.connection.rtc_connection.iceGatheringState === 'complete')
			this.connection_status_indicator.textContent = "Connected";
		else
			this.connection_status_indicator.textContent = "Connecting...";
	}
}
customElements.define("room-container", RoomElement);
