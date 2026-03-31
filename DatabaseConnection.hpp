#pragma once
#include "db_interface.h"
// #include <array>
// #include <memory>
// #include <string>

enum struct USER_OR_GROUP {USER, GROUP};

class DatabaseConnection {
public:
	virtual ~DatabaseConnection() {
	}
	// virtual void init() = 0;
	virtual int getUniquePermissionObjectId() const = 0;
	virtual int storePermissionCollection(int permission_object_id, USER_OR_GROUP user_or_group, int user_or_group_id) = 0;

	virtual void declareAccountCursor() = 0;
	virtual db_account_struct* getValueFromAccountCursor() = 0;
	virtual void closeAccountCursor() = 0;

	virtual void declareGroupCursor() = 0;
	virtual db_group_struct* getValueFromGroupCursor() = 0;
	virtual void closeGroupCursor() = 0;
	virtual void declareGroupHeirarchyCursor() = 0;
	virtual db_group_heirarchy_struct* getValueFromGroupHeirarchyCursor() = 0;
	virtual void closeGroupHeirarchyCursor() = 0;
	virtual void declareGroupMemberCursor() = 0;
	virtual db_group_member_struct* getValueFromGroupMemberCursor() = 0;
	virtual void closeGroupMemberCursor() = 0;

	virtual void declarePermissionCollectionCursor(int permission_object_id) = 0;
	virtual db_permission_collection_struct* getValueFromPermissionCollectionCursor() = 0;
	virtual void closePermissionCollectionCursor() = 0;
	virtual void declarePermissionSettingCursor(int permission_collection_id) = 0;
	virtual db_permission_setting_struct* getValueFromPermissionSettingCursor() = 0;
	virtual void closePermissionSettingCursor() = 0;
};
