import { global_styles } from "./styles.js";
import { call_manager } from "./RoomConnection.js";

class RoomBarAudioElements {
	audio_elements = {"mic": null, "desktop_audio": null};
	static createAudio(source_stream) {
		const audio_element = document.createElement("audio");
		audio_element.autoplay = true;
		// audio_element.controls = true; // element is hidden when this is out commented
		audio_element.srcObject = source_stream;
		return audio_element;
	}
	update(peer) {
		["mic", "desktop_audio"].forEach(track_name => {
			if (peer.trackEnabled(track_name) && peer.streams[track_name]) {
				if (!this.audio_elements[track_name])
					this.audio_elements[track_name] = RoomBarAudioElements.createAudio(peer.streams[track_name]);
				if (this.audio_elements[track_name].srcObject !== peer.streams[track_name])
					this.audio_elements[track_name].srcObject = peer.streams[track_name];
			}
			else if (this.audio_elements[track_name]) {
				this.audio_elements[track_name].srcObject = null;
				this.audio_elements[track_name].remove();
				this.audio_elements[track_name] = null;
			}
		});
	}
	updateVisibility(audio_container) {
		["mic", "desktop_audio"].forEach(track_name => {
			if (this.audio_elements[track_name] && !audio_container.contains(this.audio_elements[track_name]))
				audio_container.appendChild(this.audio_elements[track_name]);
		});
	}
	unset() {
		["mic", "desktop_audio"].forEach(track_name => {
			if (this.audio_elements[track_name]) {
				this.audio_elements[track_name].srcObject = null;
				this.audio_elements[track_name].remove();
			}
		});
	}
}

class RoomBarElement extends HTMLElement {
	peer_elements = new Map();
	constructor() {
		// Always call super first in constructor
		super();
		const template = document.getElementById("room-bar-template");
		const shadow = this.attachShadow({ mode: "open" });
		shadow.adoptedStyleSheets = [global_styles]; // imported at the top there
		shadow.appendChild(template.content.cloneNode(true));

		for (const element of shadow.querySelectorAll("[id]")) {
			this[element.id] = element;
		}
		this.mic_toggle_button.onclick = this.onMicButtonToggle;
		this.leave_button.onclick = () => {call_manager.leave();}
	} // constructor()
	connectedCallback() {
		console.log("Custom element added to page.");
		call_manager.addEventListener("connectionchange", this.onConnectionChange);
		call_manager.addEventListener("connectionpeersupdated", this.onConnectionPeersUpdated);
	}
	disconnectedCallback() {
		console.log("Custom element removed from page.");
		call_manager.removeEventListener("connectionchange", this.onConnectionChange);
		call_manager.removeEventListener("connectionpeersupdated", this.onConnectionPeersUpdated);
	}
	onMicButtonToggle = async() => {
		this.mic_toggle_button.disabled = true;
		call_manager.toggleMic();
	}
	onConnectionPeersUpdated = () => {
		this.connected_indicator.textContent = `${call_manager.getNumberOfPeers()} in room`;
		this.setPeerDisplays(call_manager.getPeers());
	}
	onConnectionChange = () => {
		console.log("[RoomBarElement] onConnectionChange fired");
		this.mic_toggle_button.textContent = call_manager.muted ? "Unmute" : "Mute";
		this.hidden = !call_manager.isConnected();
		if (call_manager.isConnected())
			this.connected_indicator.href = `#/room/${call_manager.connection.room_id}`;
		this.mic_toggle_button.disabled = !call_manager.connection?.can_share_media;
	}
	setPeerDisplays(peers) { // almost duplicate from RoomElement.js
		for (const [id, peer] of peers) {
			if (!this.peer_elements.has(id)) {
				this.peer_elements.set(id, new RoomBarAudioElements());
			}
			this.peer_elements.get(id).update(peer);
			this.peer_elements.get(id).updateVisibility(this.audio_container);
		}
		for (const [id, peer] of this.peer_elements) {
			if (!peers.has(id)) {
				this.peer_elements.get(id).unset();
				this.peer_elements.delete(id);
			}
		}
	}
	render() {
		this.hidden = true;
		this.classList.add("ToolBar");
		// this.connected_indicator.textContent = `${call_manager.getNumberOfPeers()} peers`;
	}
}
customElements.define("room-bar", RoomBarElement);
