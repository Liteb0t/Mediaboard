#include "DatabaseConnection.hpp"
#include <iostream>

// A seperate PermissionSetting class is used for futureproofing; 
// in Permissions 2 custom constraints will be added.
class PermissionSetting {
public:
	PermissionSetting(db_permission_setting_struct* permission_setting)
			: id(permission_setting->id),
			setting(static_cast<THREE_STATE_SETTING>(permission_setting->setting)) {
		std::cout << "[PermissionSetting] Cached with ID " << id << std::endl;
	}
	PermissionSetting(int permission_collection_id, PERMISSION permission, THREE_STATE_SETTING setting, DatabaseConnection* db)
			: /*permission_collection_id(permission_collection_id), permission(permission),*/ setting(setting) {
		this->id = db->storePermissionSetting(permission_collection_id, permission, setting);
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
	void set(THREE_STATE_SETTING setting, DatabaseConnection* db) {
		if (setting != this->setting) {
			this->setting = setting;
			db->updatePermissionSetting(this->id, this->setting);
		}
	}
private:
	int id;
	// int permission_collection_id;
	// PERMISSION permission;
	THREE_STATE_SETTING setting;
};
