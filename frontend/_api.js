"use strict";
/*
include(`tokens.m4')
*/

// Use when identity authorisation is required.
class API {
	static {
        API.api_url = "api/";
		API.error_dialog = document.createElement("dialog");
		API.error_dialog.setAttribute("closedby", "any");
		let dialog_header = document.createElement("h2");
		dialog_header.textContent = "Error";
		API.error_message = document.createElement("p");
		let dialog_toolbar = document.createElement("div");
		dialog_toolbar.classList.add("ToolBar");
		let close_button = document.createElement("button");
		close_button.textContent = "Close";
		close_button.onclick = () => { API.error_dialog.close(); }
		dialog_toolbar.append(close_button);
		API.error_dialog.append(dialog_header, API.error_message, dialog_toolbar);
		document.body.appendChild(API.error_dialog);
	}
	static getAccountToken() {
		let account_key = localStorage.getItem("account_key");
		let account_username = localStorage.getItem("account_username");
		if (account_key !== null && account_username !== null) {
			return `${account_key}.${account_username}`;
		}
		else {
			let local_key = localStorage.getItem("local_key");
			if (local_key !== null) {
				return `${local_key}.Public`;
			}
			else {
				throw new Error("account_key and/or account_username are null");
			}
		}
	}
	static async sendRequest(method, endpoint, headers = null, body = null) {
		let fetch_response;
		try {
			fetch_response = await fetch("_ROOT_URL" + API.api_url + endpoint + "/", {
				method: method,
				headers: {"token": API.getAccountToken(),
					...headers
				},
				body: body && JSON.stringify(body)
			});
			if (!fetch_response.ok) {
				let header_error_message = fetch_response.headers.get("message");
				if (header_error_message == null) {
					throw new Error(`${fetch_response.status} ${fetch_response.statusText}`);
				}
				else {
					throw new Error(`${fetch_response.status} ${fetch_response.statusText}\n${header_error_message}`);
				}
			}
		}
		catch (error) {
			API.error_message.innerText = error.message;
			API.error_dialog.showModal();
			console.error(error.message);
		}
		return fetch_response;
	}
}
