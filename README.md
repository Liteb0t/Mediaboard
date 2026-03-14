# ![FUZE](https://fuze.page/static/fuze-min-hover.png) Mediaboard
## Note: This README is for the 0.0.5 release. The deployment process is being simplified for 0.1 and the updated README will be coming shortly.
### Required packages (Debian 12/Devuan 5)
`postgresql`\
`postgresql-contrib`\
`ecpg`\
`nlohmann-json3-dev`\
`libboost1.88-dev`\
`libboost-program-options1.88-dev`\
`libboost-filesystem1.88-dev`\
`cmake`\
`m4`\
### Imagemagick
Note: FreeBSD users can skip this step because the pkg contains all the required delegates.\
Clone and configure [Imagemagick](https://github.com/ImageMagick/ImageMagick) with the delegates for JPEG, PNG, WEBP, XML, and JPEG-XL.\
You may need to install dependencies first:\
`libxml2-dev`\
`libjxl-dev`\
Configure:\
`./configure --with-jpeg --with-jxl --with-png --with-webp --with-xml`\
Ensure the configure output ends with all the delegates listed:\
`DELEGATES         = jng jpeg jxl lcms png webp xml zlib`\
Then install:\
`make`\
`sudo make install`
### Required packages (FreeBSD 15.0)
`ImageMagick7-nox11`\
`nlohmann-json`\
`boost-libs`\
`postgresql18-server`   Versions 15-17 work too\
`postgresql18-contrib`   ^\
Add the following line to `/etc/rc.conf`:\
`postgresql_enable="YES"`
### Developing on MacOS
Install [Homebrew](https://brew.sh/)\
Brew install: `nlohmann-json` `imagemagick` `boost` `postgresql@15` `meson`\
add to ~/.zshrc:\
`export PATH=/opt/homebrew/Cellar/postgresql@15/15.15/bin:$PATH`\
The pkg-config for postgresql may not work out of the box. If that is the case, follow these instructions:
`Brew ls postgresql | grep pkgconfig`\
`export PATH=/opt/homebrew/Cellar/postgresql@15/15.15/bin:$PATH` - Adjust the postgresql version to match the result from `brew ls` in the line above.\
`export PKG_CONFIG_PATH=/opt/homebrew/Cellar/postgresql@15/15.15/lib/pkgconfig/`\
Use meson instead of make to build. To configure:\
`meson setup build`\
`meson configure --pkg-config-path $PKG_CONFIG_PATH build -Dcpp_std=c++20 -Dcpp_args=-stdlib=libc++`
### Building
Currently there are two options: the `Makefile` and the `meson.build`.\
To build using the Makefile, simply run `make`. \
Do note that libraries may not link with `make` without manual intervention.\
To build with meson, first run:\
`meson setup build`\
Then to build:\
`sh build.sh`\
`cd build`\
`ninja`
### Postgres setup
#### Create the cluster
*This may be skipped on certain Linux distros such as Debian. Check if the server is already running with `systemctl status postgresql` or `service postgresql status`*
`initdb -D /var/db/postgres/18/main/`\
`pg_ctl -D /var/db/postgres/18/main start`\
#### And now that the cluster is running...
To create the database:\
`su -`\
`su postgres`\
`createdb fuze_mediaboard`\
`psql -d fuze_mediaboard`\
fuze_mediaboard=# `CREATE USER mediaboard_server WITH PASSWORD '<password>'`\
fuze_mediaboard=# `\q`\
To import the database template, go back to your user account and run:\
`psql -U postgres fuze_mediaboard < database_template.sql`\
`psql -U postgres fuze_mediaboard < default_groups.sql`\
\
Add the following line to [pg_hba.conf](https://www.postgresql.org/docs/15/auth-pg-hba-conf.html). Insert it at the top of the table so that it won't be overridden by other settings:\
`local   fuze_mediaboard mediaboard_server                       password`\
\
`mediaboard_server` is the Postgres user which interacts with the database named `fuze_mediaboard`.\
Set the environment variable `FUZE_MEDIABOARD_PASSWORD` with the same password used in the CREATE_USER statement earlier. Open a new terminal window or reboot your system to apply the change.
### Administrator account
The administrator is able to delete posts from any user. To create the administrator account:\
`./build/server --create_administrator <password>`\
If you ever forget the password, you can simply run the create_administrator command again.
### Execute the program
If you built using meson, run `./build/server` - You must run the server from the Mediaboard directory, not inside `build`.\
If you built using make, run `./server`.\
In a browser open `localhost:8300`\
You should see an empty page with a toolbar at the top. You can login to the administrator account with username "Administrator" and the password you set earlier\
![Login page](https://cdn.fuze.page/Mediaboard/Tutorial/Mediaboard_login_page.png)
### Manage permissions
By default, users cannot view or create threads or send messages. To enable this, click on the "Manage server" tab in the toolbar as an administrator.\
![Permissions in the Manage Server page](https://cdn.fuze.page/Mediaboard/Tutorial/Mediaboard_manage_permissions.png)
In the Manage permissions tab, click "Add group" and select "Public". Now set the desired permissions to "Allow".
### Deployment
Let's say, for example, the Mediaboard will be accessible under `/mediaboard/`.\
Open `tokens.m4` and locate the following line:
```
define(`_WEBSOCKET_URL', `ws://localhost:8300')
```
Set the value to the publicly accessible URL Mediaboard is proxied to.\
In this example, if our domain is *fuze.page*, the value should be `wss://fuze.page/mediaboard/`. **Do not forget: if using HTTPS, set the scheme to `wss://`.**\
Also change the definition of `_ROOT_URL` from `/` to `/mediaboard/`\
Run `make` to apply the changes, or `sh build.sh` if using Meson.
#### HTTPS support
Fuze Mediaboard does not provide HTTPS. For that, you should use a reverse proxy like [NGINX](https://nginx.org/).\
Example NGINX reverse proxy settings:
```
location /mediaboard/ {
  proxy_pass http://localhost:8300/; # Fuze Mediaboard
  proxy_set_header X-Real-IP $remote_addr;
  proxy_set_header Host $host;
  proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
  client_max_body_size 100M;
  proxy_pass_request_headers on;

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
### Storing user-submitted media in a different location
User-uploaded content is stored in a subdirectory named `media/`. By default, this is in the same directory as the server executable. You can choose a different directory within the server's filesystem to store media; Open `config.ini` and set `media_path` to another location.
### Dumping the database template
After making changes to the database schema, dump it to the repository using this command:\
`pg_dump --schema-only fuze_mediaboard >database_template.sql`
