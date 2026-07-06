"use strict";

class PRESET_GROUPS {
	static ADMINISTRATORS = 0;
	static USERS = 1;
	static PUBLIC = 2;
}

class UserListFactory {
	constructor() {
		this.users_json = {};
		this.user_lists = [];
	}
	addUserList(user_list) {
		this.user_lists.push(user_list);
	}
	async refreshUsers() {
		let response = await fetch("./api/users");
		if (response.ok) {
			let response_json;
			try {
				response_json = await response.json()
			}
			catch (error) {
				error_message.innerText = `Could not parse users JSON`;
				error_dialog.showModal();
				return null;
			}
			this.users_json = response_json["users"];
			// console.log(this.users_json);
			this.client_rank = Number(response.headers.get("Client-Rank"));
			for (let user_list of this.user_lists) {
				user_list.refresh(this.users_json);
			}
		}
		else {
			error_message.innerText = "Could not retrieve users from API.";
			error_dialog.showModal();
		}
	}
}
const user_list_factory = new UserListFactory();

class GroupListFactory {
	constructor() {
		this.groups_json = {};
		this.group_lists = [];
	}
	addGroupList(group_list) {
		this.group_lists.push(group_list);
	}
	async refreshGroups() {
		let response = await fetch("./api/groups");
		if (response.ok) {
			let response_json;
			try {
				response_json = await response.json()
			}
			catch (error) {
				error_message.innerText = "Could not parse groups JSON\n";
				error_dialog.showModal();
				return null;
			}
			this.groups_json = response_json;
			// console.log(response_json);
			for (let group_list of this.group_lists) {
				group_list.refresh(this.groups_json);
			}
		}
		else {
			console.log(response);
			console.error("Response is not ok");
		}
	}
}
const group_list_factory = new GroupListFactory();

class Group {
	constructor(group_list, group_json, click_function) {
		this.group_list = group_list;
		this.id = group_json.id;
		this.name = group_json.name;
		this.permission_editable = group_json.permission_editable ?? false;
		this.element = document.createElement("li");
		this.element.id = `group-${this.id}`;
		this.element.classList.add("ToolBar");
		this.label = document.createElement("button");
		this.label.classList.add("ToolBarButton");
		this.label.style["flex-grow"] = 1;
		this.label.textContent = this.name;
		this.label.onclick = () => { console.log("group " + this.name + "clicked"); this.group_list.groupClickEvent(this); }
		this.element.append(this.label);
	}
}

class GroupList {
	constructor(group_list_container) {
		this.group_list_container = group_list_container;
		this.selected_group = null;
		this.json = null;
	}
	/*
	appendGroup(group_json) {
		let new_group = new Group(this, group_json);
		this.group_list_container.appendChild(new_group.element);
	}*/
	createGroup(group_json) {
		let new_group = new Group(this, group_json);
		return new_group;
	}
	groupClickEvent(group) {
		this.selected_group = group;
		console.log(`Group ${group.id} clicked.`);
	}
	getGroupIdFromElement(element) {
		let group_id = Number(element.id.substring("group-".length));
		return group_id;
	}
	getList() {
		let as_list = [];
		for (let group_list_entry of this.group_list_container.children) {
			let group_id = this.getGroupIdFromElement(group_list_entry);
			as_list.push(group_id);
		}
		return as_list;
	}
}

class HeirarchyEditableGroup extends Group {
	constructor(group_list, group_json) {
		super(group_list, group_json);
		this.heirarchy_editable = group_json["heirarchy_editable"];
		// this.client_editable = group_json["permission_editable"];
		if (this.heirarchy_editable) {
			this.up_button = document.createElement("button");
			this.up_button.classList.add("RightAligned");
			this.up_button.textContent = "\u2BC5";
			this.up_button.onclick = () => { this.moveUp(); };
			this.down_button = document.createElement("button");
			// this.down_button.classList.add("ToolBarButton");
			this.down_button.textContent = "\u2BC6";
			this.down_button.onclick = () => { this.moveDown(); };
			this.element.append(this.up_button, this.down_button);
		}
	}
	moveUp() {
		let previous_element = this.element.previousElementSibling;
		if (previous_element !== null) {
			previous_element.insertAdjacentElement("beforebegin", this.element);
		}
		this.updateHeirarchy();
	}
	moveDown() {
		let next_element = this.element.nextElementSibling;
		if (next_element !== null) {
			next_element.insertAdjacentElement("afterend", this.element);
		}
		this.updateHeirarchy();
	}
	async updateHeirarchy() {
		this.element.classList.add("Processing");
		let heirarchy = this.group_list.getList();
		// let group_to_move = this.element.previousElementSibling;

		let response = await this.group_list.sendGroupHeirarchyToServer(heirarchy);
		// console.log(response);
		// console.log(response.ok);
		// TODO don't call refresh unnecessarily
		group_list_factory.refreshGroups();
	}
}

class ManageGroupsGroupList extends GroupList {
	constructor(group_list_container, group_manager, delete_button, rename_button) {
		super(group_list_container);
		this.group_manager = group_manager;
		this.selected_group = null;
		this.selected_group_indicator = selected_group_indicator;
		this.delete_button = delete_button;
		this.rename_button = rename_button;
		this.delete_button.onclick = async() => {
			this.delete_button.disabled = true;
			let response = await fetch(`./api/group/${this.selected_group.id}`, {method: "DELETE"});
			if (response.ok) {
				group_list_factory.refreshGroups();
				this.selected_group_indicator.textContent = "None selected";
				this.rename_button.disabled = true;
			}
			else {
				this.delete_button.disabled = false;
			}
		}
	}
	/*
	appendGroup(group_json) {
		let new_group = new HeirarchyEditableGroup(this, group_json);
		this.group_list_container.appendChild(new_group.element);
	}*/
	createGroup(group_json) {
		let new_group = new HeirarchyEditableGroup(this, group_json);
		return new_group;
	}
	refresh(groups_json) {
		const fragment = new DocumentFragment();
		let previous_group_not_editable;
		this.groups_json = groups_json;
		// console.log(groups_json);
		for (let rank = 1; rank <= groups_json["group_heirarchy"].length; rank++) {
			let group_id = groups_json["group_heirarchy"][rank-1];
			let group_json = groups_json["groups"][group_id];
			// console.log(group_json);
			// TODO do not make new Group objects unnecessarily
			// this.appendGroup(group_json);
			let new_group = this.createGroup(group_json);
			if (group_json.id === PRESET_GROUPS.USERS ||
					group_json.id === PRESET_GROUPS.PUBLIC) {
				new_group.element.style["display"] = "none";
			}
			else {
				// heirarchy_editable groups enable up/down buttons by default.
				if (group_json.heirarchy_editable && previous_group_not_editable) {
					new_group.up_button.disabled = true;
				}
				// can't move down the final visible group in the list
				if (group_json.heirarchy_editable && rank === groups_json["group_heirarchy"].length - 2) {
					new_group.down_button.disabled = true;
				}
			}
			if (!group_json.heirarchy_editable) {
				previous_group_not_editable = true;
			}
			else {
				previous_group_not_editable = false;
			}
			fragment.appendChild(new_group.element);
		}
		this.group_list_container.replaceChildren(fragment);
	}
	groupClickEvent(group) {
		this.selected_group = group;
		this.selected_group_indicator.textContent = this.selected_group.name;
		this.group_manager.member_list.showMembersInGroup(this.selected_group.id);
		this.setSelectedGroupLabel();
	}
	setSelectedGroupLabel() {
		this.selected_group_indicator.textContent = this.selected_group.name;
		if (this.selected_group.permission_editable) {
			this.rename_button.disabled = false;
			this.delete_button.disabled = false;
		}
		else {
			this.rename_button.disabled = true;
			this.delete_button.disabled = true;
		}
	}
	async sendGroupHeirarchyToServer(new_group_heirarchy) {
		let data_to_send = {
			new_group_heirarchy: new_group_heirarchy,
			username: localStorage.getItem("account_username"),
			key: localStorage.getItem("account_key")
		};
		// for (let element of group_list.children) {
		// 	let group_id = Number(element.id.substring("group-".length));
		// 	data_to_send.new_group_heirarchy.push(group_id);
		// }
		let response = await fetch("./api/group_heirarchy", {method: "PUT", body: JSON.stringify(data_to_send)});
		return response;
	}
	async createNewGroup() {
		let new_group_data = {
			"group": {
				name: create_group_form_group_name.value
			}
		};
		let response = await fetch("./api/create_group", {method: "POST", body: JSON.stringify(new_group_data)});
		if (response.ok) {
			group_list_factory.refreshGroups();
			// const new_group_id = response.headers.get("New-Group-ID");
			// console.log("New group ID: " + new_group_id);
			// const new_group = new Group(new_group_id, new_group_data["group"].name, new_group_data["group"].permissions);
			// groups[new_group_id] = new_group;
			// new_group.moveUp();
		}
		create_group_form_group_name.value = "";
	}
}

class User {
	constructor(user_list, user_json, clickable = false) {
		// console.log(user_json);
		this.user_list = user_list;
		this.id = user_json["id"];
		this.rank = user_json["rank"];
		this.element = document.createElement("li");
		this.element.id = `user-${this.id}`;
		this.element.classList.add("ToolBar");
		this.username = user_json["username"];
		this.permission_editable = user_json.permission_editable ?? false;

		if (clickable) {
			this.label = document.createElement("button");
			this.label.classList.add("ToolBarButton");
			this.label.style["flex-grow"] = 1;
			this.label.onclick = () => { this.user_list.userClickEvent(this); }
		}
		else {
			this.label = document.createElement("span");
		}
		this.label.textContent = this.username;
		this.element.appendChild(this.label);
	}
}

class UserList {
	constructor(user_list_container) {
		this.user_list_container = user_list_container;
		this.selected_user = null;
	}
	createUser(user_json) {
		// console.log(user_json);
		let new_user = new User(this, user_json);
		return new_user;
	}

	async refresh(users_json) {
		const fragment = new DocumentFragment();
		for (let user_json of Object.values(users_json)) {
			let new_user = this.createUser(user_json);
			fragment.appendChild(new_user.element);
		}
		this.user_list_container.replaceChildren(fragment);
	}
}

class ManageUsersUser extends User {
	constructor(user_list, user_json) {
		super(user_list, user_json);
		this.element.appendChild(this.label);
		this.element.classList.add("WithBorders");
		for (let group_json of user_json["groups"]) {
			let group_element = document.createElement("span");
			group_element.textContent = group_json["name"];
			this.element.appendChild(group_element);
		}
		this.add_group_button = document.createElement("button");
		this.add_group_button.classList.add("ToolBarButton", "RightAligned");
		this.add_group_button.textContent = "Add group";
		if (this.rank <= user_list_factory.client_rank) {
			this.add_group_button.disabled = true;
		}
		else {
			this.add_group_button.onclick = () => {
				this.user_list.selected_user = this;
				add_user_to_group_dialog_username.textContent = this.username;
				add_user_to_group_dialog_group_list_element
					.querySelectorAll("input")
					.forEach(element => {
						const list_group_id = Number(element.id.substring("add_user_to_group_dialog_group_".length));
						for (let user_group of user_list_factory.users_json[this.id]["groups"]) {
							if (user_group.id == list_group_id) {
								element.style["display"] = "none";
								add_user_to_group_dialog_group_list_element.querySelector(`label[for=${element.id}]`).style["display"] = "none";
								return;
							}
						}
						element.style["display"] = "initial";
						add_user_to_group_dialog_group_list_element.querySelector(`label[for=${element.id}]`).style["display"] = "initial";
					});
				add_user_to_group_dialog.showModal();
			}
		}
		this.element.append(this.add_group_button);
		// this.user_list.user_list_container.appendChild(this.element);
	}
}

class ManageUsersUserList extends UserList {
	constructor(user_list_container) {
		super(user_list_container)
	}
	createUser(user_json) {
		let new_user = new ManageUsersUser(this, user_json);
		return new_user;
	}
}

class ManageGroupsUser extends User {
	constructor (member_list, member_json) {
		super(member_list, member_json);
		this.element.classList.add("WithBorders");
		this.remove_button = document.createElement("button");
		this.remove_button.classList.add("ToolBarButton");
		this.remove_button.textContent = "Dismiss";
		if (!this.permission_editable) {
			this.remove_button.disabled = true;
		}
		this.remove_button.onclick = async() => {
			// this.member_list.removeClickEvent();
			let response = await fetch(`./api/group/${this.user_list.group_manager.group_list.selected_group.id}/member/${this.id}`, {method: "DELETE"});
			if (response.ok) {
				this.user_list.user_list_container.removeChild(this.element);
				user_list_factory.refreshUsers();
			}
		}
		this.element.append(this.remove_button);
	}
}

class ManageGroupsMemberList extends UserList {
	constructor(member_list_container, group_manager) {
		super(member_list_container);
		this.group_manager = group_manager;
	}
	createMember(member_json) {
		let new_member = new ManageGroupsUser(this, member_json);
		return new_member;
	}
	showMembersInGroup(group) {
		this.selected_group = group;
		this.refresh();
	}
	async refresh() {
		if (this.group_manager.group_list.selected_group != null) {
			let response = await fetch(`./api/group/${this.group_manager.group_list.selected_group.id}/members`);
			if (response.ok) {
				let response_json;
				try {
					response_json = await response.json()
				}
				catch (error) {
					error_message.innerText = "Could not parse members JSON\n";
					error_dialog.showModal();
					return null;
				}
				// console.log(response_json);
				const fragment = new DocumentFragment();
				/*
				for (let member_json of Object.values(response_json["members"])) {
					let new_member = this.createMember(member_json);
					fragment.appendChild(new_member.element);
				}
				*/
				for (let user_id of response_json) {
					let user_json = user_list_factory.users_json[user_id];
					let new_member = this.createMember(user_json);
					fragment.appendChild(new_member.element);
				}
				this.user_list_container.replaceChildren(fragment);
			}
		}
		else {
			this.user_list_container.textContent = "Select a group to view the members in that group.";
		}
	}
}

class GroupManager {
	constructor(group_list_element, member_list_element, delete_button, rename_button) {
		this.group_list = new ManageGroupsGroupList(group_list_element, this, delete_button, rename_button);
		group_list_factory.addGroupList(this.group_list);
		this.member_list = new ManageGroupsMemberList(member_list_element, this);
		user_list_factory.addUserList(this.member_list);
	}
}

class PermissionSettingsGroup extends Group {
	constructor(group_list, group_json) {
		super(group_list, group_json);
		this.remove_button = document.createElement("button");
		this.remove_button.classList.add("RightAligned");
		this.remove_button.textContent = "Remove";
		// this.client_editable = group_list_factory.groups_json.groups[this.id]["permission_editable"];
		if (this.permission_editable === false) {
			this.remove_button.disabled = true;
		}
		else {
			this.remove_button.onclick = async() => {
				await this.group_list.removeGroup(this);
			}
		}
		this.element.appendChild(this.remove_button);
	}
}

class PermissionSettingsGroupList extends GroupList {
	constructor(permission_settings_object, group_list_container) {
		super(group_list_container);
		this.permission_settings_object = permission_settings_object;
	}
	createGroup(group_json) {
		let new_group = new PermissionSettingsGroup(this, group_json);
		return new_group;
	}
	async removeGroup(group) {
		try {
			let response = await fetch("./api/" + this.permission_settings_object.api_location + `permissions/group/${group.id}`, {method: "DELETE"});
			if (response.ok) {
				if (this.selected_group.id === group.id) {
					this.permission_settings_object.deselect();
				}
				this.group_list_container.removeChild(group.element);
				await this.permission_settings_object.refreshPermissions(); // to show the group in the add group dialog
				this.permission_settings_object.add_group_group_list.refresh(group_list_factory.groups_json);
			}
			else {
				let response_error_message = response.headers.get("message");
				response_error_message ??= `${response.status}\n${response.statusText}`;
				throw new Error(response_error_message);
			}
		}
		catch(error) {
			console.error(error);
			error_message.innerText = error && error.message ? error.message : "An unknown error occured";
			error_dialog.showModal();
		}
	}
	refresh(groups_json) {
		const fragment = new DocumentFragment();
		let previous_group_not_editable;
		this.groups_json = groups_json;
		for (let rank = 1; rank <= groups_json["group_heirarchy"].length; rank++) {
			let group_id = groups_json["group_heirarchy"][rank-1];
			let group_json = groups_json["groups"][group_id];
			// console.log(group_id);
			// console.log(group_json);
			if (this.permission_settings_object.permissions_json["group_permissions"][group_id] != null) {
				let new_group = this.createGroup(group_json);
				// console.log(new_group);
				fragment.appendChild(new_group.element);
			}
		}
		this.group_list_container.replaceChildren(fragment);
	}
	groupClickEvent(group) {
		this.selected_group = group;
		// this.selected_group_indicator.textContent = this.selected_group.name;
		this.permission_settings_object.group_is_selected = true;
		this.permission_settings_object.user_is_selected = false;
		this.permission_settings_object.showPermissionsForGroup(this.selected_group);
	}
}

class PermissionSettingsUser extends User {
	constructor(user_list, user_json) {
		super(user_list, user_json, true);
		this.remove_button = document.createElement("button");
		this.remove_button.classList.add("RightAligned");
		this.remove_button.textContent = "Remove";
		this.permission_editable = user_list_factory.users_json[this.id]["permission_editable"];
		if (this.permission_editable === false) {
			this.remove_button.disabled = true;
		}
		else {
			this.remove_button.onclick = async() => {
				await this.user_list.removeUser(this);
			}
		}
		this.element.appendChild(this.remove_button);
	}
}

class PermissionSettingsUserList extends UserList {
	constructor(permission_settings_object, user_list_container) {
		super(user_list_container);
		this.permission_settings_object = permission_settings_object;
	}
	createUser(user_json) {
		let new_user = new PermissionSettingsUser(this, user_json);
		return new_user;
	}
	async removeUser(user) {
		try {
			let response = await fetch("./api/" + this.permission_settings_object.api_location + `permissions/user/${user.id}`, {method: "DELETE"});
			if (response.ok) {
				if (this.selected_user.id === user.id) {
					this.permission_settings_object.deselect();
				}
				this.user_list_container.removeChild(user.element);
				await this.permission_settings_object.refreshPermissions(); // to show the user in the add user dialog
				this.permission_settings_object.add_user_user_list.refresh(user_list_factory.users_json);
			}
			else {
				let response_error_message = response.headers.get("message");
				response_error_message ??= `${response.status}\n${response.statusText}`;
				throw new Error(response_error_message);
			}
		}
		catch(error) {
			console.error(error);
			error_message.innerText = error && error.message ? error.message : "An unknown error occured";
			error_dialog.showModal();
		}
	}
	refresh(users_json) {
		const fragment = new DocumentFragment();
		let previous_user_not_editable;
		this.users_json = users_json;
		for (const user_json of Object.values(users_json)) {
			// console.log(user_json);
			if (this.permission_settings_object.permissions_json["user_permissions"][user_json.id] !== undefined) {
				let new_user = this.createUser(user_json);
				// console.log(new_user);
				fragment.appendChild(new_user.element);
			}
		}
		this.user_list_container.replaceChildren(fragment);
	}
	userClickEvent(user) {
		this.selected_user = user;
		// this.selected_user_indicator.textContent = this.selected_user.name;
		this.permission_settings_object.group_is_selected = false;
		this.permission_settings_object.user_is_selected = true;
		this.permission_settings_object.showPermissionsForUser(this.selected_user);
	}
}

class PermissionSettingsAddGroupGroupList extends GroupList {
	constructor(permission_settings_object, group_list_container) {
		super(group_list_container);
		this.permission_settings_object = permission_settings_object;
		this.add_group_dialog = permission_settings_add_group_dialog;
	}
	refresh(groups_json) {
		const fragment = new DocumentFragment();
		let previous_group_not_editable;
		this.groups_json = groups_json;
		for (let rank = 1; rank <= groups_json["group_heirarchy"].length; rank++) {
			let group_id = groups_json["group_heirarchy"][rank-1];
			let group_json = groups_json["groups"][group_id];
			// console.log(group_id);
			// console.log(group_json);
			if (this.permission_settings_object.permissions_json["group_permissions"][group_id] === undefined && group_list_factory.groups_json.groups[group_id]["permission_editable"]) {
				let new_group = this.createGroup(group_json);
				// new_group.client_editable = group_list_factory.groups_json.groups[new_group.id]["permission_editable"];
				// console.log(new_group);
				fragment.appendChild(new_group.element);
			}
		}
		this.group_list_container.replaceChildren(fragment);
	}
	async groupClickEvent(group) {
		let response = await fetch("./api/" + this.permission_settings_object.api_location + `permissions/group/${group.id}`, {method: "POST"});
		if (response.ok) {
			this.group_list_container.removeChild(group.element);
			await this.permission_settings_object.refreshPermissions();
			// console.log("group below");
			// console.log(group);
			// console.log(this.permission_settings_object.permissions_json["group_permissions"]);
			this.permission_settings_object.add_group_group_list.refresh(group_list_factory.groups_json);
			this.permission_settings_object.group_list.refresh(group_list_factory.groups_json);
			// console.log(this.permission_settings_object.permissions_json);
			this.permission_settings_object.group_list.groupClickEvent(group);
		}
		this.add_group_dialog.close();
	}
}

class PermissionSettingsAddUserUserList extends UserList {
	constructor(permission_settings_object, user_list_container) {
		super(user_list_container);
		this.permission_settings_object = permission_settings_object;
		this.add_user_dialog = permission_settings_add_user_dialog;
	}
	createUser(user_json) { // override UserList function to have clickable enabled
		let new_user = new User(this, user_json, true); // the 'true' makes the user clickable
		return new_user;
	}
	refresh(users_json) {
		const fragment = new DocumentFragment();
		// this.users_json = users_json;
		// console.log(users_json);
		for (const user_json of Object.values(users_json)) {
			// console.log(user_json);
			if (this.permission_settings_object.permissions_json["user_permissions"][user_json.id] === undefined && user_list_factory.client_rank < user_list_factory.users_json[user_json.id]["rank"]) {
				let new_user = this.createUser(user_json);
				// console.log(new_user);
				new_user.permission_editable = true;
				fragment.appendChild(new_user.element);
			}
		}
		this.user_list_container.replaceChildren(fragment);
	}
	async userClickEvent(user) {
		let response = await fetch("./api/" + this.permission_settings_object.api_location + `permissions/user/${user.id}`, {method: "POST"});
		if (response.ok) {
			this.user_list_container.removeChild(user.element);
			await this.permission_settings_object.refreshPermissions();
			// console.log("user below");
			// console.log(user);
			// console.log(this.permission_settings_object.permissions_json["user_permissions"]);
			this.permission_settings_object.add_user_user_list.refresh(user_list_factory.users_json);
			this.permission_settings_object.user_list.refresh(user_list_factory.users_json);
			this.permission_settings_object.user_list.userClickEvent(user);
		}
		this.add_user_dialog.close();
	}
}

class PermissionCollection {
	constructor(permission_settings_object, permission_collection_container, enabled_permissions) {
		this.permission_collection_container = permission_collection_container;
		this.permission_settings_object = permission_settings_object;
		this.enabled_permissions = enabled_permissions; // There are different permissions available to be set for boards, threads, and server. Eg. setting the "create thread" permission within a thread would be pointless.
		permission_collection_container.textContent = "Select a group or user to view or modify its permission settings.";
	}
	static permissions = { // Mirrors PERMISSION enum in PermissionSetting.hpp
		0: "Manage permissions",
		1: "View thread",
		2: "Create thread",
		3: "Send message",
		4: "Delete post",
		5: "Upload file"
	};
	static three_state_settings = {
		0: "Deny",
		1: "Inherit",
		2: "Allow"
	};
	showPermissions(permission_collection_json, permission_editable_by_client) {
		const fragment = new DocumentFragment();
		for (const permission_id of this.enabled_permissions) {
			let current_setting; // Set to the threestatesetting of this permission
			const permission_setting_container = document.createElement("li");
			permission_setting_container.id = `user-${this.id}`;
			permission_setting_container.classList.add("ToolBar", "WithBorders");
			const permission_name_element = document.createElement("span");
			permission_name_element.textContent = PermissionCollection.permissions[permission_id];
			permission_setting_container.appendChild(permission_name_element);

			const permission_setting = permission_collection_json[permission_id] ?? 1;
			for (const [three_state_setting_id, three_state_setting_name] of Object.entries(PermissionCollection.three_state_settings)) {
				const input_container = document.createElement("div");
				const input_id = `permission-${permission_id}-setting-${three_state_setting_id}`;
				const radio_element = document.createElement("input");
				radio_element.type = "radio";
				radio_element.id = input_id;
				radio_element.name = `permission-${permission_id}`;
				if (permission_editable_by_client) {
					radio_element.onclick = async() => {
						if (current_setting != three_state_setting_id) {
							current_setting = Number(three_state_setting_id);
							await this.permission_settings_object.setPermission(Number(permission_id), current_setting);
						}
					}
				}
				else {
					radio_element.disabled = true;
				}
				if (permission_setting === Number(three_state_setting_id)) {
					radio_element.checked = true;
					current_setting = Number(three_state_setting_id);
				}
				const label_element = document.createElement("label");
				label_element.setAttribute("for", input_id);
				label_element.textContent = three_state_setting_name;

				input_container.append(radio_element, label_element);
				permission_setting_container.append(input_container);
			}
			fragment.appendChild(permission_setting_container);
		}
		this.permission_collection_container.replaceChildren(fragment);
	}
}

class PermissionSettings {
	constructor(_user_list_element, _group_list_element, _permission_list_element, _selected_permission_user_or_group_indicator, api_location) {
		// this.group_list_element = _group_list_element;
		this.group_list = new PermissionSettingsGroupList(this, _group_list_element);
		group_list_factory.addGroupList(this.group_list);
		this.user_list = new PermissionSettingsUserList(this, _user_list_element);
		user_list_factory.addUserList(this.user_list);
		let enabled_permissions;
		// TODO add board-level permissions
		if (api_location.substring(0, api_location.indexOf("/")) === "server") {
			enabled_permissions = [0, 1, 2, 3, 4, 5];
		}
		else { // This is a thread
			enabled_permissions = [1, 3, 4, 5];
		}
		this.permission_collection = new PermissionCollection(this, _permission_list_element, enabled_permissions); // PermissionCollection
		this.permission_list_element = _permission_list_element;
		this.group_is_selected = null;
		this.user_is_selected = null;
		this.add_group_button = permission_settings_add_group_button;
		this.add_user_button = permission_settings_add_user_button;
		this.selected_permission_user_or_group_indicator = _selected_permission_user_or_group_indicator;
		this.selected_permission_user_or_group_indicator.textContent = " (No group or user selected)";
		this.api_location = api_location;
		this.add_group_group_list = new PermissionSettingsAddGroupGroupList(this, permission_settings_add_group_dialog_group_list);
		group_list_factory.addGroupList(this.add_group_group_list);
		this.add_user_user_list = new PermissionSettingsAddUserUserList(this, permission_settings_add_user_dialog_user_list);
		user_list_factory.addUserList(this.add_user_user_list);
		this.add_group_button.onclick = () => {
			this.add_group_group_list.add_group_dialog.showModal();
		};
		this.add_user_button.onclick = () => {
			this.add_user_user_list.add_user_dialog.showModal();
		};
		this.permissions_json = {
			group_permissions: {},
			user_permissions: {}
		};
	}
	createGroup(group_json) {
		let new_group = new PermissionSettingsGroup(this, group_json);
		return new_group;
	}
	showPermissionsForGroup(group) {
		this.permission_collection.showPermissions(this.permissions_json["group_permissions"][group.id]["permission_collection"] || {}, group.permission_editable);
		this.selected_permission_user_or_group_indicator.textContent = ` for group ${group.name}`;
	}
	showPermissionsForUser(user) {
		this.permission_collection.showPermissions(this.permissions_json["user_permissions"][user.id]["permission_collection"] || {}, user.permission_editable);
		this.selected_permission_user_or_group_indicator.textContent = ` for user ${user.username}`;
	}
	deselect() {
		while (this.permission_list_element.children.length > 0) {
			this.permission_list_element.removeChild(this.permission_list_element.children[0]);
		}
		this.selected_permission_user_or_group_indicator.textContent = " (No group or user selected)";
		this.group_is_selected = false;
		this.user_is_selected = false;
	}
	async setPermission(permission_id, three_state_setting) {
		let response;
		let request_json = {
			"permission": permission_id,
			"setting": three_state_setting
		};
		if (this.group_is_selected === true) {
			this.permissions_json["group_permissions"][this.group_list.selected_group.id]["permission_collection"][request_json.permission] = request_json.setting;
			response = await fetch("./api/" + this.api_location + `permissions/group/${this.group_list.selected_group.id}`, {method: "PUT", body: JSON.stringify(request_json)});
		}
		else if (this.user_is_selected === true) {
			this.permissions_json["user_permissions"][this.user_list.selected_user.id]["permission_collection"][request_json.permission] = request_json.setting;
			response = await fetch("./api/" + this.api_location + `permissions/user/${this.user_list.selected_user.id}`, {method: "PUT", body: JSON.stringify(request_json)});
		}
		else {
			error_message.textContent = "No group or user is selected";
			error_dialog.showModal();
		}
	}
	async refreshPermissions() {
		let response = await fetch("./api/" + this.api_location + "permissions");
		if (response.ok) {
			let response_json;
			try {
				response_json = await response.json()
			}
			catch (error) {
				error_message.innerText = "Could not parse permissions JSON;\n";
				error_dialog.showModal();
				return null;
			}
			// console.log(response);
			// console.log(response_json);
			this.permissions_json["user_permissions"] = response_json["user_permissions"];
			this.permissions_json["group_permissions"] = response_json["group_permissions"];
			// this.client_rank = Number(response.headers.get("Client-Rank"));
			/*
			const fragment = new DocumentFragment();
			for (let [group_id, group_permission_json] of Object.entries(this.permissions_json["group_permissions"])) {
				// Add group to group list
				// let new_group = this.group_list.createGroup(group_list_factory.groups_json.groups[group_id]);
				let permission_entry_container = document.createElement("li");
				permission_entry_container.innerText = group_id + group_permission_json;
				fragment.appendChild(permission_entry_container);
			}
			this.permission_list_element.replaceChildren(fragment);
			*/
		}
		else {
			console.log(response);
			console.error("Response is not ok");
		}
	}
}

class addUserToGroupsGroupList {
	constructor(dialog, element, ok_button_element) {
		this.dialog = dialog;
		this.element = element;
		this.ok_button = ok_button_element;
		this.ok_button.onclick = async() => {
			this.addUserToGroupAction();
			this.dialog.close();
		}
	}
	async addUserToGroupAction() {
		let groups_to_add = [];
		this.element.querySelectorAll("input")
			.forEach(input_element => {
				if (input_element.checked) {
					groups_to_add.push(Number(input_element.id.substring("add_user_to_group_dialog_group_".length)));
				}
			});
		// console.log(groups_to_add);
		if (groups_to_add.length > 0) {
			let response = await fetch(`./api/user/${the_user_list.selected_user.id}/add_groups`, {method: "POST", body: JSON.stringify({"groups_by_id":groups_to_add})});
			if (response.ok === true) {
				await user_list_factory.refreshUsers(); // TODO only refresh the user the groups were added to
				this.refresh(group_list_factory.groups_json);
			}
		}
	}
	refresh(groups_json) {
		const fragment = new DocumentFragment();
		for (let rank = 0; rank < groups_json["group_heirarchy"].length; rank++) {
			let group_id = groups_json["group_heirarchy"][rank];
			let group_json = groups_json["groups"][group_id];
			// console.log(group_json);
			if (group_json.id !== PRESET_GROUPS.USERS &&
					group_json.id !== PRESET_GROUPS.PUBLIC &&
					rank > user_list_factory.client_rank &&
					user_list_factory.users_json) {
				let list_item = document.createElement("div");
				let group_id_tag = `add_user_to_group_dialog_group_${group_id}`;
				let group_checkbox_element = document.createElement("input");
				group_checkbox_element.id = group_id_tag;
				group_checkbox_element.type = "checkbox";
				group_checkbox_element.name = "add_user_to_group_dialog_group_list_item";
				let group_label_element = document.createElement("label");
				group_label_element.setAttribute("for", group_id_tag);
				group_label_element.innerText = group_json["name"];
				list_item.append(group_checkbox_element, group_label_element);
				fragment.appendChild(list_item);
			}
		}
		this.element.replaceChildren(fragment);
	}
}
