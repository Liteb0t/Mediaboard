### Database setup
Install postgresql\
To import the database template:\
`psql -X fuze_mediaboard < fuze_mediaboard_template.sql`\
Add the following line to pg_hba.conf:\
`local   fuze_mediaboard mediaboard_server                       password`\
### Required packages (Debian/Devuan)
`postgresql`\
`ecpg`\
`libboost1.81-dev`\
`libboost-program-options1.81-dev`\
`Imagemagick` build with the delegates for JPEG, PNG, WEBP, and JPEG-XL. On Debian/Devuan the apt build doesn't come with JPEG-XL so you need to build it yourself.\
`make`\
`g++`
### Web server settings [deployment]
Nginx reverse proxy settings:
```
location /mediaboard/ {
  proxy_pass http://localhost:8300/; # Fuze Mediaboard
  proxy_set_header X-Real-IP $remote_addr;
  proxy_set_header Host $host;
  proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
  # WebSocket support
  proxy_http_version 1.1;
  proxy_set_header Upgrade $http_upgrade;
  proxy_set_header Connection "upgrade";
  proxy_send_timeout 7d;
  proxy_read_timeout 7d;
}
```

configure these lines in `index.html`:
```javascript\
const root_url = "/"; // set these URLS according to the server configuration
const api_url = "api/"; // these will be managed by a central config file eventually
const media_url = "media/"; // so you wont need to reconfigure these every update
const thumbnail_url = "media/thumbnails/";
```
And also change localhost:8300 the URL that the nginx reverse proxy is serving:
```javascript
let sock = new WebSocket("ws://localhost:8300");
```

