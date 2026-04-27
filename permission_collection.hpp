#include "permission_setting.hpp"
#include <unordered_map>

enum struct ACCOUNT_OR_GROUP {ACCOUNT, GROUP};

class PermissionCollection {
public:
	// PermissionCollection(int id, std::optional<int> account_id, std::optional<int> group_id)
	PermissionCollection(int id, int account_id, int group_id)
			: id(id),
			account_id(account_id),
			group_id(group_id),
			account_or_group(account_id != -1 ? ACCOUNT_OR_GROUP::ACCOUNT : ACCOUNT_OR_GROUP::GROUP) {
	}
	bool passPermission(PERMISSION permission_type, bool inherited_permission) const {
		auto permission_iterator = permission_map.find(permission_type);
		if (permission_iterator != permission_map.end()) {
			return permission_iterator->second.getBool(inherited_permission);
		}
		else
			return inherited_permission;
	}
	void addPermissionSetting(int permission_setting_id, int permission_number, THREE_STATE_SETTING setting) {
		PermissionSetting permission_setting(permission_setting_id, setting);
		permission_map.emplace(static_cast<PERMISSION>(permission_number), permission_setting);
	}
	void setPermission(PERMISSION permission_type, THREE_STATE_SETTING setting) { permission_map.at(permission_type).set(setting);	}
	bool containsPermissionType(PERMISSION permission_type) const { return this->permission_map.contains(permission_type); }
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
	const ACCOUNT_OR_GROUP getAccountOrGroupEnumValue() const { return this->account_or_group; }
	int getId() const { return this->id; }
private:
	const int id;
	// int permission_object_id;
	const ACCOUNT_OR_GROUP account_or_group;
	const std::optional<int> account_id;
	const std::optional<int> group_id;
	std::unordered_map<PERMISSION, PermissionSetting> permission_map;
};
