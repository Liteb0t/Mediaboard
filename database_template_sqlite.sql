CREATE TABLE permission_collection(id INTEGER PRIMARY KEY AUTOINCREMENT, permission_object_id INTEGER, account_id INTEGER, permission_group_id INTEGER);
CREATE TABLE permission_setting(id INTEGER PRIMARY KEY AUTOINCREMENT, permission_collection_id INTEGER, permission_number INTEGER, setting INTEGER);
CREATE TABLE permission_group_heirarchy(rank INTEGER, permission_group INTEGER);
