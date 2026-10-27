import { call_manager } from "./RoomConnection.js";
import { global_styles } from "./styles.js";

class RoomElementPeerElements {
	video = null;
	constructor(id) {
		this.table_entry = {row: document.createElement("tr")};
	}
	static createVideo(source_stream) {
		const video = document.createElement("video");
		video.autoplay = true;
		video.playsInline = true;
		video.srcObject = source_stream;
		console.log(video.srcObject.getVideoTracks()[0].muted);
		return video;
	}
	update(peer) {
		if (peer.trackEnabled("video") && peer.streams.video) {
			if (!this.video)
				this.video = RoomElementPeerElements.createVideo(peer.streams.video);
			if (this.video.srcObject !== peer.streams.video)
				this.video.srcObject = peer.streams.video;
		}
		else if (this.video) {
			this.video.srcObject = null;
			this.video.remove();
			this.video = null;
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
	updateVisibility(video_container, peer_list_container) {
		if (this.video && !video_container.contains(this.video))
			video_container.appendChild(this.video);
		if (!peer_list_container.contains(this.table_entry.row))
			peer_list_container.appendChild(this.table_entry.row);
	}
	unset() {
		if (this.video) {
			this.video.srcObject = null;
			this.video.remove();
		}
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
		this.room_share_button.onclick = async() => {
			this.room_share_button.disabled = true;
			if (await call_manager.connection?.toggleShareVideo(this.share_screen_checkbox.checked, this.share_desktop_audio_checkbox.checked)) {
				this.room_share_button.textContent = "Stop sharing video";
			}
			else {
				this.room_share_button.textContent = "Share video";
			}
			this.room_share_button.disabled = false;
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
	}
	setPeerDisplays(peers) {
		for (const [id, peer] of peers) {
			if (!this.peer_elements.has(id)) {
				this.peer_elements.set(id, new RoomElementPeerElements());
			}
			this.peer_elements.get(id).update(peer);
			this.peer_elements.get(id).updateVisibility(this.video_container, this.peers_list);
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
			this.disconnected_dialog.showModal();
		}
		else if (call_manager.connection.rtc_connection.iceGatheringState === 'complete')
			this.connection_status_indicator.textContent = "Connected";
		else
			this.connection_status_indicator.textContent = "Connecting...";
	}
}
customElements.define("room-container", RoomElement);
