class Session extends EventTarget {
	server_permissions = {};
	highest_ranked_group = null;
	logged_in = null;

	async refresh() {
		const response = await fetch("api/user/client");
		if (response.ok) {
			const response_json = await response.json();
			this.server_permissions = response_json.server_permissions;
			this.highest_ranked_group = response_json.highest_ranked_group ?? null;
			this.logged_in = response_json["has_cookie"];
			this.dispatchEvent(new Event("change"));
		}
		else {
			throw response.headers.get("message") ?? "An unknown error occured when attempting to fetch client.";
		}
	}
}

export const session = new Session();
