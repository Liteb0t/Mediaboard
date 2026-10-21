import { global_styles } from "./styles.js";
import { call_manager } from "./RoomConnection.js";

class RoomBarElement extends HTMLElement {
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
	}
	disconnectedCallback() {
		console.log("Custom element removed from page.");
		call_manager.removeEventListener("connectionchange", this.onConnectionChange);
	}
	onMicButtonToggle = async() => {
		this.mic_toggle_button.disabled = true;
		call_manager.toggleMic();
	}
	onConnectionChange = () => {
		console.log("[RoomBarElement] onConnectionChange fired");
		this.mic_toggle_button.textContent = call_manager.muted ? "Unmute" : "Mute";
		this.hidden = !call_manager.isConnected();
		this.mic_toggle_button.disabled = !call_manager.connection?.can_share_media;
	}
	async render() {
		this.connected_indicator.textContent = `${call_manager.getNumberOfPeers()} peers`;
		this.connected_indicator.hidden = true;
	}
}
customElements.define("room-bar", RoomBarElement);
