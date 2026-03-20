#include "DatabaseConnectionPostgres.hpp"
#include "DatabaseConnection.hpp"
#include "db_interface.h"

DatabaseConnectionPostgres::DatabaseConnectionPostgres(std::string connection_string, std::string database_name, unsigned short port) {
	db_connect(database_name.c_str(), port);
}
int DatabaseConnectionPostgres::storePermissionCollection(int permission_object_id, USER_OR_GROUP user_or_group, int user_or_group_id) {
	return db_store_permission_collection(permission_object_id,
								   user_or_group == USER_OR_GROUP::USER ? user_or_group_id : -1,
								   user_or_group == USER_OR_GROUP::GROUP ? user_or_group_id : -1);
}
