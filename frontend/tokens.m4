# This is the configuration file for the front-end. For back-end config, see 'config.ini'
# This file is used during CMake's build process to fill in macros in the frontend.
changequote(`[', `]')
changequote([`], [`])

define(`_SITE_NAME`, `Fuze Mediaboard`)
define(`_FAVICON_URL`, `https://fuze.page/favicon.ico`)
define(`_SHOW_WATERMARKS`, `true`)

# Legacy option. Required to show thumbnails on images uploaded before 0.1.1
define(`_THUMBNAIL_FILE_FORMAT`, `jpg`)
# For best results, make sure the thumbnail_size setting in config.ini matches this.
define(`_THUMBNAIL_SIZE`, `150`)

# Also edit max_http_body in config.ini. May be removed in a future version.
define(`_FILE_SIZE_LIMIT_MB`, `25`)

# These are not supposed to be changed
define(`_POST_MAX_NAME`, `32`)
define(`_POST_MAX_FILE_NAME`, `205`)
define(`_POST_MAX_CONTENT`, `5000`)
define(`_GROUP_MAX_NAME`, `32`)
define(`_ACCOUNT_MAX_USERNAME`, `32`)
# To prevent javascript strings using the backtick character '`' from being interpreted as quotes in m4:
changequote(`[[[`, `]]]`)
