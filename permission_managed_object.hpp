#ifndef PERMISSION_MANAGED_OBJECT
#define PERMISSION_MANAGED_OBJECT

#include <algorithm>
#include <iostream>
#include <vector>
#include <boost/smart_ptr.hpp>
#include <nlohmann/json.hpp>
#include "permission_collection.hpp"
#include "user.hpp"
#include "group.hpp"

enum class BUILTIN_GROUPS { ADMINISTRATORS, USERS, PUBLIC };

class PermissionObjectBase : public boost::enable_shared_from_this<PermissionObjectBase> {
public:
	PermissionObjectBase(int permission_object_id);
	void cacheAllPermissions(/*int permission_object_id*/);
	void addGroupPermissionCollection(int group_id) {
		std::cout << "[PermissionObjectBase] adding group permission_collection for group" << group_id << std::endl;
		PermissionCollection permission_collection(this->permission_object_id, USER_OR_GROUP::GROUP, group_id);
		this->group_permissions.emplace(group_id, permission_collection);
	}
	void addUserPermissionCollection(int user_id) {
		std::cout << "[PermissionObjectBase] adding user permission_collection for user" << user_id << std::endl;
		PermissionCollection permission_collection(this->permission_object_id, USER_OR_GROUP::USER, user_id);
		this->user_permissions.emplace(user_id, permission_collection);
	}
	void removeGroupPermissionCollection(int group_id) {
		this->group_permissions.at(group_id).remove();
		this->group_permissions.erase(group_id);
	}
	void removeUserPermissionCollection(int user_id) {
		this->user_permissions.at(user_id).remove();
		this->user_permissions.erase(user_id);
	}
	void setGroupPermission(int group_id, PERMISSION permission_type, THREE_STATE_SETTING setting) {
		std::unordered_map<int, PermissionCollection>::const_iterator group_iterator = this->group_permissions.find(group_id);
		if (group_iterator == this->group_permissions.end())
			this->addGroupPermissionCollection(group_id);
		this->group_permissions.at(group_id).setPermission(permission_type, setting);
	}
	void setUserPermission(int user_id, PERMISSION permission_type, THREE_STATE_SETTING setting) {
		std::unordered_map<int, PermissionCollection>::const_iterator user_iterator = this->user_permissions.find(user_id);
		if (user_iterator == this->user_permissions.end())
			this->addUserPermissionCollection(user_id);
		this->user_permissions.at(user_id).setPermission(permission_type, setting);
	}
	bool passPermissionForGroup(bool inherited_permission, PERMISSION permission, int group_id) const {
		// Check if a group permission is set for this object
		std::unordered_map<int, PermissionCollection>::const_iterator group_iterator = this->group_permissions.find(group_id);
		if (group_iterator != this->group_permissions.end())
			inherited_permission = group_iterator->second.passPermission(permission, inherited_permission);
		return inherited_permission;
	}
	virtual const std::vector<int>* getOrderedGroups() const = 0;
	virtual std::vector<int> getOrderedGroupsContainingMember(int user_id) const = 0;
	virtual bool getInheritedPermission(int user_id, PERMISSION permission) const = 0;
	virtual int getUserRank(int user_id) const = 0;
	virtual int getGroupRank(int group_id) const = 0;
	bool userHasPermission(int user_id, PERMISSION permission) const {
		bool inherited_permission = this->getInheritedPermission(user_id, permission);
		inherited_permission = this->passPermissionForGroup(inherited_permission, permission, static_cast<int>(BUILTIN_GROUPS::PUBLIC));
		inherited_permission = this->passPermissionForGroup(inherited_permission, permission, static_cast<int>(BUILTIN_GROUPS::USERS));
		std::vector<int> user_ordered_groups = this->getOrderedGroupsContainingMember(user_id);
		for (std::vector<int>::const_reverse_iterator it = user_ordered_groups.rbegin(); it != user_ordered_groups.rend(); it++) {
		// std::for_each(user_ordered_groups.rbegin(), user_ordered_groups.rend(), [permission](int &group_id) { passPermissionForGroup(group_id)
		// for (int group_id : this->getOrderedGroupsContainingMember(user_id)) {
			inherited_permission = this->passPermissionForGroup(inherited_permission, permission, *it);
		}
		std::unordered_map<int, PermissionCollection>::const_iterator user_iterator = this->user_permissions.find(user_id);
		if (user_iterator != this->user_permissions.end()) {
			std::cout << "user Id found in user_permissions\n";
			inherited_permission = user_iterator->second.passPermission(permission, inherited_permission);
		}

		return inherited_permission;
	}
	bool userHasPermissionForGroup(int user_id, PERMISSION permission, int group_id) const {
		if (!this->userHasPermission(user_id, permission))
			return false;
		return this->getUserRank(user_id) < this->getGroupRank(group_id);
	}
	bool userHasPermissionForUser(int user_id, PERMISSION permission, int _user_id) const {
		if (!this->userHasPermission(user_id, permission))
			return false;
		return this->getUserRank(user_id) < this->getUserRank(_user_id);
	}
	virtual const boost::shared_ptr<std::unordered_map<int, User>> getUsers() const = 0;
protected:
	nlohmann::json getPermissionCollectionsAsJson(int client_id) const;
private:
	int permission_object_id; // Used to identify this object in the database
	std::unordered_map<int, PermissionCollection> user_permissions;
	std::unordered_map<int, PermissionCollection> group_permissions;
};

class PermissionManager : public PermissionObjectBase {
public:
	PermissionManager(int permission_object_id)
			: PermissionObjectBase(0) {}
	bool getInheritedPermission(int user_id, PERMISSION permission) const {
		return false;
	}
	const std::vector<int>* getOrderedGroups() const {
		return &(this->ordered_groups);
	}
	std::vector<int> getOrderedGroupsContainingMember(int user_id) const {
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
	int getUserRank(int user_id) const {
		int i;
		for (i = 0; i < this->ordered_groups.size(); i++) {
			if (this->groups.at(ordered_groups[i]).containsMember(user_id))
				break;
		}
		return i;
	}
	int getIdFromUsername(std::string username) const {
		return this->username_to_id_map.at(username);
	}
	bool userExists(std::string username) const {
		std::unordered_map<std::string, int>::const_iterator it = this->username_to_id_map.find(username);
	   	return it != this->username_to_id_map.end();
	};
	bool checkUserKey(int user_id, std::string key) const {
		return this->users.at(user_id).keyMatches(key);
	}
	const boost::shared_ptr<std::unordered_map<int, User>> getUsers() const {
		return boost::make_shared<std::unordered_map<int, User>>(this->users);
	}
	int getGroupRank(int group_id) const {
		int rank;
		for (rank = 0; this->ordered_groups[rank] != group_id; rank++)
			;
		return rank;
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
		// TODO apply erase group without requiring a server restart...
		// ...This would involve finding all permission collections linked to the group and removing them.
	}
	void removeUserFromGroup(int user_id, int group_id) {
		this->groups.at(group_id).removeMember(user_id);
		db_remove_member_from_group(user_id, group_id);
	}
protected:
	const Group* getGroup(int group_id) const {
		return &(this->groups.at(group_id));
	}
	int addGroup(std::string group_name, int group_rank);
	void addUserToGroup(int user_id, int group_id) {
		this->groups.at(group_id).addMember(user_id);
		db_add_member_to_group(user_id, group_id);
	}
	bool userExists(int user_id) const {
		std::unordered_map<int, User>::const_iterator it = this->users.find(user_id);
	   	return it != this->users.end();
	};
	const User* getUser(int user_id) const {
		return &(this->users.at(user_id));
	}
	// const User* getUser(std::string username) const {
	// 	return &(this->users.at(this->usename_to_id_map.at(username)));
	// }
	bool checkUserPassword(int user_id, std::string password) const {
		return this->users.at(user_id).passwordMatches(password);
	}
	std::string getUserKey(int user_id) const {
		return this->users.at(user_id).getKey();
	}
	const User* createUser(std::string username, std::string password) {
		User new_user(username, password);
		this->users.emplace(new_user.getId(), new_user);
		this->username_to_id_map.emplace(username, new_user.getId());
		// Every registered account is implicitly a member of the "Users" group
		// this->groups.at(static_cast<int>(BUILTIN_GROUPS::USERS)).addMember(new_user.getId());
		return &(this->users.at(new_user.getId()));
	}
	void cacheAllGroups();
	void cacheAllUsers();

	void setOrderedGroups(std::vector<int> ordered_groups) {
		this->ordered_groups = ordered_groups;
		this->saveGroupHeirarchy(); // Apply changes to the database
	}
	// TODO remove this duplicate of addUserToGroup
	void toGroupAddMember(int group_id, int user_id) {
		std::cout << "Adding user " << user_id << " to group " << group_id << std::endl;
		this->groups.at(group_id).addMember(user_id);
	}
private:
	void saveGroupHeirarchy() const;
	std::unordered_map<int, User> users;
	std::unordered_map<std::string, int> username_to_id_map;
	std::unordered_map<int, Group> groups;
	std::vector<int> ordered_groups;
};

class PermissionManagedObject : public PermissionObjectBase {
public:
	PermissionManagedObject(boost::shared_ptr<PermissionObjectBase> parent_object, int permission_object_id)
			: PermissionObjectBase(permission_object_id), parent_object(parent_object) {}
	bool getInheritedPermission(int user_id, PERMISSION permission) const { 
		return this->parent_object->userHasPermission(user_id, permission);
	}
	const std::vector<int>* getOrderedGroups() const {
		return this->parent_object->getOrderedGroups();
	}
	std::vector<int> getOrderedGroupsContainingMember(int user_id) const {
		return this->parent_object->getOrderedGroupsContainingMember(user_id);
	}
	int getUserRank(int user_id) const {
		return this->parent_object->getUserRank(user_id);
	}
	int getGroupRank(int group_id) const {
		return this->parent_object->getGroupRank(group_id);
	}
	const boost::shared_ptr<std::unordered_map<int, User>> getUsers() const {
		return this->parent_object->getUsers();
	}
private:
	boost::shared_ptr<PermissionObjectBase> parent_object;
};
#endif
