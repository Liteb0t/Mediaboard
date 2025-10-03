# This is the configuration file for the front-end. For back-end config, see 'config.ini'
# This file is read at compile-time so that index.html has these values inserted.

# Webpage establishes a websocket connection to this URL
define(`_WEBSOCKET_URL', `ws://localhost:8300')
# On HTTPS connections, the URL scheme is 'wss://'. The line below is an example:
# define(`_WEBSOCKET_URL', `wss://example.com/mediaboard/')

define(`_SITE_NAME', `Fuze Mediaboard')

# The path where Fuze Mediaboard is hosted.
define(`_ROOT_URL', `/')
# Path must start and end with a '/'. For example:
# define(`_ROOT_URL', `/mediaboard/')

define(`_FILE_SIZE_LIMIT_MB', `25')

# To prevent javascript strings using the backtick character '`' from being interpreted as quotes in m4:
changequote(`[[[', `]]]')
