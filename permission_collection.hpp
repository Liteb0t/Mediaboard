#include "DatabaseConnection.hpp"
#include "permission_setting.hpp"
#include "db_interface.h"
#include <iostream>
#include <unordered_map>

class PermissionCollection {
public:
	PermissionCollection(int id, int user_id, int group_id)
			: id(id),
			user_id(user_id),
			group_id(group_id),
			user_or_group(user_id != -1 ? USER_OR_GROUP::USER : USER_OR_GROUP::GROUP) {
		std::cout << "Caching permission collection with user_id " << user_id << " and group_id " << group_id << std::endl;
	}
	PermissionCollection(int permission_object_id, USER_OR_GROUP user_or_group, int user_or_group_id, DatabaseConnection* db)
			: user_or_group(user_or_group),
			user_id(user_or_group == USER_OR_GROUP::USER ? user_or_group_id : -1),
			group_id(user_or_group == USER_OR_GROUP::GROUP ? user_or_group_id : -1),
			id(db->storePermissionCollection(permission_object_id, user_or_group, user_or_group_id)) {
				// this->id = db_store_permission_collection(permission_object_id, this->user_id, this->group_id);
		std::cout << "Storing permission collection with user_id " << user_id << " and group_id " << group_id << std::endl;
	}
	void removeFromDatabase() {
		db_delete_permission_collection(this->id);
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
		permission_map.emplace(static_cast<PERMISSION>(db_permission_setting->permission_number), permission_setting);
	}
	void setPermission(PERMISSION permission_type, THREE_STATE_SETTING setting) {
		auto permission_iterator = this->permission_map.find(permission_type);
		if (permission_iterator == this->permission_map.end()) {
			// std::cout << "PermissionCollection " << this->id << ": permission " << static_cast<int>(permission_type) << " not found" << std::endl;
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
	const USER_OR_GROUP getUserOrGroupEnumValue() const { return this->user_or_group; }

private:
	const int id;
	// int permission_object_id;
	const USER_OR_GROUP user_or_group;
	const int user_id;
	const int group_id;
	std::unordered_map<PERMISSION, PermissionSetting> permission_map;
};
