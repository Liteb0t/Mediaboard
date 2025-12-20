enum struct THREE_STATE_SETTING { DENY, ALLOW, INHERIT };

// A seperate PermissionSetting class is used for futureproofing; 
// in Permissions 2 custom constraints will be added.
class PermissionSetting {
public:
	PermissionSetting(THREE_STATE_SETTING setting) : setting(setting) {}
	// ~PermissionSetting() {
	// 	db_delete_permission_setting(this->id);
	// }
	bool getBool(bool inherited_permission) const {
		if (this->setting == THREE_STATE_SETTING::INHERIT)
			return inherited_permission;
		else {
			// TODO only return non-inherited setting when constraints (if any) return true
			return this->setting == THREE_STATE_SETTING::ALLOW;
		}
	}
	void set(THREE_STATE_SETTING setting) { this->setting = setting; }
private:
	THREE_STATE_SETTING setting;
};
