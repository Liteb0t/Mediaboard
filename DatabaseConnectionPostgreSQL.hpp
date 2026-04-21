#include "DatabaseConnection.hpp"
#include "db_interface.h"
#include "libpq-fe.h"
#include <string>

class DatabaseConnectionPostgreSQL : public DatabaseConnection {
public:
	DatabaseConnectionPostgreSQL(const std::string& postgresql_uri, const std::string& program_version_string);
	DatabaseConnectionPostgreSQL(const std::string& postgresql_user, const std::string& postgresql_host, const unsigned short postgresql_port, const std::string& postgresql_database_name, const std::string& current_version);
	~DatabaseConnectionPostgreSQL();

	int createAccount(const char* username, const char* password_hash, const char* intermediate_salt_base64) override;
	int getAccountByUsername(const std::string& username) override;
	std::string getIntermediateSaltFromAccount(int account_id) override;
	bool userMatchesPassword(int account_id, const std::string& password_hash_hash_base64) override;

	int getUniquePermissionObjectId() const override { return db_get_unique_permission_object_id(); }
	int storePermissionCollection(int permission_object_id, USER_OR_GROUP user_or_group, int user_or_group_id) override;
	int storePermissionSetting(int permission_collection_id, PERMISSION permission, THREE_STATE_SETTING setting) override;
	void updatePermissionSetting(int permission_setting_id, THREE_STATE_SETTING setting) override;

	void declareAccountCursor() override { db_create_cursor_for_account(); }
	db_account_struct* getValueFromAccountCursor() override { return db_cursor_retrieve_account(); }
	void closeAccountCursor() override { db_free_cursor_for_account(); }

	void declareGroupCursor() override { db_create_cursor_for_group(); }
	db_group_struct* getValueFromGroupCursor() override { return db_cursor_retrieve_group(); }
	void closeGroupCursor() override { return db_free_cursor_for_group(); }
	void declareGroupHeirarchyCursor() override { db_create_cursor_for_group_heirarchy(); }
	db_group_heirarchy_struct* getValueFromGroupHeirarchyCursor() override { return db_cursor_retrieve_group_heirarchy(); }
	void closeGroupHeirarchyCursor() override { db_free_cursor_for_group_heirarchy(); }
	void declareGroupMemberCursor() override { db_create_cursor_for_group_member(); }
	db_group_member_struct* getValueFromGroupMemberCursor() override { return db_cursor_retrieve_group_member(); }
	void closeGroupMemberCursor() override { db_free_cursor_for_group_member(); }

	void declarePermissionCollectionCursor(int permission_object_id) override { db_create_cursor_for_permission_collection(permission_object_id); }
	db_permission_collection_struct* getValueFromPermissionCollectionCursor() override { return db_cursor_retrieve_permission_collection(); }
	void closePermissionCollectionCursor() override { db_free_cursor_for_permission_collection(); }
	void declarePermissionSettingCursor(int permission_collection_id) override { db_create_cursor_for_permission_setting(permission_collection_id); };
	db_permission_setting_struct* getValueFromPermissionSettingCursor() override { return db_cursor_retrieve_permission_setting(); };
	void closePermissionSettingCursor() override { db_free_cursor_for_permission_setting(); };
private:
	PGconn* db;
	void getSecret(char* secret_base64) override;
	void migrateIfVersionIsNewer(const std::string& program_version_string);
	void writeDatabaseVersion(const std::string& program_version_string);
	// const std::string getDatabaseVersion() const override;
	// void connectToDatabase(const std::string& connection_target) override;
	void execWriteOnlyStatement(const std::string& statement);
	void execWriteOnlyStatement(const char* statement);
	void execMultipleWriteOnlyStatements(std::istream& stream);
	bool writeMigrations(std::ostream& stream, const std::string& database_version_string);
};
