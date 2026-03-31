#include "db_interface.h"
#include <iostream>

// Caution: only insert values just before NUMBER_OF_PERMISSIONS; otherwise existing database entries will be messed up
enum struct PERMISSION {  // Mirrors PermissionCollection.permissions in _permissions.js
	MANAGE_PERMISSIONS,
	VIEW_THREAD,
	CREATE_THREAD,
	SEND_MESSAGE,
	DELETE_POST,
	// AUTHOR_DELETE_THREAD,
	// NON_AUTHOR_DELETE_THREAD,
	// NON_AUTHOR_VIEW_MESSAGE,
	// NON_AUTHOR_VIEW_THREAD,
	// UPLOAD_FILE,
	// NON_AUTHOR_DELETE_FILE,
	NUMBER_OF_PERMISSIONS
};

enum struct THREE_STATE_SETTING { DENY, INHERIT, ALLOW };

// A seperate PermissionSetting class is used for futureproofing; 
// in Permissions 2 custom constraints will be added.
class PermissionSetting {
public:
	PermissionSetting(db_permission_setting_struct* permission_setting)
			: id(permission_setting->id),
			setting(static_cast<THREE_STATE_SETTING>(permission_setting->setting)) {
		std::cout << "[PermissionSetting] Cached with ID " << id << std::endl;
	}
	PermissionSetting(int permission_collection_id, PERMISSION permission, THREE_STATE_SETTING setting)
			: /*permission_collection_id(permission_collection_id), permission(permission),*/ setting(setting) {
		this->id = db_store_permission_setting(permission_collection_id, static_cast<int>(permission), static_cast<int>(this->setting));
	}
	bool getBool(bool inherited_permission) const {
		if (this->setting == THREE_STATE_SETTING::INHERIT)
			return inherited_permission;
		else {
			// TODO only return non-inherited setting when constraints (if any) return true
			return this->setting == THREE_STATE_SETTING::ALLOW;
		}
	}
	THREE_STATE_SETTING get() const { return this->setting; }
	void set(THREE_STATE_SETTING setting) {
		if (setting != this->setting) {
			this->setting = setting;
			db_update_permission_setting(this->id, static_cast<int>(this->setting));
		}
	}
private:
	int id;
	// int permission_collection_id;
	// PERMISSION permission;
	THREE_STATE_SETTING setting;
};
