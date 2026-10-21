// import { session } from "./session.js";
#include "./modules/API.js"

class BoardListElement extends HTMLElement {
	constructor() {
		// Always call super first in constructor
		super();
		const template = document.getElementById("board-list-template");
		const shadow = this.attachShadow({ mode: "open" });
		shadow.adoptedStyleSheets = [global_styles]; // imported from index.html
		shadow.appendChild(template.content.cloneNode(true));

		// for (const element of shadow.querySelectorAll("[id]")) {
		// 	this[element.id] = element;
		// }
		for (const element of shadow.querySelectorAll(["#create_board_button", "#board_list_container", "#manage_server_button"])) {
			this[element.id] = element;
		}
	}
	connectedCallback() {
		session.addEventListener("change", this.updateInterface);
		session.refresh(); // fetches client info asynchronously
	}

	disconnectedCallback() {
		session.removeEventListener("change", this.updateInterface);
	}
	updateInterface = () => {
		this.manage_server_button.style["display"] = session.server_permissions["manage_permissions"] ? "" : "none";
	}
	async render() {
		this._abort_controller?.abort();
		this._abort_controller = new AbortController();
		const signal = this._abort_controller.signal;
		let boards_json = await API.fetchJson("api/boards", {signal: signal}, "boards");
		if (signal.aborted)
			return;
		this.showAllBoards(boards_json);
	}

	showAllBoards(response_json) {
		this.create_board_button.style["display"] = response_json["client_permissions"]["create_board"] ? "" : "none";
		// const fragment = new DocumentFragment();
		const table_element = document.createElement("table");
		const headers_element = document.createElement("tr");
		["Slug", "Title", "Last post time", "Action"].forEach((column_name) => {
			let header = document.createElement("th");
			header.textContent = column_name;
			headers_element.append(header);
		});
		table_element.append(headers_element);
		for (let board of response_json.boards) {
			console.log(board);
			const board_element = document.createElement("tr");
			["slug", "title"].forEach((column_name) => {
				let col = document.createElement("td");
				col.textContent = board[column_name];
				board_element.append(col);
			});
			{
				let col = document.createElement("td");
				if (board["last_post_time"]) {
					let post_timestamp_element = document.createElement("time");
					let post_date = new Date(board["last_post_time"] * 1000 - timezone_offset_seconds * 1000).toISOString();;
					post_date = post_date.substring(0, 10) + " " + post_date.substring(11, 19);
					post_timestamp_element.textContent = post_date;
					col.appendChild(post_timestamp_element);
				}
				else {
					col.appendChild(document.createTextNode("No posts yet."));
				}
				board_element.append(col);
			}
			{
				let col = document.createElement("td");
				// if (board.client_permissions.manage_permissions) {
				// 	const manage_permissions_link = document.createElement("a");
				// 	manage_permissions_link.href = `manage_permissions.html?location=/board/${board.slug}`;
				// 	manage_permissions_link.textContent = "Manage permissions";
				// 	col.append(manage_permissions_link, document.createTextNode(" :: "));
				// }
				if (board["client_permissions"]["create_board"]) {
					const edit_board_link = document.createElement("a");
					edit_board_link.href = `edit_board.html?board=${board.slug}`;
					edit_board_link.textContent = "Edit";
					col.append(edit_board_link, document.createTextNode(" :: "));
				}
				const board_link = document.createElement("a");
				board_link.href = `#/board/${board.slug}`;
				board_link.textContent = "Open";
				col.append(board_link);
				board_element.append(col);
			}

			// const slug_element = document.createElement("td");
			// slug_element.textContent = board.slug;
			table_element.append(board_element);
		}
		if (response_json.boards.length === 0)
			this.board_list_container.replaceChildren(document.createTextNode("No boards visible to this client."));
		else {
			// fragment.appendChild(table_element);
			this.board_list_container.replaceChildren(table_element);
		}
	}
}

customElements.define("board-list", BoardListElement);
