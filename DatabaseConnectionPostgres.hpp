#include "DatabaseConnection.hpp"
#include "db_interface.h"

class DatabaseConnectionPostgres : public DatabaseConnection {
public:
	DatabaseConnectionPostgres(std::string connection_string, std::string database_name, unsigned short port);
	~DatabaseConnectionPostgres() override {
		db_disconnect();
	}
};
