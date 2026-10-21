class Peer {
	id = 0;
	username = null;
	constructor(id) {
		this.id = id;
	}
	streams = {
		mic: null,
		desktop_audio: null,
		video: null
	};
	enabled = {
		mic: false,
		desktop_audio: false,
		video: false
	}
	trackEnabled(track_name) {
		if (typeof this.enabled[track_name] === "undefined")
			throw "invalid track_name " + track_name;
		else
			return this.enabled[track_name];
	}
	update(peer_info) {
		this.username = peer_info.username;
		["mic", "desktop_audio", "video"].forEach(track_name => {
			this.enabled[track_name] = peer_info[`${track_name}_sharing_enabled`];
		});
	}
}

class RoomConnection extends EventTarget {
	room_id = null;
	sock = null;
	peers = new Map();
	rtc_connection = null;
	can_share_media = false;
	websocket_url = (window.location.protocol === "http:" ? "ws://" : "wss://") + window.location.host + window.location.pathname;
	constructor(room_id = null) {
		super();
		this.on_room_creation_event = new Event("roomcreated");
		this.on_change_event = new Event("change");
		this.on_peers_updated_event = new Event("peersupdated");
		this.sock = new WebSocket(this.websocket_url);
		this.sock.onopen = (event) => {
			if (room_id === null)
				this.createRoom();
			else
				this.makeRtcConnection(room_id);
		}
		this.sock.onmessage = (event) => {
			try {
				let message_json = JSON.parse(event.data);
				console.log(message_json);
				if (message_json.type === "create_room_response") {
					this.makeRtcConnection(message_json.payload.room_id);
					this.dispatchEvent(this.on_room_creation_event);
					//this.room_creation_promise?.resolve(message_json.payload);
				}
				else if (message_json.type === "webrtc_offer") {
					console.log("calling onReceiveOffer");
					this.onReceiveOffer(message_json.payload);
				}
				else if (message_json.type === "webrtc_room_connect") {
					this.onReceiveMakeRtcConnection(message_json.payload);
				}
				else if (message_json.type === "peers_updated") {
					this.updatePeers(message_json.payload);
					this.dispatchEvent(this.on_peers_updated_event);
					// this.showPeers(message_json.payload.peers);
					// this.updateMediaElements(message_json.payload);
				}
				else if (message_json.type === "error") {
					error_message.textContent = message_json.error_message;
					error_dialog.showModal();
				}
				else if (message_json.type === "webrtc_relay_added") {
					this.onRelaySlotAdded(message_json.payload);
				}
				else {
					throw `message_json type ${message_json.type} not recognised.`;
				}
			}
			catch (error) {
				console.error(error);
				error_message.textContent = error;
				connection_status_indicator.textContent = "Errored.";
				error_dialog.showModal();
			}
		}
		this.sock.onclose = (event) => {
			this.peers.clear();
			this.dispatchEvent(this.on_peers_updated_event);
			this.dispatchEvent(this.on_change_event);
		}
	}
	updatePeers(payload) {
		for (let peer_info of Object.values(payload.peers)) {
			let id = Number(peer_info.connection_id);
			if (!this.peers.has(id))
				this.peers.set(id, new Peer(id));
			this.peers.get(id).update(peer_info);
		}
		for (let [id, peer] of this.peers) {
			if (!payload.peers[id])
				this.peers.delete(id);
		}
	}
	createRoom() {
		console.log("Creating new room");
		this.sock.send(JSON.stringify({
			"type": "create_room"
			// ,"board": state.selected_board.slug TODO restore board linking
		}));
	}
	isConnected() {
		return !(this.sock == null);
	}
	static MIC_AUDIO_MID = "ma";
	static DESKTOP_AUDIO_MID = "da";
	static VIDEO_MID = "v";
	micEnabled() {
		return !(this.mic_audio_track == null);
	}
	async enableMic() {
		if (this.micEnabled())
			return;
		const mic = await navigator.mediaDevices.getUserMedia({ audio: true });
		this.mic_audio_track = mic.getAudioTracks()[0];
		await this.mic_audio_transceiver.sender.replaceTrack(this.mic_audio_track);
		this.broadcastSharingStatus();
		// this.dispatchEvent(this.on_change_event);
	}

	async disableMic() {
		if (!this.micEnabled())
			return;
		await this.mic_audio_transceiver.sender.replaceTrack(null);
		this.mic_audio_track.stop(); // releases the device — OS indicator goes away
		this.mic_audio_track = null;
		this.broadcastSharingStatus();
		// this.dispatchEvent(this.on_change_event);
	}
	close() {
		this.mic_audio_track?.stop();
		this.video_sending_track?.stop();
		this.desktop_audio_track?.stop();
		this.rtc_connection?.close();
		if (this.sock.readyState < WebSocket.CLOSING)
			this.sock.close();
		this.peers.clear();
		this.sock.close();
		this.dispatchEvent(this.on_change_event);
		this.dispatchEvent(this.on_peers_updated_event);
	}
/*
	onDisconnectFromRoom(payload) {
		selected_room_id = -1;
		// connection_information_container.style["display"] = "none";
		room_container.style["display"] = "none";
		// document.getElementById(`room_${selected_room_id}`).classList.add("Connected");
	}*/
	async enableShareVideo() {
		room_share_button.disabled = true;
		try {
			if (share_screen_checkbox.checked) {
				const display = await navigator.mediaDevices.getDisplayMedia({ video: true, audio: true });
				this.video_sending_track = display.getVideoTracks()[0];
				console.log("depug information:");
				console.log(display.getAudioTracks());
				if (share_desktop_audio_checkbox.checked)
					this.desktop_audio_track = display.getAudioTracks()[0]; // undefined if the OS/browser doesn't support system audio capture
				else
					this.desktop_audio_track = null;
			}
			else {
				// probably webcam
				const cam = await navigator.mediaDevices.getUserMedia({ video: { width: 1280, height: 720 } });
				this.video_sending_track = cam.getVideoTracks()[0];
				this.desktop_audio_track = null;
			}
			await this.video_sending_transceiver.sender.replaceTrack(this.video_sending_track);
			await this.desktop_audio_transceiver.sender.replaceTrack(this.desktop_audio_track);
			this.broadcastSharingStatus();
			room_share_button.textContent = "Stop sharing video";
			room_share_button.onclick = () => { this.disableShareVideo(); }
		}
		catch (error) {
			RoomConnection.handleMediaError(error);
		}
		finally {
			room_share_button.disabled = false;
		}
	}
	async disableShareVideo() {
		room_share_button.disabled = true;
		await this.video_sending_transceiver.sender.replaceTrack(null);
		await this.desktop_audio_transceiver.sender.replaceTrack(null);
		this.video_sending_track?.stop();
		this.desktop_audio_track?.stop();
		this.video_sending_track = null;
		this.desktop_audio_track = null;
		room_share_button.textContent = "Share video";
		room_share_button.onclick = () => { this.enableShareVideo(); }
		room_share_button.disabled = false;
		this.broadcastSharingStatus();
	}

	makeRtcConnection(room_id) {
		this.room_id = room_id;
		this.sock.send(JSON.stringify({
			type: "webrtc_room_connect_request",
			room: room_id
		}));
	}

	async onReceiveMakeRtcConnection(payload) {
		this.can_share_media = payload.can_share_media;
		// selected_room_id = payload.room;
		this.rtc_connection = new RTCPeerConnection({
			// Recommended for libdatachannel
			bundlePolicy: 'max-bundle',
			iceServers: payload.ice_servers
		});
		this.dispatchEvent(this.on_change_event);
		console.log("[onReceiveWatchStream] audio_senders: " + JSON.stringify(payload.audio_senders));
		// listening_connections[payload.connection_id] = conn;
		this.rtc_connection.ontrack = (event) => { this.handleIncomingTrack(event); } // must be set before setRemoteDescription
		this.rtc_connection.onicegatheringstatechange = (state) => {
			console.log("[sender] ICE gathering state: " + this.rtc_connection.iceGatheringState);
			if (this.rtc_connection.iceGatheringState === 'complete') {
				this.dispatchEvent(this.on_change_event);
				// connection_status_indicator.textContent = "Connected.";
				const answer = this.rtc_connection.localDescription;
				this.sock.send(JSON.stringify({
					type: "webrtc_room_connect_answer",
					payload: {
						connection_id: payload.connection_id,
						// sender_connection_id: Number(sender_connection_input.value), // keyframe will be requested from this peer
						description: {
							type: answer.type,
							sdp: answer.sdp
						}
					}
				}))
			}
		}
		this.rtc_connection.onicecandidateerror = (event) => {
			console.error("ICE candidate error:", event.errorCode, event.errorText, event.url);
		};
		try {
			await this.rtc_connection.setRemoteDescription(payload.description);
			console.log("Done. Creating answer...");

			/////// Sending //////
			this.video_sending_track = null;
			this.mic_audio_track = null;
			this.desktop_audio_track = null;
			if (payload.can_share_media) {
			const transceivers = this.rtc_connection.getTransceivers();
			console.log("Transceivers: ", transceivers);
			this.video_sending_transceiver =  this.rtc_connection.getTransceivers().find(t => t.mid === RoomConnection.VIDEO_MID);
			this.mic_audio_transceiver =  this.rtc_connection.getTransceivers().find(t => t.mid === RoomConnection.MIC_AUDIO_MID);
			this.desktop_audio_transceiver =  this.rtc_connection.getTransceivers().find(t => t.mid === RoomConnection.DESKTOP_AUDIO_MID);
			this.video_sending_transceiver.direction = "sendonly";
			this.mic_audio_transceiver.direction = "sendonly";
			this.desktop_audio_transceiver.direction = "sendonly";
			}

			const answer = await this.rtc_connection.createAnswer();
			console.log("Done. Setting local description...");
			await this.rtc_connection.setLocalDescription(answer);
			console.log("Finished setting local description");
		}
		catch (err) {
			RoomConnection.handleMediaError(err, this.rtc_connection);
		}
	}
	getConnectionIdFromMid(mid) {
		return Number(mid.substring(mid.lastIndexOf('-') + 1));
	}
	handleIncomingTrack(event) {
		try {
			const mid = event.transceiver.mid;
			const connection_id = this.getConnectionIdFromMid(mid);
			console.log("got connection id");
			console.log(mid);
			console.log(connection_id);
			console.log(this.peers);
			if (!this.peers.has(connection_id)) {
				this.peers.set(connection_id, new Peer(connection_id));
				console.log(this.peers);
			}
			console.log("Incoming track MID " + mid);
			if (mid && mid.startsWith(RoomConnection.VIDEO_MID)) {
				this.peers.get(connection_id).streams.video = new MediaStream([event.track]);
			}
			else if (mid && mid.startsWith(RoomConnection.MIC_AUDIO_MID)) {
				this.peers.get(connection_id).streams.mic = new MediaStream([event.track]);
			}
			else if (mid && mid.startsWith(RoomConnection.DESKTOP_AUDIO_MID)) {
				this.peers.get(connection_id).streams.desktop_audio = new MediaStream([event.track]);
			}
			else {
				throw "Unrecognized MID " + mid;
			}
		}
		catch (err) {
			RoomConnection.handleMediaError(err);
		}
	}

	async onRelaySlotAdded(payload) {
		await this.rtc_connection.setRemoteDescription(payload.description);
		const answer = await this.rtc_connection.createAnswer();
		await this.rtc_connection.setLocalDescription(answer);
		this.sock.send(JSON.stringify({
			type: "webrtc_relay_added_answer",
			payload: {
				relay_name: payload.relay_name,
				description: { type: answer.type, sdp: answer.sdp }
			}
		}));
	}
	broadcastSharingStatus() {
		this.sock.send(JSON.stringify({
			type: "webrtc_sharing_status_update",
			payload: {
				mic_sharing_enabled: !!this.mic_audio_track,
				desktop_audio_sharing_enabled: !!this.desktop_audio_track,
				video_sharing_enabled: !!this.video_sending_track
			}
		}));
	}
	static handleMediaError(error, pc) {
		let error_content;
		switch (error.name) {
			case "NotAllowedError":
				error_content = "Client denied permission to access screen or camera.";
				break;
			case "NotFoundError":
				error_content = "Media device not found.";
				break;
			case "NotReadableError":
				error_content = "Media device not readable. Maybe it's being used by another application?";
				break;
			default:
				error_content = error.message ?? String(error);
		}
		console.error(error);
		error_message.textContent = error_content;
		error_dialog.showModal();
		pc?.close();
	}
}

class CallManager extends EventTarget {
	muted = true;
	deafened = false;
	connection = null;
	on_room_created_event = new Event("roomcreated");
	on_connection_change_event = new Event("connectionchange");
	on_connection_peers_updated_event = new Event("connectionpeersupdated");
	constructor() {
		super();
	}
	join(room_id) {
		this.connection = new RoomConnection(room_id);
		// relaying event listeners (maybe unnecessary)

		this.connection.addEventListener("roomcreated", () => {this.dispatchEvent(this.on_room_created_event)});
		this.connection.addEventListener("change", () => {this.dispatchEvent(this.on_connection_change_event)});
		this.connection.addEventListener("peersupdated", () => {
			console.log("[CallManager] caught peersupdated event");
			this.dispatchEvent(this.on_connection_peers_updated_event)});
	}
	leave() {
		this.connection.close();
		this.connection = null; // event listeners cleared
	}
	isConnected() {
		return !(this.connection == null);
	}
	getNumberOfPeers() {
		return this.connection ? this.connection.peers.length : 0;
	}
	getPeers() {
		return this.connection?.peers ?? new Map();
	}
	toggleMic() {
		this.muted ? this.enableMic() : this.disableMic();
	}
	enableMic() {
		this.muted = false;
		this.connection.enableMic();
		this.dispatchEvent(this.on_connection_change_event);
	}
	disableMic() {
		this.muted = true;
		this.connection.disableMic();
		this.dispatchEvent(this.on_connection_change_event);
	}
	canShareMedia() {
		return this.connection?.can_share_media;
	}
	getRoomId() {
		if (!this.connection)
			throw "can't getRoomId(): connection is null";
		else
			return this.connection.room_id;
	}
}
export const call_manager = new CallManager();
