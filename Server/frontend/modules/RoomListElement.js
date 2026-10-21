import { global_styles } from "./styles.js";
import { session } from "./session.js";
import { API } from "./API.js";

class RoomListElement extends HTMLElement {
	static select_all_boards = "(all)";
	router = null;
	constructor() {
		// Always call super first in constructor
		super();
		const template = document.getElementById("room-list-template");
		const shadow = this.attachShadow({ mode: "open" });
		shadow.adoptedStyleSheets = [global_styles]; // imported at the top there
		shadow.appendChild(template.content.cloneNode(true));

		for (const element of shadow.querySelectorAll("[id]")) {
			this[element.id] = element;
		}

		this.board_select.onchange = () => {this.render();}
		this.create_room_button.onclick = async() => {
			await this.connectToNewRoom();
		}
	} // constructor()
	connectedCallback() {
		console.log("Custom element added to page.");
		session.refresh(); // fetches client info asynchronously
	}

	disconnectedCallback() {
		console.log("Custom element removed from page.");
	}

	// connectClientToRoom(room_id) {
	// 	room_list_container.style["display"] = "none";
	// 	room_connection = new RoomConnection(room_id);
	// }
	async connectToNewRoom() {
		// const room_id = await this.createNewRoom();
		this.router.navigate("/room/new");
	}
	/*
	async createNewRoom() {
		// TODO add optional board parameter, and maybe signalling
		const response = await fetch("/api/room", {method: "POST"});
		if (response.ok) {
			let new_room_id = Number(response.headers.get("new-room-id"));
			return new_room_id;
		}
		else {
			const error_message = response.headers.get("message") ?? `An unknown error occured when attempting to fetch ${thing}.`;
			throw error_message;
		}
	}
	*/
	// refresh_button.onclick = () => {getAllRooms();} // imported from navbar.html
	// document.addEventListener("on_room_exit", () => {  // imported from room.html
	// 	this.getAllRooms();
	// 	room_list_container.style["display"] = null;
	// });

	async render(get_boards = false) {
		this._abort_controller?.abort();
		this._abort_controller = new AbortController();
		const signal = this._abort_controller.signal;
		// this.multiple_selector_component?.clearSelection();

		let params = new URLSearchParams();
		if (this.board_select.value !== RoomListElement.select_all_boards) {
			params.set("board", this.board_select.value);
		}

		const params_string = params.toString();
		const url = new URL(window.location.href);
		url.search = params_string;
		history.pushState(null, null, url);
		console.log("params_string: " + params_string);
		if (get_boards) {
			await Promise.all([
				this.getAllBoards(signal),
				this.getAllRooms(params_string, signal)
			]);
		}
		else
			this.getAllRooms(params_string, signal)
	}

	async getAllBoards(signal) {
		const boards_json = await API.fetchJson("api/boards", {signal: signal}, "boards");
		if (signal.aborted)
			return;
		this.showBoardOptions(boards_json);
	}
	showBoardOptions(boards_json) {
		const fragment = new DocumentFragment();
		let option_element = document.createElement("option");
		option_element.value = RoomListElement.select_all_boards;
		option_element.textContent = RoomListElement.select_all_boards;
		fragment.appendChild(option_element);
		for (let board of boards_json.boards) {
			let option_element = document.createElement("option");
			option_element.value = board.slug;
			option_element.textContent = `${board.slug} - ${board.title}`;
			fragment.appendChild(option_element);
		}
		this.board_select.replaceChildren(fragment);
	}
	async getAllRooms(params_string, signal) {
		const rooms_json = await API.fetchJson(`./api/rooms?${params_string}`, {signal: signal}, "rooms");
		if (signal.aborted)
			return;
		this.showAllRooms(rooms_json);
	}

	showAllRooms(rooms_json) {
		const fragment = new DocumentFragment();
		for (let room of rooms_json.rooms) {
			// console.log(thread.id);
			// console.log(thread.post_zero);
			let room_element = document.createElement("tr");
			room_element.id = `room_${room.id}`;
			// Toolbar at the bottom of each thread
			const id_indicator = document.createElement("span");
			id_indicator.textContent = room_element.id;
			const number_of_peers = document.createElement("span");
			number_of_peers.innerHTML = `<b>${room.number_of_peers}</b>&nbsp;connected`;
			const connect_button = document.createElement("a");
			connect_button.textContent = "Connect";
			connect_button.href = "#/room/" + room.id;
			// connect_button.onclick = () => {
			// 	this.connectClientToRoom(room.id);
			// }
			[id_indicator, number_of_peers, connect_button].forEach(element => {
				const cell = document.createElement("td");
				cell.appendChild(element);
				room_element.appendChild(cell);
			})

			fragment.appendChild(room_element);
		}
		if (fragment.children.length === 0)
			this.room_list.replaceChildren(document.createTextNode("None."));
		else
			this.room_list.replaceChildren(fragment);
	}
}
customElements.define("room-list", RoomListElement);
