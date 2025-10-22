# ![FUZE](https://fuze.page/static/fuze-min-hover.png) Mediaboard
### Required packages (Debian 12/Devuan 5)
`postgresql`\
`ecpg`\
`libboost1.81-dev`\
`libboost-program-options1.81-dev`\
`libboost-filesystem1.81-dev`\
`Imagemagick` build with the delegates for JPEG, PNG, WEBP, and JPEG-XL. On Debian/Devuan the apt build doesn't come with JPEG-XL so you need to build it yourself.\
`make`\
`g++`\
`m4`
### Database setup
Install postgresql.\
\
To import the database template, run:\
`psql -X fuze_mediaboard < fuze_mediaboard_template.sql`\
\
Add the following line to [pg_hba.conf](https://www.postgresql.org/docs/15/auth-pg-hba-conf.html). Insert it high enough in the table so that it won't be overridden by other settings:\
`local   fuze_mediaboard mediaboard_server                       password`\
\
`mediaboard_server` is the Postgres user which interacts with the database named `fuze_mediaboard`.\
Set a password for this user. Set an environment variable `FUZE_MEDIABOARD_PASSWORD` with the same password.
### Deployment settings
Example Nginx reverse proxy settings:
```
location /mediaboard/ {
  proxy_pass http://localhost:8300/; # Fuze Mediaboard
  proxy_set_header X-Real-IP $remote_addr;
  proxy_set_header Host $host;
  proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
  client_max_body_size 100M;

  # WebSocket support
  proxy_http_version 1.1;
  proxy_set_header Upgrade $http_upgrade;
  proxy_set_header Connection "upgrade";
  proxy_send_timeout 7d;
  proxy_read_timeout 7d;
}
# Optional: store media in another location
location /mediaboard/media/ {
  try_files $uri $uri/ =404;
  alias /path/to/media/folder/;
}
```
In the example above, Fuze Mediaboard is hosted on `/mediaboard/`.\
Open `tokens.m4` and locate the following line:
```
define(`_WEBSOCKET_URL', `ws://localhost:8300')
```
Set the value to the publicly accessible URL Mediaboard is proxied to.\
In this example, if our domain is *fuze.page*, the value should be `wss://fuze.page/mediaboard/`. **Do not forget: if using HTTPS, set the scheme to `wss://`.**\
Also change the definition of `_ROOT_URL` from `/` to `/mediaboard/`\
Run `make` to apply the changes.
### Storing user-submitted media in a different location
By default, media is stored in `media/`. You can choose a different directory within the server's filesystem to store media. Open `config.ini` and set `media_path` to another location.
