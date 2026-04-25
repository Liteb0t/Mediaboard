#include "permission_managed_object.hpp"
#include "DatabaseConnection.hpp"
#include "db_interface.h"
#include <iostream>

PermissionObjectBase::PermissionObjectBase(int permission_object_id, DatabaseConnection* db) // On extraction from database
		: permission_object_id(permission_object_id),
		db(db) {
	this->cacheAllPermissions();
}

PermissionObjectBase::PermissionObjectBase(DatabaseConnection* db) // On new object creation
		: permission_object_id(db->getUniquePermissionObjectId()),
		db(db) {
}

void PermissionObjectBase::cacheAllPermissions() {
	std::cout << "[PermissionObjectBase] retrieving permissions for " << this->permission_object_id << ": ";
	db->declarePermissionCollectionCursor(this->permission_object_id);
	while (true) {
		db_permission_collection_struct* permission_collection = db->getValueFromPermissionCollectionCursor();
		if (!permission_collection->has_value)
			break;
		std::cout << permission_collection->id << ", ";
		PermissionCollection new_permission_collection(permission_collection->id, permission_collection->account_id, permission_collection->group_id);

		// Add settings, if any
		db->declarePermissionSettingCursor(permission_collection->id);
		while (true) {
			db_permission_setting_struct* permission_setting = db->getValueFromPermissionSettingCursor();
			if (permission_setting->has_value)
				new_permission_collection.addPermissionSetting(permission_setting);
			else
				break;
		}
		db->closePermissionSettingCursor();

		if (new_permission_collection.getUserOrGroupEnumValue() == USER_OR_GROUP::USER)
			this->user_permissions.emplace(permission_collection->account_id, new_permission_collection);
		else
			this->group_permissions.emplace(permission_collection->group_id, new_permission_collection);
	}
	db->closePermissionCollectionCursor();
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
	std::cout << "[PermissionManager] getting user_permissions_json..." << std::endl;
	// for (const std::pair<int, User> user : *this->getUsers()) {
	boost::shared_ptr<std::unordered_map<int, User>> _users = this->getUsers();
	for (std::unordered_map<int, User>::const_iterator user_it = _users->begin(); user_it != _users->end(); user_it++) {
		int user_id = user_it->first;
		std::cout << user_id << ", ";
		std::unordered_map<int, PermissionCollection>::const_iterator user_permission_collection_it = this->user_permissions.find(user_id);
		if (user_permission_collection_it != this->user_permissions.end()) {
			const std::unordered_map<PERMISSION, PermissionSetting>* permission_settings = user_permission_collection_it->second.getPermissionMap();
			nlohmann::json permission_collection_json = nlohmann::json::object();
			for (std::unordered_map<PERMISSION, PermissionSetting>::const_iterator permission_it = permission_settings->begin(); permission_it != permission_settings->end(); permission_it++) {
				permission_collection_json[std::to_string(static_cast<int>(permission_it->first))] = static_cast<int>(permission_it->second.get());
			}
			user_permissions_json[std::to_string(user_id)]["permission_collection"] = permission_collection_json;
		}
	}
	std::cout << "done." << std::endl;

	permission_collections_json["user_permissions"] = user_permissions_json;

	return permission_collections_json;
}

PermissionManager::PermissionManager(int permission_object_id, DatabaseConnection* db)
		: PermissionObjectBase(0, db),
		owner_id(db->getOwnerIdIfExists()) {
	this->cacheAllGroups();
	// this->cacheAllUsers();
	if (this->owner_id)
		this->grantOwnerPrivileges();
}

// Grants all permissions to the Owner group
// This will no longer be needed when the database can populate the entries on first start
void PermissionManager::grantOwnerPrivileges() {
	std::cout << "[PermissionManager] grantOwnerPrivileges()" << std::endl;
	for (int permission_number = 0; permission_number < static_cast<int>(PERMISSION::NUMBER_OF_PERMISSIONS); permission_number++) {
		if (!this->passPermissionForGroup(false, static_cast<PERMISSION>(permission_number), static_cast<int>(BUILTIN_GROUPS::OWNER)))
			this->setGroupPermission(0, static_cast<PERMISSION>(permission_number), THREE_STATE_SETTING::ALLOW);
	}
}

void PermissionManager::cacheAllUsers() {
	std::cout << "[PermissionManager] Retreiving accounts from database... ";
	db->declareAccountCursor();
	while (true) {
		struct db_account_struct* account_struct = db->getValueFromAccountCursor();
		if (!account_struct->has_value)
			break;
		std::cout << account_struct->id << ", ";
		User user(account_struct);
		this->users.emplace(account_struct->id, user);
		this->username_to_id_map.emplace(user.getUsername(), account_struct->id);
	}
	db->closeAccountCursor();
	std::cout << "done." << std::endl;
}

void PermissionManager::cacheAllGroups() {
	std::cout << "[PermissionManager] Retreiving groups from database... ";
	// struct db_group_array* group_array = db_retrieve_groups();
	db->declareGroupCursor();
	while (true) {
		db_group_struct* group_struct = db->getValueFromGroupCursor();
		if (!group_struct->has_value)
			break;
		Group group(group_struct);
		this->groups.emplace(group_struct->id, group);
		std::cout << group_struct->id << ", ";
	}
	db->closeGroupCursor();
	std::cout << "added " << this->groups.size() << " groups";
	db->declareGroupHeirarchyCursor();
	while (true) {
		db_group_heirarchy_struct* group_heirarchy_struct = db->getValueFromGroupHeirarchyCursor();
		if (!group_heirarchy_struct->has_value)
			break;
		this->ordered_groups.push_back(group_heirarchy_struct->group_id);
	}
	db->closeGroupHeirarchyCursor();
	std::cout << ", established heirarchy";
	db->declareGroupMemberCursor();
	while (true) {
		db_group_member_struct* group_member_struct = db->getValueFromGroupMemberCursor();
		if (!group_member_struct->has_value)
			break;
		this->groups.at(group_member_struct->group_id).addMember(group_member_struct->account_id);
	}
	db->closeGroupMemberCursor();
	std::cout << ", added users to groups." << std::endl;

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
	std::cout << "[PermissionManager] Saving new group heirarchy: ";
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
