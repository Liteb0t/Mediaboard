#pragma once
#include <iostream>

namespace FuzeHttp {
// Caution: only insert values just before NUMBER_OF_PERMISSIONS; otherwise existing database entries will be messed up
enum struct PERMISSION {  // Mirrors PermissionCollection.permissions in _permissions.js
	MANAGE_PERMISSIONS,
	VIEW_THREAD,
	CREATE_THREAD,
	SEND_MESSAGE,
	DELETE_POST,
	UPLOAD_FILE,
	// AUTHOR_DELETE_THREAD,
	// NON_AUTHOR_DELETE_THREAD,
	// NON_AUTHOR_VIEW_MESSAGE,
	// NON_AUTHOR_VIEW_THREAD,
	// NON_AUTHOR_DELETE_FILE,
	NUMBER_OF_PERMISSIONS
};

enum struct THREE_STATE_SETTING { DENY = 0, INHERIT = 1, ALLOW = 2 };

// A seperate PermissionSetting class is used for futureproofing; 
// in Permissions 2 custom constraints will be added.
class PermissionSetting {
public:
	PermissionSetting(int id, THREE_STATE_SETTING setting)
			: id(id),
			setting(setting) {
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
	void set(THREE_STATE_SETTING setting) { this->setting = setting; }
	int getId() const { return this->id; }
private:
	int id;
	// int permission_collection_id;
	// PERMISSION permission;
	THREE_STATE_SETTING setting;
};
} // namespace FuzeHttp
