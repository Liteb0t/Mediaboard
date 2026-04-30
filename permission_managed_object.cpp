#include "permission_managed_object.hpp"
#include <iostream>

PermissionObjectBase::PermissionObjectBase(int permission_object_id, FuzeDBI::Connection* fuze_dbi) // On extraction from database
		: id(permission_object_id),
		fuze_dbi(fuze_dbi) {
	this->cacheAllPermissions();
}

PermissionObjectBase::PermissionObjectBase(FuzeDBI::Connection* fuze_dbi) // On new object creation
		: fuze_dbi(fuze_dbi),
		id(fuze_dbi->query<int>("SELECT permission_object_id FROM _sequences")) {
	fuze_dbi->query<void>("UPDATE _sequences SET permission_object_id = $1", this->id+1);
}

void PermissionObjectBase::cacheAllPermissions() {
	std::cout << "[PermissionObjectBase] retrieving permissions for " << this->id << ": ";
	for (auto permission_collection_tuple : fuze_dbi->queryRows<std::tuple<int, std::optional<int>, std::optional<int>>>("SELECT id, account_id, permission_group_id FROM permission_collection WHERE permission_object_id = $1", this->id)) {
		std::optional<int> account_id = std::get<1>(permission_collection_tuple);
		std::optional<int> group_id = std::get<2>(permission_collection_tuple);
		if (!account_id)
			std::cout << "account ID: NULL" << std::endl;
		if (!group_id)
			std::cout << "group ID: NULL" << std::endl;
		PermissionCollection new_permission_collection(std::get<0>(permission_collection_tuple), account_id, group_id);

		for (auto permission_setting_tuple : fuze_dbi->queryRows<std::tuple<int, int, int>>("SELECT id, permission_number, setting FROM permission_setting WHERE permission_collection_id = $1", new_permission_collection.getId())) {
			std::cout << std::get<0>(permission_setting_tuple) << ", ";
			new_permission_collection.addPermissionSetting(std::get<0>(permission_setting_tuple), static_cast<PERMISSION>(std::get<1>(permission_setting_tuple)), static_cast<THREE_STATE_SETTING>(std::get<2>(permission_setting_tuple)));
		}
		if (new_permission_collection.getAccountOrGroupEnumValue() == ACCOUNT_OR_GROUP::ACCOUNT)
			this->account_permissions.emplace(account_id.value(), new_permission_collection);
		else
			this->group_permissions.emplace(group_id.value(), new_permission_collection);
	}
	std::cout << "done." << std::endl;
}

/*
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
*/
PermissionManager::PermissionManager(int permission_object_id, FuzeDBI::Connection* fuze_dbi)
		: PermissionObjectBase(0, fuze_dbi),
		owner_id(/*db->getOwnerIdIfExists()*/ 0) {
	this->cacheAllGroups();
	this->cacheAllAccounts();
	if (this->owner_id)
		this->grantOwnerPrivileges();
}

// Grants all permissions to the Owner group
// This will no longer be needed when the database can populate the entries on first start
void PermissionManager::grantOwnerPrivileges() {
	std::cout << "[PermissionManager] grantOwnerPrivileges()" << std::endl;
	if (!this->groups.contains(static_cast<int>(BUILTIN_GROUPS::OWNER)))
		throw std::runtime_error("Group OWNER does not exist");
	for (int permission_number = 0; permission_number < static_cast<int>(PERMISSION::NUMBER_OF_PERMISSIONS); permission_number++) {
		if (!this->passPermissionForGroup(false, static_cast<PERMISSION>(permission_number), static_cast<int>(BUILTIN_GROUPS::OWNER)))
			this->setGroupPermission(static_cast<int>(BUILTIN_GROUPS::OWNER), static_cast<PERMISSION>(permission_number), THREE_STATE_SETTING::ALLOW);
	}
}


void PermissionManager::cacheAllAccounts() {
	std::cout << "[PermissionManager] Retreiving accounts from database... ";
	for (auto user_t : fuze_dbi->queryRows<std::tuple<int, std::string>>("SELECT id, username FROM account")) {
		std::cout << std::get<0>(user_t) << ", ";
		this->accounts.emplace(std::get<0>(user_t), Account{
			.id = std::get<0>(user_t),
			.username = std::get<1>(user_t)
		});
	}
	std::cout << "done." << std::endl;
}

void PermissionManager::cacheAllGroups() {
	std::cout << "[PermissionManager] Retreiving groups from database... ";
	// struct db_group_array* group_array = db_retrieve_groups();
	for (auto group_tuple : fuze_dbi->queryRows<std::tuple<int, std::string>>("SELECT id, name FROM permission_group")) {
		Group group(std::get<0>(group_tuple), std::get<1>(group_tuple));
		this->groups.emplace(std::get<0>(group_tuple), group);
	}
	std::cout << "added " << this->groups.size() << " groups";

	for (auto group_id : fuze_dbi->queryRows<int>("SELECT permission_group_id FROM permission_group_heirarchy ORDER BY rank")) {
		this->ordered_groups.push_back(group_id);
	}
	std::cout << ", established heirarchy";

	for (auto group_member : fuze_dbi->queryRows<std::tuple<int, int>>("SELECT permission_group_id, account_id FROM permission_group_account")) {
		this->groups.at(std::get<0>(group_member)).addMember(std::get<1>(group_member));
	}
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
			std::cerr << std::format("Duplicate group {} detected.", group_id) << std::endl;
			consistency_test_passed = false;
		}
		else
			new_group_order_set.insert(group_id);

		// Check if all groups exist
		if (!this->groupExists(group_id)) {
			std::cerr << std::format("Group {} does not exist.", group_id) << std::endl;
			consistency_test_passed = false;
		}
	}
	if (consistency_test_passed)
		std::cout << "[PermissionManager] No issues were found." << std::endl;
	else
		throw std::runtime_error("[PermissionManager] Test failed.");
}

int PermissionManager::createAccount(const std::string& username, const char* password_hash_hash, const char* intermediate_salt_base64) {
	int new_account_id = fuze_dbi->query<int>("SELECT account_id FROM _sequences");
	fuze_dbi->query<void>("UPDATE _sequences SET account_id = $1", new_account_id+1);
	fuze_dbi->query<void>("INSERT INTO account(id, username, password_hash_hash_base64, intermediate_salt_base64) VALUES ($1, $2, $3, $4)", new_account_id, username.c_str(), password_hash_hash, intermediate_salt_base64);
	this->accounts.emplace(new_account_id, Account{
		.id = new_account_id,
		.username = username
	});
	return new_account_id;
}


int PermissionManager::addGroup(std::string group_name, int group_rank) {
	int new_group_id = fuze_dbi->query<int>("SELECT permission_group_id FROM _sequences");
	fuze_dbi->query<void>("UPDATE _sequences SET account_id = $1", new_group_id+1);
	fuze_dbi->query<void>("INSERT INTO permission_group(id, name) VALUES ($1, $2)", new_group_id, group_name.c_str());
	Group new_group(new_group_id, group_name);
	this->groups.emplace(new_group_id, new_group);
	// https://stackoverflow.com/a/6935419
	std::vector<int>::const_iterator group_to_insert_above = this->ordered_groups.begin() + group_rank;
	this->ordered_groups.insert(group_to_insert_above, new_group_id);
	this->saveGroupHeirarchy();
	return new_group_id;
}

void PermissionManager::saveGroupHeirarchy() const {
	std::cout << "[PermissionManager] Saving new group heirarchy: ";
	for (int rank = 0; rank < this->ordered_groups.size(); rank++) {
		fuze_dbi->query<void>("INSERT INTO permission_group_heirarchy(rank, permission_group) VALUES ($1, $2)", rank, this->ordered_groups[rank]);
		std::cout << rank << ": " << this->ordered_groups[rank] << ", ";
	}
	std::cout << "done." << std::endl;
}

/*
bool PermissionManagedObject::isOwnedBy(const Client& client) const {
	if (!this->owner_account_id)
		return false;
	else if (client.account_id && client.account_id.value() == this->owner_account_id)
		return true;
	else if (client.session_id == this->owner.value().session_id)
		return true;
	else
		return false;
}
*/
