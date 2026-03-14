#include "DatabaseConnectionPostgres.hpp"

DatabaseConnectionPostgres::DatabaseConnectionPostgres(std::string connection_string, std::string database_name, unsigned short port) {
	db_connect(database_name.c_str(), port);
}
