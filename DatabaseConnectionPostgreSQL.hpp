#include "DatabaseConnection.hpp"
#include "libpq-fe.h"

class DatabaseConnectionPostgreSQL : public DatabaseConnection {
public:
	DatabaseConnectionPostgreSQL(const std::string& postgresql_uri, const std::string& program_version_string);
	DatabaseConnectionPostgreSQL(const std::string& postgresql_user, const std::string& postgresql_host, const unsigned short postgresql_port, const std::string& postgresql_database_name, const std::string& current_version);
	~DatabaseConnectionPostgreSQL();
	// void init() override {};
	int getUniquePermissionObjectId() const override { return db_get_unique_permission_object_id(); }
	int storePermissionCollection(int permission_object_id, USER_OR_GROUP user_or_group, int user_or_group_id) override;

	void declarePermissionCollectionCursor(int permission_object_id) override { db_create_cursor_for_permission_collection(permission_object_id); }
	db_permission_collection_struct* getValueFromPermissionCollectionCursor() override { return db_cursor_retrieve_permission_collection(); }
	void closePermissionCollectionCursor() override { db_free_cursor_for_permission_collection(); }
	void declarePermissionSettingCursor(int permission_collection_id) override { db_create_cursor_for_permission_setting(permission_collection_id); };
	db_permission_setting_struct* getValueFromPermissionSettingCursor() override { return db_cursor_retrieve_permission_setting(); };
	void closePermissionSettingCursor() override { db_free_cursor_for_permission_setting(); };
private:
	PGconn* db;
	void migrateIfVersionIsNewer(const std::string& program_version_string);
	void writeDatabaseVersion(const std::string& program_version_string);
	// const std::string getDatabaseVersion() const override;
	// void connectToDatabase(const std::string& connection_target) override;
	void execWriteOnlyStatement(const std::string& statement);
	void execWriteOnlyStatement(const char* statement);
	void execMultipleWriteOnlyStatements(std::istream& stream);
	bool writeMigrations(std::ostream& stream, const std::string& database_version_string);
};
