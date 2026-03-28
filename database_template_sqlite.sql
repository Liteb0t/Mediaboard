CREATE TABLE permission_collection(id int AUTOINCREMENT, permission_object_id int, account_id int, permission_group_id int);
CREATE TABLE permission_setting(id int AUTOINCREMENT, permission_collection_id int, permission_number int, setting int);
