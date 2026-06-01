# This is the configuration file for the front-end. For back-end config, see 'config.ini'
# This file is read at compile-time so that index.html has these values inserted.

# Webpage establishes a websocket connection to this URL
define(`_WEBSOCKET_URL', `ws://localhost:8300')
# On HTTPS connections, the URL scheme is 'wss://'. The line below is an example:
# define(`_WEBSOCKET_URL', `wss://example.com/mediaboard/')

define(`_SITE_NAME', `Change SITE_NAME in frontend/tokens.m4')
define(`_SHOW_WATERMARKS', `true')

# The path where Fuze Mediaboard is hosted.
define(`_ROOT_URL', `/')
# Path must start and end with a '/'. For example:
# define(`_ROOT_URL', `/mediaboard/')
define(`_FAVICON_URL', `https://fuze.page/favicon.ico')

# Legacy option. Required to show thumbnails on images uploaded before 0.1.1
define(`_THUMBNAIL_FILE_FORMAT', `jpg')
# For best results, make sure the thumbnail_size setting in config.ini matches this.
define(`_THUMBNAIL_SIZE', `150')

# Also edit the parser_->body_limit in http_session.cpp
define(`_FILE_SIZE_LIMIT_MB', `100')

define(`_POST_MAX_NAME', `32')
define(`_POST_MAX_FILE_NAME', `205')
define(`_POST_MAX_CONTENT', `5000')
define(`_GROUP_MAX_NAME', `32')
define(`_ACCOUNT_MAX_USERNAME', `32')

# To prevent javascript strings using the backtick character '`' from being interpreted as quotes in m4:
changequote(`[[[', `]]]')
