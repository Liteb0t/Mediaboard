#include "permission_setting.hpp"
#include "db_interface.h"
#include <unordered_map>

enum struct PERMISSION {
	MANAGE_PERMISSIONS = 0
	// AUTHOR_DELETE_THREAD,
	// NON_AUTHOR_DELETE_THREAD,
	// NON_AUTHOR_VIEW_MESSAGE,
	// NON_AUTHOR_VIEW_THREAD,
	// UPLOAD_FILE,
	// NON_AUTHOR_DELETE_FILE
};

class PermissionCollection {
public:
	PermissionCollection() {
		// TODO add entry to database
		this->id = -1;
	}
	PermissionCollection(int id) : id(id) {}
	bool passPermission(PERMISSION permission_type, bool inherited_permission) const {
		auto permission_iterator = permission_map.find(permission_type);
		if (permission_iterator != permission_map.end()) {
			return permission_iterator->second.getBool(inherited_permission);
		}
		else
			return inherited_permission;
	}
	void setPermission(PERMISSION permission_type, THREE_STATE_SETTING setting) {
		auto permission_iterator = permission_map.find(permission_type);
		if (permission_iterator == permission_map.end()) {
			PermissionSetting permission_setting(setting);
			permission_map.emplace(permission_type, permission_setting);
		}
		else {
			permission_iterator->second.set(setting);
		}
		// TODO remove permission from map when there are no constrains and setting == INHERIT
	}
	/*
	void deletePermission(PERMISSION permission_type) {
		std::unordered_map<PERMISSION, PermissionSetting>::const_iterator it = this->permission_map.find(permission_type);
		// permission_setting is identified by the collection ID and permission_type, as if it's a composite primary key
		db_delete_permission_setting(this->id, static_cast<int>(permission_type));
		this->permission_map.erase(it);
	}*/

private:
	int id;
	std::unordered_map<PERMISSION, PermissionSetting> permission_map;
};
