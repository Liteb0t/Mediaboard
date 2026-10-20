export class API {
	static async fetchJson(location, args, thing = "something") {
		const response = await fetch(location, args);
		if (response.ok) {
			const response_json = await response.json();
			return response_json;
		}
		else {
			const error_message = response.headers.get("message") ?? `An unknown error occured when attempting to fetch ${thing}.`;
			throw error_message;
		}
	}
}
