CREATE TABLE account(id INTEGER PRIMARY KEY AUTOINCREMENT, username TEXT, password_hash TEXT, key TEXT);
CREATE TABLE permission_group(id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT);
CREATE TABLE permission_group_heirarchy(rank INTEGER, permission_group INTEGER);
CREATE TABLE permission_group_account(group_id INTEGER, account_id INTEGER);
CREATE TABLE permission_collection(id INTEGER PRIMARY KEY AUTOINCREMENT, permission_object_id INTEGER, account_id INTEGER, permission_group_id INTEGER);
CREATE TABLE permission_setting(id INTEGER PRIMARY KEY AUTOINCREMENT, permission_collection_id INTEGER, permission_number INTEGER, setting INTEGER);
