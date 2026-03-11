INSERT INTO permission_group(id, name) VALUES (0, 'Administrators');
INSERT INTO permission_group(id, name) VALUES (1, 'Users');
INSERT INTO permission_group(id, name) VALUES (2, 'Public');
INSERT INTO permission_group_heirarchy(rank, permission_group) VALUES (0, 0);
INSERT INTO permission_group_heirarchy(rank, permission_group) VALUES (1, 1);
INSERT INTO permission_group_heirarchy(rank, permission_group) VALUES (2, 2);
