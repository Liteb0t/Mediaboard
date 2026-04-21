#include "DatabaseConnection.hpp"
#include "db_interface.h"
#include <boost/filesystem/path.hpp>
#include <sqlite3.h>

class DatabaseConnectionSQLite : public DatabaseConnection {
public:
	DatabaseConnectionSQLite(const boost::filesystem::path& database_directory, const std::string& filename, const std::string& program_version_string);
	~DatabaseConnectionSQLite() override;
	void firstTimeSetup(const boost::filesystem::path& database_directory, const std::string& program_version_string);
	int getUniquePermissionObjectId() const override { return -1; } // Not yet implemented
	int storePermissionCollection(int permission_object_id, USER_OR_GROUP user_or_group, int user_or_group_id) override;
	int storePermissionSetting(int permission_collection_id, PERMISSION permission, THREE_STATE_SETTING setting) override;
	void updatePermissionSetting(int permission_setting_id, THREE_STATE_SETTING setting) override;
private:
	sqlite3* db;
	sqlite3_stmt* stmt;
	sqlite3_stmt* stmt2; // Used when two cursors are active at once, for the "inner" cursor
	void getSecret(char* secret_base6) override;
	void writeDatabaseVersion(const std::string& program_version_string);
	// const std::string getDatabaseVersion() const override;
	// void connectToDatabase(const std::string& connection_target) override;
	void execWriteOnlyStatement(const std::string& statement);
	void execWriteOnlyStatement(const char* statement);
	void execMultipleWriteOnlyStatements(std::istream& stream);
	bool writeMigrations(std::ostream& stream, const std::string& database_version_string);

	int createAccount(const char* username, const char* password_hash, const char* intermediate_salt_base64) override;
	int getAccountByUsername(const std::string& username) override;
	std::string getIntermediateSaltFromAccount(int account_id) override;
	bool userMatchesPassword(int account_id, const std::string& password_hash_hash_base64) override;

	void declareAccountCursor() override;
	db_account_struct* getValueFromAccountCursor() override;
	void closeAccountCursor() override;

	void declareGroupCursor() override;
	db_group_struct* getValueFromGroupCursor() override;
	void closeGroupCursor() override;
	void declareGroupHeirarchyCursor() override;
	db_group_heirarchy_struct* getValueFromGroupHeirarchyCursor() override;
	void closeGroupHeirarchyCursor() override;
	void declareGroupMemberCursor() override;
	db_group_member_struct* getValueFromGroupMemberCursor() override;
	void closeGroupMemberCursor() override;

	void declarePermissionCollectionCursor(int permission_object_id) override;
	db_permission_collection_struct* getValueFromPermissionCollectionCursor() override;
	void closePermissionCollectionCursor() override;
	void declarePermissionSettingCursor(int permission_collection_id) override;
	db_permission_setting_struct* getValueFromPermissionSettingCursor() override;
	void closePermissionSettingCursor() override;
};
