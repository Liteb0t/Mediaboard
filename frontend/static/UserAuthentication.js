"use strict";

class UserAuthentication {
	constructor(username, password) {
		this.username = username;
		this.password = password;
	}
	async requestAccountParameters(new_account = false) {
		const url_extension = new_account ? "request_new_account_parameters" : "request_login_parameters";
		const request_new_account_parameters_response = await fetch("./registration/" + url_extension, {
			method: "POST",
			body: JSON.stringify({username: this.username})
		});
		// console.log(request_new_account_parameters_response);
		if (!(request_new_account_parameters_response && request_new_account_parameters_response.ok)) {
			let res_message = request_new_account_parameters_response.headers.get("message");
			throw new Error("Could not request account parameters: " + res_message ? res_message : request_new_account_parameters_response.statusText);
		}
		this.parameters_json = await request_new_account_parameters_response.json();
		// this.intermediate_salt_base64 = new_account_parameters_response_json["intermediate_salt_base64"];
		// this.salt_base64 = new_account_parameters_response_json["salt_base64"];
		// this.password_hash_length = new_account_parameters_response_json["password_hash_length"];
		// this.pwhash_opslimit = new_account_parameters_response_json["pwhash_opslimit"];
		// this.pwhash_memlimit = new_account_parameters_response_json["pwhash_memlimit"];
	}
	async calculateHash() {
		const hashed_password = await hashwasm.argon2id({
			password: this.password,
			salt: this.parameters_json["salt_base64"],
			iterations: this.parameters_json["pwhash_opslimit"],
			parallelism: 1,
			memorySize: this.parameters_json["pwhash_memlimit"] >> 10,
			hashLength: this.parameters_json["password_hash_length"],
			outputType: "binary"
		});
		return hashed_password.toBase64({alphabet: "base64url"});
	}
}
