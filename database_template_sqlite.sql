CREATE TABLE _info(version TEXT);
INSERT INTO _info VALUES('0.1.0');
CREATE TABLE permission_collection(id int, permission_object_id int, account_id int, permission_group_id int);
CREATE TABLE permission_setting(id int, permission_collection_id int, permission_number int, setting int);
