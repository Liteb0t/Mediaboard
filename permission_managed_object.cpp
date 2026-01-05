#include "permission_managed_object.hpp"
#include "db_interface.h"
#include <iostream>

PermissionObjectBase::PermissionObjectBase(int permission_object_id)
		: permission_object_id(permission_object_id) {
	std::cout << "PermissionObjectBase constructor called" << std::endl;
	this->cacheAllPermissions();
}

void PermissionObjectBase::cacheAllPermissions(/*int permission_object_id*/) {
	// this->permission_object_id = permission_object_id;
	std::cout << "[PermissionObjectBase] retrieving permissions for " << this->permission_object_id << ": ";
	// Permission Collections
	db_permission_collection_array* permission_collection_array = db_retrieve_permission_collections_for_permission_object(this->permission_object_id);
	for (int i = 0; i < permission_collection_array->used; i++) {
		/*
		int permission_collection_id = permission_collection_array->array[i].id;
		std::cout << permission_collection_id << ", ";
		USER_OR_GROUP user_or_group; int user_or_group_id;
		if (permission_collection_array->array[i].group_id > -1) {
			user_or_group = USER_OR_GROUP::GROUP;
			user_or_group_id = permission_collection_array->array[i].group_id;
		}
		else {
			user_or_group = USER_OR_GROUP::USER;
			user_or_group_id = permission_collection_array->array[i].account_id;
		}
		*/
		PermissionCollection new_permission_collection(this->permission_object_id, &permission_collection_array->array[i]);
		// Get permission settings
		db_permission_setting_array* permission_settings = db_retrieve_permission_settings_for_permission_collection(permission_collection_array->array[i].id);
		for (int j = 0; j < permission_settings->used; j++) {
			new_permission_collection.addPermissionSetting(&permission_settings->array[j]);
			// int permission_number = permission_settings->array[j].permission_number;
			// new_permission_collection.setPermission(static_cast<PERMISSION>(permission_number), static_cast<THREE_STATE_SETTING>(permission_settings->array[j].setting));
		}
		freePermissionSettingArray(permission_settings);

		std::cout << "permission collection ID: " << permission_collection_array->array[i].id << std::endl;
		std::cout << permission_collection_array->array[i].account_id << std::endl;
		if (permission_collection_array->array[i].account_id != -1)
			this->user_permissions.emplace(permission_collection_array->array[i].account_id, new_permission_collection);
		else
			this->group_permissions.emplace(permission_collection_array->array[i].group_id, new_permission_collection);
	}
	freePermissionCollectionArray(permission_collection_array);
	std::cout << "done." << std::endl;
}

nlohmann::json PermissionObjectBase::getPermissionCollectionsAsJson(int client_id) const {
	nlohmann::json permission_collections_json;
	// client_rank not used because client_editable status is given by dumpAllGroups()/dumpAllUsers()
	// int client_rank = this->getUserRank(client_id);

	nlohmann::json group_permissions_json = nlohmann::json::object();
	for (int i = 0; i < this->getOrderedGroups()->size(); i++) {
		// for (int group_id : *(this->getOrderedGroups())) {
		int group_id = (*(this->getOrderedGroups()))[i];
		std::unordered_map<int, PermissionCollection>::const_iterator group_permission_collection_it = this->group_permissions.find(group_id);
		if (group_permission_collection_it != this->group_permissions.end()) {
			const std::unordered_map<PERMISSION, PermissionSetting>* permission_settings = group_permission_collection_it->second.getPermissionMap();
			nlohmann::json permission_collection_json = nlohmann::json::object();
			for (std::unordered_map<PERMISSION, PermissionSetting>::const_iterator permission_it = permission_settings->begin(); permission_it != permission_settings->end(); permission_it++) {
				permission_collection_json[std::to_string(static_cast<int>(permission_it->first))] = static_cast<int>(permission_it->second.get());
			}
			// bool group_permission_is_client_editable;
			// if (this->userHasPermission(client_id, PERMISSION::MANAGE_PERMISSIONS) && client_rank < i)
			// 	group_permission_is_client_editable	= true;
			// else
			// 	group_permission_is_client_editable = false;
			// group_permissions_json[std::to_string(group_id)]["client_editable"] = group_permission_is_client_editable;
			group_permissions_json[std::to_string(group_id)]["permission_collection"] = permission_collection_json;
		}
	}
	permission_collections_json["group_permissions"] = group_permissions_json;

	nlohmann::json user_permissions_json = nlohmann::json::object();
	// int user_rank = this->getUserRank(client_id);
	// boost::shared_ptr<std::unordered_map<int, User>> users = this->getUsers();
	for (const std::pair<int, User> user : *this->getUsers()) {
		std::unordered_map<int, PermissionCollection>::const_iterator user_permission_collection_it = this->user_permissions.find(user.first);
		if (user_permission_collection_it != this->user_permissions.end()) {
			const std::unordered_map<PERMISSION, PermissionSetting>* permission_settings = user_permission_collection_it->second.getPermissionMap();
			nlohmann::json permission_collection_json = nlohmann::json::object();
			for (std::unordered_map<PERMISSION, PermissionSetting>::const_iterator permission_it = permission_settings->begin(); permission_it != permission_settings->end(); permission_it++) {
				permission_collection_json[std::to_string(static_cast<int>(permission_it->first))] = static_cast<int>(permission_it->second.get());
			}
			user_permissions_json[std::to_string(user.first)]["permission_collection"] = permission_collection_json;
		}
	}

	permission_collections_json["user_permissions"] = user_permissions_json;

	return permission_collections_json;
}

void PermissionManager::cacheAllUsers() {
	std::cout << "Retreiving accounts from database..." << std::endl;
	struct db_account_array* account_array = db_retrieve_accounts();
	for (int i = 0; i < account_array->used; i++) {
		std::cout << account_array->array[i].id << ", ";
		User user(&account_array->array[i]);
		this->users.emplace(account_array->array[i].id, user);
		this->username_to_id_map.emplace(user.getUsername(), account_array->array[i].id);
	}
	std::cout << "done." << std::endl;
	freeAccountArray(account_array);
}

void PermissionManager::cacheAllGroups() {
	std::cout << "Retreiving groups from database..." << std::endl;
	struct db_group_array* group_array = db_retrieve_groups();
	for (int i = 0; i < group_array->used; i++) {
		Group group(&group_array->array[i]);
		this->groups.emplace(group_array->array[i].id, group);
		std::cout << group_array->array[i].id << ", ";
	}
	std::cout << "added " << this->groups.size() << " groups." << std::endl;
	freeGroupArray(group_array);
	struct db_group_heirarchy_array* group_heirarchy = db_retrieve_group_heirarchy();
	this->ordered_groups.reserve(group_heirarchy->used + 4);
	for (int i = 0; i < group_heirarchy->used; i++) {
		this->ordered_groups.push_back(group_heirarchy->array[i].group_id);
	}
	freeGroupHeirarchyArray(group_heirarchy);
	struct db_group_member_array* group_member_array = db_retrieve_group_members();
	for (int i = 0; i < group_member_array->used; i++) {
		this->toGroupAddMember(group_member_array->array[i].group_id, group_member_array->array[i].account_id);
	}
	freeGroupMemberArray(group_member_array);

	// Check that the permission_group table is consistent with the permission_group_heirarchy table
	std::cout << "[PermissionManager] Checking consistency between groups and heirarchy..." << std::endl;
	bool consistency_test_passed = true;
	if (this->ordered_groups.size() != this->groups.size()) {
		std::cerr << std::string("Number of groups does not match") << std::endl;
		consistency_test_passed = false;
	}
	std::unordered_set<int> new_group_order_set;
	for (int group_id : this->ordered_groups) {
		// Check for duplicates
		std::unordered_set<int>::const_iterator duplicate_check_it = new_group_order_set.find(group_id); 
		if (duplicate_check_it != new_group_order_set.end()) {
			std::cerr << "Duplicate group " + std::to_string(group_id) + " detected" << std::endl;
			consistency_test_passed = false;
		}
		else
			new_group_order_set.insert(group_id);

		// Check if all groups exist
		if (!this->groupExists(group_id)) {
			std::cerr << "Group " + std::to_string(group_id) + " does not exist." << std::endl; // Group does not exist
			consistency_test_passed = false;
		}
	}
	if (consistency_test_passed)
		std::cout << "[PermissionManager] No issues were found." << std::endl;
	else
		std::cout << "[PermissionManager] Test failed." << std::endl;
}

int PermissionManager::addGroup(std::string group_name, int group_rank) {
	Group new_group(group_name);
	int new_group_id = new_group.getId();
	this->groups.emplace(new_group_id, new_group);
	// https://stackoverflow.com/a/6935419
	std::vector<int>::const_iterator group_to_insert_above = this->ordered_groups.begin() + group_rank;
	this->ordered_groups.insert(group_to_insert_above, new_group_id);
	this->saveGroupHeirarchy();
	return new_group_id;
}

void PermissionManager::saveGroupHeirarchy() const {
	std::cout << "[PermissionManager] running setOrderedGroups" << std::endl;
	std::cout << "Logging from ordered_groups: ";
	for (int i = 0; i < this->ordered_groups.size(); i++)
		std::cout << i << ": " << this->ordered_groups[i] << ", ";
	std::cout << "done." << std::endl;

	struct db_group_heirarchy_array group_heirarchy;
	initGroupHeirarchyArray(&group_heirarchy, this->ordered_groups.size());
	for (int i = 0; i < this->ordered_groups.size(); i++) {
		struct db_group_heirarchy_struct group;
		group.rank = i;
		group.group_id = this->ordered_groups[i];
		insertToGroupHeirarchyArray(&group_heirarchy, group);
	}
	db_update_group_heirarchy(&group_heirarchy);
	freeGroupHeirarchyArray(&group_heirarchy);
}
