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
	PermissionSettingIterator* retrievePermissionSettings(int permission_collection_id) override {
		return new PermissionSettingIteratorSQLite(permission_collection_id);
	}
private:
	void writeDatabaseVersion(const std::string& program_version_string);
	// const std::string getDatabaseVersion() const override;
	// void connectToDatabase(const std::string& connection_target) override;
	void execWriteOnlyStatement(std::string& statement);
	void execWriteOnlyStatement(const char* statement);
	void execMultipleWriteOnlyStatements(std::istream& stream);
	bool writeMigrations(std::ostream& stream, const std::string& database_version_string);
	class PermissionCollectionIteratorSQLite : public DatabaseConnection::PermissionCollectionIterator {
	public:
		PermissionCollectionIteratorSQLite(int permission_object_id) {
			db_create_cursor_for_permission_collection(permission_object_id);
		}
		~PermissionCollectionIteratorSQLite() override {
			db_free_cursor_for_permission_collection();
		}
		db_permission_collection_struct* getValue() const override { return db_cursor_retrieve_permission_collection(); }
	};
	PermissionCollectionIterator* retrievePermissionCollections(int permission_object_id) override {
		return new PermissionCollectionIteratorSQLite(permission_object_id);
	}
	class PermissionSettingIteratorSQLite : public DatabaseConnection::PermissionSettingIterator {
	public:
		PermissionSettingIteratorSQLite(int permission_collection_id) {
			db_create_cursor_for_permission_setting(permission_collection_id);
		}
		~PermissionSettingIteratorSQLite() override {
			db_free_cursor_for_permission_setting();
		}
		db_permission_setting_struct* getValue() const override { return db_cursor_retrieve_permission_setting(); }
	};
	sqlite3* db;
};
