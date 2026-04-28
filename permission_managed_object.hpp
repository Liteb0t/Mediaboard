#ifndef PERMISSION_MANAGED_OBJECT
#define PERMISSION_MANAGED_OBJECT

#include "FuzeDBI.hpp"
#include "group.hpp"
#include "permission_collection.hpp"
#include <algorithm>
#include <iostream>
#include <nlohmann/json.hpp>
#include <vector>

enum class BUILTIN_GROUPS {
	OWNER = 0,
	USERS = 1,
	PUBLIC = 2
};

struct Client {
	const int id;
	std::optional<int> account_id;
	// const std::string session_id;
};

struct Account {
	static const int PUBLIC = 0;
	const int id;
	std::string username;
};

class PermissionObjectBase {
public:
	PermissionObjectBase(int permission_object_id, FuzeDBI::Connection* fuze_dbi); // retrieve from database
	PermissionObjectBase(FuzeDBI::Connection* fuze_dbi); // save new object to database
	void cacheAllPermissions();
	void addGroupPermissionCollection(int group_id) {
		std::cout << "[PermissionObjectBase] adding group permission_collection for group " << group_id << std::endl;
		// PermissionCollection permission_collection(this->permission_object_id, USER_OR_GROUP::GROUP, group_id, db);
		// this->group_permissions.emplace(group_id, permission_collection);
	}
	void addAccountPermissionCollection(int account_id) {
		std::cout << "[PermissionObjectBase] adding account permission_collection for account " << account_id << std::endl;
		// PermissionCollection permission_collection(this->permission_object_id, USER_OR_GROUP::USER, account_id, db);
		// this->account_permissions.emplace(account_id, permission_collection);
	}
	void removeGroupPermissionCollection(int group_id) {
		// this->group_permissions.at(group_id).removeFromDatabase();
		this->group_permissions.erase(group_id);
	}
	void removeAccountPermissionCollection(int account_id) {
		// this->account_permissions.at(account_id).removeFromDatabase();
		this->account_permissions.erase(account_id);
	}
	void setGroupPermission(int group_id, PERMISSION permission_type, THREE_STATE_SETTING setting) {
		std::unordered_map<int, PermissionCollection>::const_iterator group_iterator = this->group_permissions.find(group_id);
		if (group_iterator == this->group_permissions.end())
			this->addGroupPermissionCollection(group_id);
		this->group_permissions.at(group_id).setPermission(permission_type, setting);
	}
	void setAccountPermission(int user_id, PERMISSION permission_type, THREE_STATE_SETTING setting) {
		if (auto it = this->account_permissions.find(user_id); it == this->account_permissions.end())
			this->addAccountPermissionCollection(user_id);
		this->account_permissions.at(user_id).setPermission(permission_type, setting);
	}
	bool passPermissionForGroup(bool inherited_permission, PERMISSION permission, int group_id) const {
		inherited_permission = this->passInheritedPermissionForGroup(inherited_permission, permission, group_id);
		// Check if a group permission is set for this object
		std::unordered_map<int, PermissionCollection>::const_iterator group_iterator = this->group_permissions.find(group_id);
		if (group_iterator != this->group_permissions.end())
			inherited_permission = group_iterator->second.passPermission(permission, inherited_permission);
		return inherited_permission;
	}
	bool passPermissionForAccount(bool inherited_permission, PERMISSION permission, int account_id) const {
		inherited_permission = this->passInheritedPermissionForAccount(inherited_permission, permission, account_id);
		if (auto it = this->account_permissions.find(account_id); it != this->account_permissions.end()) // Check if a client permission is set for this object
			inherited_permission = it->second.passPermission(permission, inherited_permission);
		return inherited_permission;
	}
	virtual const std::vector<int>* getOrderedGroups() const = 0;
	virtual std::vector<int> getOrderedGroupsContainingMember(int user_id) const = 0;
	virtual bool passInheritedPermissionForGroup(bool inherited_permission, PERMISSION permission, int group_id) const = 0;
	virtual bool passInheritedPermissionForAccount(bool inherited_permission, PERMISSION permission, int account_id) const = 0;
	virtual int getClientRank(const Client& client) const = 0;
	virtual int getGroupRank(int group_id) const = 0;
	// virtual const boost::shared_ptr<std::unordered_map<int, Account>> getAccounts() const = 0;
	bool clientHasPermission(const std::optional<Client>& client, PERMISSION permission) const {
		bool inherited_permission = false;
		// PUBLIC and USERS are built-in, that is, they are never placed in an account's group list. This is because every account is implicitly a part of these two groups
		inherited_permission = this->passPermissionForGroup(inherited_permission, permission, static_cast<int>(BUILTIN_GROUPS::PUBLIC));
		if (client && client.value().account_id) {
			inherited_permission = this->passPermissionForGroup(inherited_permission, permission, static_cast<int>(BUILTIN_GROUPS::USERS));
			std::vector<int> user_ordered_groups = this->getOrderedGroupsContainingMember(client.value().id);
			for (std::vector<int>::const_reverse_iterator it = user_ordered_groups.rbegin(); it != user_ordered_groups.rend(); it++) {
				inherited_permission = this->passPermissionForGroup(inherited_permission, permission, *it);
			}
			inherited_permission = this->passPermissionForAccount(inherited_permission, permission, client.value().account_id.value());
		}
		return inherited_permission;
	}
	bool clientHasPermissionForGroup(const Client& client, PERMISSION permission, int group_id) const {
		if (!this->clientHasPermission(client, permission))
			return false;
		return this->getClientRank(client) < this->getGroupRank(group_id);
	}
	bool clientHasPermissionForClient(const Client& client, PERMISSION permission, const Client& _client) const {
		if (!this->clientHasPermission(client, permission))
			return false;
		return this->getClientRank(client) < this->getClientRank(_client);
	}
	bool permissionCollectionExistsForGroup(int group_id) const { return this->group_permissions.contains(group_id); }
	bool permissionCollectionExistsForAccount(int account_id) const { return this->account_permissions.contains(account_id); }
protected:
	nlohmann::json getPermissionCollectionsAsJson(int client_id) const;
	int permission_object_id; // Used to identify this object in the database
	FuzeDBI::Connection* fuze_dbi;
private:
	std::unordered_map<int, PermissionCollection> group_permissions;
	std::unordered_map<int, PermissionCollection> account_permissions;
};

class PermissionManager : public PermissionObjectBase {
public:
	PermissionManager(int permission_object_id, FuzeDBI::Connection* fuze_dbi);
	const std::vector<int>* getOrderedGroups() const override {
		return &(this->ordered_groups);
	}
	std::vector<int> getOrderedGroupsContainingMember(int user_id) const override {
		std::vector<int> ordered_groups_containing_member;
		ordered_groups_containing_member.reserve(this->ordered_groups.size());
		for (int group_id : this->ordered_groups) {
			if (this->groups.at(group_id).containsMember(user_id))
				ordered_groups_containing_member.push_back(group_id);
		}
		return ordered_groups_containing_member;
	}
	bool groupExists(int group_id) const {
		std::unordered_map<int, Group>::const_iterator it = this->groups.find(group_id); 
		return it != this->groups.end();
	}
	int getClientRank(const Client& client) const override {
		if (!client.account_id)
			return this->ordered_groups.size(); // This is the least privileged rank
		int account_id = client.account_id.value();
		if (this->owner_id && account_id == this->owner_id.value())
			return 0; // This is the most privileged rank
		int i;
		for (i = 0; i < this->ordered_groups.size() - 2; i++) { // 2 is subtracted because USERS and PUBLIC are hard-coded groups
			if (this->groups.at(ordered_groups[i]).containsMember(account_id))
				break;
		}
		return i + 1; // 1 is added because the ADMINISTRATORS group is one rank below OWNER
	}
	std::optional<int> getIdFromUsername(const std::string& username) const {
		if (auto it = this->username_to_id_map.find(username); it != this->username_to_id_map.end())
			return it->second;
		else
			return {};
	}
	std::string getUsernameFromAccount(int account_id) const {
		return this->accounts.at(account_id).username;
	}
	bool userExists(std::string username) const {
		std::unordered_map<std::string, int>::const_iterator it = this->username_to_id_map.find(username);
	   	return it != this->username_to_id_map.end();
	};
	bool accountMatchesPassword(int account_id, const std::string& password) {
		for (int id :fuze_dbi->queryRows<int>("SELECT id FROM account WHERE password_hash_hash_base64 = $1", password))
			return true;
		return false;
	}
	// bool checkUserKey(int user_id, std::string key) const {
	// 	return this->users.at(user_id).keyMatches(key);
	// }
	// const boost::shared_ptr<std::unordered_map<int, Account>> getAccounts() const override {
	// 	return boost::make_shared<std::unordered_map<int, Account>>(this->accounts);
	// }
	int getGroupRank(int group_id) const override {
		int rank;
		for (rank = 0; this->ordered_groups[rank] != group_id; rank++)
			;
		return rank + 1; // 1 is added because the ADMINISTRATORS group is one rank below OWNER
	}
	void eraseGroup(int group_id) {
		std::vector<int>::const_iterator it = std::find(this->ordered_groups.begin(), this->ordered_groups.end(), group_id);
		std::cout << *it << " should match " << group_id << std::endl;
		for (int member_id : this->groups.at(group_id).getMembers()) {
			this->removeUserFromGroup(member_id, group_id);
		}
		this->ordered_groups.erase(it);
		this->groups.erase(group_id);
		this->saveGroupHeirarchy();
		db_delete_group(group_id);
	}
	void removeUserFromGroup(int user_id, int group_id) {
		this->groups.at(group_id).removeMember(user_id);
		db_remove_member_from_group(user_id, group_id);
	}
	int addGroup(std::string group_name, int group_rank);
	void addUserToGroup(int user_id, int group_id) {
		this->groups.at(group_id).addMember(user_id);
		db_add_member_to_group(user_id, group_id);
	}
	int createAccount(const std::string& username, const char* password_hash_hash, const char* intermediate_salt_base64);
	bool accountExists(const std::string username) const { return this->username_to_id_map.contains(username); }

	// PermissionManager is the highest level, so there is no parent to inherit from
	bool passInheritedPermissionForGroup(bool inherited_permission, PERMISSION permission, int group_id) const override { return inherited_permission; }
	bool passInheritedPermissionForAccount( bool inherited_permission, PERMISSION permission, int account_id ) const override { return inherited_permission; }
	const std::optional<int> owner_id;
protected:
	const Group* getGroup(int group_id) const {
		return &(this->groups.at(group_id));
	}
	bool accountExists(int account_id) const { return this->accounts.contains(account_id); }
	// const Account* getAccount(std::string username) const {
	// 	return &(this->accounts.at(this->username_to_id_map.at(username)));
	// }
	// bool checkUserPassword(int user_id, std::string password) const {
	// 	return this->users.at(user_id).passwordMatches(password);
	// }
	// std::string getUserKey(int user_id) const {
	// 	return this->users.at(user_id).getKey();
	// }
	// const User* createUser(std::string username, std::string password) {
	// 	User new_user(username, password);
	// 	this->users.emplace(new_user.getId(), new_user);
	// 	this->username_to_id_map.emplace(username, new_user.getId());
	// 	// Every registered account is implicitly a member of the "Users" group
	// 	// this->groups.at(static_cast<int>(BUILTIN_GROUPS::USERS)).addMember(new_user.getId());
	// 	return &(this->users.at(new_user.getId()));
	// }
	void cacheAllGroups();
	void cacheAllUsers();

	void setOrderedGroups(std::vector<int> ordered_groups) {
		this->ordered_groups = ordered_groups;
		this->saveGroupHeirarchy(); // Apply changes to the database
	}
	std::unordered_map<int, Account> accounts;
private:
	void grantOwnerPrivileges();
	void saveGroupHeirarchy() const;
	std::unordered_map<std::string, int> username_to_id_map;
	std::unordered_map<int, Group> groups;
	std::vector<int> ordered_groups;
};

class PermissionManagedObject : public PermissionObjectBase {
public:
	// Existing object
	PermissionManagedObject(PermissionObjectBase* parent_object, int permission_object_id, FuzeDBI::Connection* fuze_dbi)
			: PermissionObjectBase(permission_object_id, fuze_dbi), parent_object(parent_object) {
	}
	// New object
	PermissionManagedObject(PermissionObjectBase* parent_object, FuzeDBI::Connection* fuze_dbi)
			: PermissionObjectBase(fuze_dbi), parent_object(parent_object) {
	}
	bool isOwnedBy(const Client& client) const;
	const std::vector<int>* getOrderedGroups() const override {
		return this->parent_object->getOrderedGroups();
	}
	std::vector<int> getOrderedGroupsContainingMember(int user_id) const override {
		return this->parent_object->getOrderedGroupsContainingMember(user_id);
	}
	int getClientRank(const Client& client) const override {
		return this->parent_object->getClientRank(client);
	}
	int getGroupRank(int group_id) const override {
		return this->parent_object->getGroupRank(group_id);
	}
	// const boost::shared_ptr<std::unordered_map<int, Account>> getAccounts() const override {
	// 	return this->parent_object->getAccounts();
	// }
	bool passInheritedPermissionForGroup(bool inherited_permission, PERMISSION permission, int group_id) const override {
		return this->parent_object->passPermissionForGroup(inherited_permission, permission, group_id);
	}
	bool passInheritedPermissionForAccount(bool inherited_permission, PERMISSION permission, int account_id) const override {
		return this->parent_object->passPermissionForAccount(inherited_permission, permission, account_id);
	}
private:
	// std::optional<int> owner_account_id;
	PermissionObjectBase* parent_object;
};
#endif
