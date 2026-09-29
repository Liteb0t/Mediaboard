// Copyright (c) 2026, Fuze.page
// Fuze Human-oriented License v1
export module Mediaboard.Permission;
import FuzeHttp.PermissionSetting;

export namespace Mediaboard {
enum struct PERMISSION : int {
	MANAGE_PERMISSIONS,
	VIEW_THREAD,
	CREATE_THREAD,
	SEND_MESSAGE,
	DELETE_POST,
	UPLOAD_FILE,
	VIEW_BOARD, // also allows joining live rooms
	CREATE_BOARD, // or edit
	DELETE_BOARD,
	ROOM_SHARE_MEDIA, // for screensharing or mic
	// AUTHOR_DELETE_THREAD,
	// NON_AUTHOR_DELETE_THREAD,
	// NON_AUTHOR_VIEW_MESSAGE,
	// NON_AUTHOR_VIEW_THREAD,
	// NON_AUTHOR_DELETE_FILE,
	NUMBER_OF_PERMISSIONS
};
}
