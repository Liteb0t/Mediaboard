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
	class TestIteratorSQLite : public TestIterator {
	public:
		TestIteratorSQLite(DatabaseConnectionSQLite* db) : db(db) {}
		void printClassType() const override;
	private:
		DatabaseConnectionSQLite* db;
	};
	TestIterator* getTestIterator() override {
		TestIteratorSQLite* it = new TestIteratorSQLite(this);
		return it;
	}
	std::string class_type = "DatabaseConnectionSQLite";
	const std::string& getClassType() const { return this->class_type; }
	class TestRangeSQLite : public TestRange {
	public:
		virtual int operator*() override {
			return this->data[index];
		}
	private:
		std::array<int, 5> data = {1, 2, 3, 4, 5};
		virtual int getId() const override { return index; }
		virtual bool isEnd() const override { return index == 4; }
	};
	class TestRangeInitialiserSQLite : public TestRangeInitialiser {
	public:
		virtual TestRange* begin() override { return new TestRangeSQLite(); }
	};

	virtual TestRangeInitialiser& getTestRangeInitialiser() { return new TestRangeInitialiserSQLite(); }
	// TestRange* getTestRange() override {
	// 	TestRangeSQLite* it = new TestRangeSQLite();
	// 	return it;
	// }
private:
	sqlite3* db;
	sqlite3_stmt* stmt;
	sqlite3_stmt* stmt2;
	void writeDatabaseVersion(const std::string& program_version_string);
	// const std::string getDatabaseVersion() const override;
	// void connectToDatabase(const std::string& connection_target) override;
	void execWriteOnlyStatement(std::string& statement);
	void execWriteOnlyStatement(const char* statement);
	void execMultipleWriteOnlyStatements(std::istream& stream);
	bool writeMigrations(std::ostream& stream, const std::string& database_version_string);

	void declarePermissionCollectionCursor(int permission_object_id) override;
	db_permission_collection_struct* getValueFromPermissionCollectionCursor() override;
	void closePermissionCollectionCursor() override;
	void declarePermissionSettingCursor(int permission_collection_id) override;
	db_permission_setting_struct* getValueFromPermissionSettingCursor() override;
	void closePermissionSettingCursor() override;
};
