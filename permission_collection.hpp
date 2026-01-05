#include "permission_setting.hpp"
#include "db_interface.h"
#include <iostream>
#include <unordered_map>

enum struct USER_OR_GROUP {USER, GROUP};

class PermissionCollection {
public:
	PermissionCollection(int permission_object_id, db_permission_collection_struct* permission_collection) {
		this->id = permission_collection->id;
		this->group_id = permission_collection->group_id;
		this->user_id = permission_collection->account_id;
	}
	PermissionCollection(int permission_object_id, USER_OR_GROUP user_or_group, int user_or_group_id)
			: permission_object_id(permission_object_id), user_or_group(user_or_group) {
		if (this->user_or_group == USER_OR_GROUP::USER)
			this->user_id = user_or_group_id;
		else
			this->group_id = user_or_group_id;
		this->id = db_store_permission_collection(this->permission_object_id, this->user_id, this->group_id);
	}
	/*
	~PermissionCollection() {
		db_delete_permission_collection(this->id);
	}*/
	void remove() {
		db_delete_permission_collection(this->id);
		// TODO clear permission settings from database
	}
	bool passPermission(PERMISSION permission_type, bool inherited_permission) const {
		auto permission_iterator = permission_map.find(permission_type);
		if (permission_iterator != permission_map.end()) {
			return permission_iterator->second.getBool(inherited_permission);
		}
		else
			return inherited_permission;
	}
	void addPermissionSetting(db_permission_setting_struct* db_permission_setting) {
		PermissionSetting permission_setting(db_permission_setting);
		permission_map.emplace(static_cast<PERMISSION>(db_permission_setting->permission_number), db_permission_setting);
	}
	void setPermission(PERMISSION permission_type, THREE_STATE_SETTING setting) {
		auto permission_iterator = this->permission_map.find(permission_type);
		if (permission_iterator == this->permission_map.end()) {
			std::cout << "PermissionCollection " << this->id << ": permission " << static_cast<int>(permission_type) << " not found" << std::endl;
			PermissionSetting permission_setting(this->id, permission_type, setting);
			this->permission_map.emplace(permission_type, permission_setting);
		}
		else {
			permission_iterator->second.set(setting);
		}
	}
	/*
	void deletePermission(PERMISSION permission_type) {
		std::unordered_map<PERMISSION, PermissionSetting>::const_iterator it = this->permission_map.find(permission_type);
		// permission_setting is identified by the collection ID and permission_type, as if it's a composite primary key
		db_delete_permission_setting(this->id, static_cast<int>(permission_type));
		this->permission_map.erase(it);
	}*/
	const std::unordered_map<PERMISSION, PermissionSetting>* getPermissionMap() const {
		return &(this->permission_map);
	}

private:
	int id;
	int permission_object_id;
	USER_OR_GROUP user_or_group;
	int user_id = -1;
	int group_id = -1;
	std::unordered_map<PERMISSION, PermissionSetting> permission_map;
};
