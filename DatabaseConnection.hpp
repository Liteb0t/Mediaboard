#pragma once
#include "db_interface.h"
// #include <array>
// #include <memory>
#include <sodium.h>
#include <string>

enum struct USER_OR_GROUP {USER, GROUP};

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

typedef struct {unsigned char value[crypto_pwhash_SALTBYTES];} IntermediateSalt;

class DatabaseConnection {
public:
	virtual ~DatabaseConnection() {
	}
	virtual int createAccount(const char* username, const char* password_hash_hash, const char* intermediate_salt_base64) = 0;
	virtual int getAccountByUsername(const std::string& username) = 0;
	virtual std::string getIntermediateSaltFromAccount(int account_id) = 0;
	virtual bool userMatchesPassword(int account_id, const std::string& password_hash_hash_base64) = 0;

	virtual int getUniquePermissionObjectId() const = 0;
	virtual int storePermissionCollection(int permission_object_id, USER_OR_GROUP user_or_group, int user_or_group_id) = 0;
	virtual int storePermissionSetting(int permission_collection_id, PERMISSION permission, THREE_STATE_SETTING setting) = 0;
	virtual void updatePermissionSetting(int permission_setting_id, THREE_STATE_SETTING setting) = 0;

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

	virtual void getSecret(char* secret_base64) = 0;
};
