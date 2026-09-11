<h1><img src="https://fuze.page/static/fuze-min-hover.png" style="max-width: 100%;" width="94" height="45" alt="FUZE"> Mediaboard</h1>
Version <b>0.2</b><br>
View: <a href="https://github.com/Liteb0t/Mediaboard">Git</a>, <a href="https://fuze.page/software/mediaboard/documentation/">Documentation</a>, <a href="https://fuze.page/software/mediaboard/">Overview</a><br>
<p>
	<a href="https://fuze.page/software/mediaboard/">Fuze Mediaboard</a> is a self-hosted multimedia messaging platform, running on the <a href="https://github.com/Liteb0t/FuzeHttp">FuzeHttp</a> framework. It is written for humans, by humans.
</p>
<h4>Features include:</h4>
<ul>
	<li>Advanced permissions</li>
	<li>Audio, Image, and video embeds</li>
	<li>Easy database migration on update</li>
	<li>File uploading</li>
	<li>Screen-share</li>
	<li>Voice chat</li>
</ul>
<p>Supported OS: FreeBSD, Linux, MacOS. <i>Windows support coming soon.</i></p>
<h2 id="getting-started">Getting started</h2>
<p>
	Running a chat server may seem daunting, but hosting a basic instance is actually simple. As the tutorial progresses, you will start with running a local text-only server, which will evolve into a fully-featured online platform.
</p>
<h3>Compiling from source</h3>
<p>
	Prerequisites: Git, CMake (>= 3.28), Boost (>= 1.88), SQLite OR PostgreSQL.<br><br>
	First ensure that submodules are downloaded. Use this command:<br>
	<code>git submodule update --init --recursive</code><br>
	Now, to compile the server:<br>
	<code>cmake -B build -G Ninja -D WITH_WEBRTC=OFF -D WITH_MAGICK=OFF</code><br>
	<code>cmake --build build</code><br>
	<code>cmake --install build --prefix install</code><br>
	<i>Compiling is known to work with clang-19, but not GCC 14.</i>
</p>
<h3>Running the server</h3>
<p>
	<ul>
		<li>If compiled from source, execute <code>./install/bin/MediaboardServer --create_owner</code></li>
		<li>If using the AppImage, mark the file as executable and then run it.</li>
	</ul>
	The <code>--create_owner</code> flag is required on first boot. When the server starts, an invitation link will appear in the command-line output.
	<figure>
		<img src="https://fuze.page/static/Fuze_Mediaboard_0.1.3_register_owner_account_link.png" alt="Registration link">
		<figcaption>Invitation link to create owner account.</figcaption>
	</figure>
	<figure>
		<img src="https://fuze.page/static/Fuze_Mediaboard_0.1.3_accept_invitation.png" alt="Accepting the invitation">
		<figcaption>A registration form is shown when invitation is opened</figcaption>
	</figure>
	After registering the owner account, you can now interact with the server.<br>
	<i>If you ever forget the password, you can simply run the create_owner command again. Note that it will not be the same account.</i>
</p>
<h2>Making it public</h2>
<p>
	Fuze mediaboard requires HTTPS for secure remote authentication. It is recommended to use a reverse proxy like <a href="https://nginx.org/">NGINX</a>. SSL certificates (required for HTTPS) can be obtained for free using <a href="https://certbot.eff.org/">Certbot</a>.<br>
	<a href="nginx_example.txt">Example NGINX config</a>.<br><br>
	Ensure your IP is accessible to the outside world, using port forwarding if necessary.<br>
</p>
<h3>Permissions</h3>
<p>
	By default, only the owner can read and write messages.
	<ol>
		<li>After logging in, click on the "Manage server" tab in the toolbar as an administrator.</li>
		<li>In the Manage permissions tab, click "Add group" and select "Public".</li>
		<li>Change setting to <i>Allow</i> for the permissions you want to grant.</li>
	</ol>
	<figure>
		<img src="https://fuze.page/static/Fuze_Mediaboard_0.1.3_manage_server.png" alt="Public permissions set in the Manage Server page">
		<figcaption>Granted read/write permissions to Public</figcaption>
	</figure>
	Permissions can also be set on boards, which inherit from server-level settings, and threads, which inherit from boards.<br><br>
	Leaving a setting on <i>Inherit</i> means the permission will be searched by traversing up the inheritance tree (if applicable), and cascading down the group heirarchy until a non-inherited setting is found. If no permission setting is found by the time inheritance passes the server-level setting for <i>Public</i>, permission is denied.<br><br>
	If both a user and group permission are set on an object, the user setting takes priority.
	<figure>
		<img src="https://fuze.page/static/Fuze_Mediaboard_0.1.3_manage_users.png" alt="Manage Users tab">
		<figcaption>Adding groups to a user on the <i>Manage users</i> page.</figcaption>
	</figure>
	<figure>
		<img src="https://fuze.page/static/Fuze_Mediaboard_0.1.3_manage_groups.png" alt="Manage Groups tab">
		<figcaption>In <i>Manage groups</i> you can add/delete groups, change their heirarchy, and dismiss members.</figcaption>
	</figure>
</p>
<h2>Configuration</h2>
<p>
Changes made to <code>config.ini</code> are read when the server starts. These options can also be passed directly in the command line, which will override what is set in the config file.
<table>
	<tr><th colspan=2>config.ini</th></tr>
	<tr><td>server_port</td><td>Serves http://localhost:8300 by default. Change this if running multiple Mediaboard instances.</td></tr>
	<tr><td>threads</td><td>Number of async threads. Values greater than 1 should be used for testing only.</td></tr>
	<tr><td>file_size_limit_mb</td><td>For limiting the maximum size of user-uploaded files and to prevent abuse.</td></tr>
	<tr><td>favicon_url<br>site_name</td><td>Cosmetic settings.</td></tr>
	<tr><td>strip_metadata</td><td>Removes metadata from uploaded images (except SVG files).</td></tr>
	<tr><td>thumbnail_file_exension<br>thumbnail_size</td><td>Format must be supported by ImageMagick. Please note that any change in this setting will not apply to existing thumbnails - those will have to be converted manually.</td></tr>
	<tr><td>avif_thumbnails<br>
		heic_thumbnails<br>
		svg_thumbnails<br>
		webp_thumbnails<br>
		mp4_thumbnails<br>
		webm_thumbnails</td><td>Requires the respective formats to be installed in the instance of ImageMagick used - Use <code>magick -list format</code> to see if they are supported.<br>Video formats require ffmpeg.</td></tr>
	<tr><td>sqlite_database_file</td><td>Path to SQLite database file. Ignored when compiled with the PostgreSQL interface.</td></tr>
	<tr><td>
	postgresql_host<br>
	postgresql_port<br>
	postgresql_user<br>
	postgresql_database_name</td><td>PostgreSQL database connection parameters. Only used when compiled with the PostgreSQL interface.</td></tr>
	<tr><td>environment_variable_for_secret</td>Secret is used for registration, but is not strictly necessary. <td>If <code>secret_required</code>=true, this environment variable must be visible to the server</td></tr>
	<tr><td>secret_required</td><td>Controls whether environment_variable_for_secret must be found</td></tr>
</table>
</p>
<h2>Embedding into a webpage</h2>
<p>
	Fuze Mediaboard can be embedded inside of a page by using an iframe:<br>
	<code>&lt;iframe src="http://localhost:8300/">&lt;/iframe></code>
	<figure>
		<img src="https://fuze.page/static/Fuze_Mediaboard_0.1.3_iframe.png">
		<figcaption>Using Fuze Mediaboard inside of an iframe</figcaption>
	</figure>
	It can be shown with controls hidden by adding <code>display=embed</code> as a URL parameter:</br>
	<pre><code>&lt;iframe src="http://localhost:8300/?display=embed">&lt;/iframe></code></pre>
	<figure>
		<img src="https://fuze.page/static/Fuze_Mediaboard_0.1.3_iframe_embed.png">
		<figcaption>Iframe with <code>display=embed</code></figcaption>
	</figure>
	<code>display=embed</code> also works with individual threads - just add <code>&thread=0</code> (with a valid thread ID) to the path. Embedding threads has a number of potential uses, for example adding comment sections on blog posts, or an artist uploading his/her latest work.
</p>
<h2>Optional: ImageMagick support</h2>
<p>
	In order for thumbnails to work, the software needs to be compiled with <a href="https://imagemagick.org">ImageMagick</a>. The dependency is not included as a submodule; it must be installed seperately.<br>
<h3>Getting ImageMagick</h3>
<p>
	Clone and configure <a href="https://github.com/ImageMagick/ImageMagick">Imagemagick</a> with the delegates for JPEG, PNG, WEBP, XML, and JPEG-XL.<br>
	You may need to install dependencies first. Usually a custom build of ImageMagick is preferred, to ensure all required delegates are installed. FreeBSD's package is sufficient so you can install that via pkg, but Debian's apt package is not. On Debian, you should apt install <code>libxml2-dev</code> and <code>libjxl-dev</code><br>
	Change to the <code>ImageMagick</code> directory, and configure:<br>
	<code>./configure --with-jpeg --with-jxl --with-png --with-xml</code><br>
	Ensure the configure output ends with all the delegates listed:<br>
	<code>DELEGATES         = jng jpeg jxl lcms png xml zlib</code><br>
	Then install:<br>
	<code>make</code><br>
	<code>sudo make install</code>
</p>
<h3>Compiling Fuze Mediaboard with ImageMagick</h3>
<p>
	Configure with <code>-DWITH_MAGICK=ON</code>.<br>
	Note: To build without ImageMagick, add <code>-D WITH_MAGICK=OFF</code> instead to the <code>CMake -B</code> command.
</p>
<strong>ImageMagick may use significant amounts of memory, which can crash the program if system memory runs out. To set memory limits add these environment variables:</strong>
<pre class="notranslate"><code>MAGICK_MEMORY_LIMIT=512MiB
MAGICK_MAP_LIMIT=1GiB
MAGICK_DISK_LIMIT=2GiB
</code></pre>
</p>
<h2>Optional: WebRTC Live Rooms</h2>
<p>
	Live Rooms are similar to Discord's voice channels. Users can share and receive live video and audio, from the desktop, microphone, and webcam. The main difference is Live Rooms are created and closed on-demand, whereas a Discord server has a set number of voice channels created by the server administrator(s).<br><br>
	When this feature is enabled, each Board gets a "Live" page, which will appear in the top navigation bar within each Board.
<h3>Compiling Fuze Mediaboard with WebRTC features</h3>
	The dependencies for this are included as git submodules. The <a href="https://libdatachannel.org/">libdatachannel</a> library is used by the server.<br>
	Configure with <code>-DWITH_WEBRTC=ON</code>.
</p>
<h3 id="ice-servers">ICE servers</h3>
<p>
	On the Manage Server page, there is a tab to configure WebRTC features. There will be a table with columns for "Type", "Hostname", "Port", "Transport", and "Shared secret". ICE servers are often needed to connect remote peers in live rooms, where they might not have a unique public IP, thanks to CGNAT.<br>
	The "Transport" and "Shared secret" values are only used for TURN.
	<figure>
		<img src="https://fuze.page/static/Fuze_Mediaboard_0.2_ice_servers.png" alt="ICE servers config table">
		<figcaption>"Add server" to add a new entry, "Clear" to remove an entry, and "Submit" to update the list stored on the server.</figcaption>
	</figure>
</p>
<h2 id="postgresql-as-the-database">Optional: Using PostgreSQL as the database</h2>
<p>
	Alternatively, you can build with the PostgreSQL interface instead:<br>
	<code>cmake -B build -D FUZEDBI_USE_POSTGRES=ON</code><br>
	<br>
	Set the following options in <code>config.ini</code> to correspond to the database:
	<code>postgresql_host</code>,
	<code>postgresql_port</code>,
	<code>postgresql_user</code>, and
	<code>postgresql_database_name</code>.
</p>
<h3>Start cluster</h3>
<p>
	This may be skipped on certain Linux distros such as Debian. Check if the server is already running with <code>systemctl status postgresql</code> or <code>service postgresql status</code><br>
	<code>initdb -D /var/db/postgres/18/main/</code><br>
	<code>pg_ctl -D /var/db/postgres/18/main start</code><br>
</p>
<h3>Create database</h3>
<p>
	<code>su -</code><br>
	<code>su postgres</code><br>
	<code>createdb fuze_mediaboard</code><br>
	<code>psql -d fuze_mediaboard</code><br>
	fuze_mediaboard=# <code>CREATE USER mediaboard_server WITH PASSWORD '<password>';</code><br>
	fuze_mediaboard=# <code>GRANT ALL ON SCHEMA public TO mediaboard_server;</code><br>
	fuze_mediaboard=# <code>\q</code><br>
	To import the database template, go back to your user account and run:<br>
	<code>psql -U postgres fuze_mediaboard < database_template.sql</code><br>
	Add the following line to <code>pg_hba.conf</code><sup>[<a href="https://www.postgresql.org/docs/15/auth-pg-hba-conf.html">documentation</a>]</sup>. Insert it at the top of the table so that it won't be overridden by other settings:<br>
	<pre><code>local      fuze_mediaboard    mediaboard_server           password</code></pre><br>
	<code>mediaboard_server</code> is the PostgreSQL user which interacts with the database named <code>fuze_mediaboard</code>.\
	Set the environment variable <code>FUZE_MEDIABOARD_PASSWORD</code> with the same password used in the CREATE_USER statement earlier. Open a new terminal window or reboot your system to apply the change.
</p>
<h2>Developer's section</h2>
<p>
	Contributions from any human are welcome. You are invited to use <a href="https://fuze.page/mediaboard/">Fuze.page Mediaboard</a> for any questions/discussion, or to e-mail me privately at <a href="mailto:mash@fuze.page">mash@fuze.page</a>.
</p>
<h3>AI policy</h3>
<p>
AI usage is not principally rejected, but any generated code must be marked with comments indicating the start and end of each AI-assisted section.
<details>
<summary>Example from CMakeLists.txt</summary>
<pre><code>elseif (file MATCHES "\.template\.[^\\\/]+$")
    message("----is template")
    set(frontend_definitions "")
    if (WITH_WEBRTC)
      set(frontend_definitions "-DWITH_WEBRTC=1")
    endif()
    # [AI glasnost] this section assisted by Claude Sonnet 5
    set(qualified_source ${FRONTEND_SOURCE_DIR}/${file})
    set(qualified_destination ${CMAKE_BINARY_DIR}/share/FuzeMediaboard/frontend/${file})
    set(depfile ${CMAKE_BINARY_DIR}/frontend_deps/${file}.d)

    get_filename_component(destination_dir ${qualified_destination} DIRECTORY)
    get_filename_component(depfile_dir ${depfile} DIRECTORY)
    file(MAKE_DIRECTORY ${destination_dir})
    file(MAKE_DIRECTORY ${depfile_dir})

    # Extract template name without .template.* suffix
    string(REGEX REPLACE "\.template\.[^\\\/]+$" "" template_basename "${file}")
    # Convert to uppercase token: index.html → INDEX
    string(TOUPPER "${template_basename}" template_token)
    string(REGEX REPLACE "[^A-Z0-9_]" "_" template_token "${template_token}")

    add_custom_command(
      OUTPUT ${qualified_destination}
      COMMAND ${CMAKE_C_COMPILER} # runs C preprocessor
              -E -P -CC -undef -nostdinc -x c -Wno-trigraphs -Wno-c23-extensions
              -D${template_token} ${frontend_definitions}
              -I ${FRONTEND_SOURCE_DIR} -I ${FRONTEND_SOURCE_DIR}/include
              -MMD -MF ${depfile} -MT ${qualified_destination}
              ${qualified_source} -o ${qualified_destination}
      DEPENDS ${qualified_source}
      DEPFILE ${depfile}
      COMMENT "Preprocessing ${file} :: ${template_token}"
      VERBATIM
    )
    list(APPEND frontend_list ${qualified_destination})
    # [AI glasnost] end AI-assisted section
  else()</code></pre>
</details>
<br>
AI should not be used to make media assets (icons, sounds etc), period. Neither should it be used to generate documentation. Aside from that, feel free to use AI to help understand the code, find bugs, and receive guidance on solutions. Refer to the Fuze Human-oriented License for more info.
</p>
<h3>Creating an AppImage</h3>
<p>
	To create Appdir required by AppImage, run:<br>
	<code>cmake --install build --prefix AppDir/usr</code><br>
	Then to bundle the dependencies, use <a href="https://github.com/linuxdeploy/linuxdeploy">Linuxdeploy</a>:<br>
	<code>./linuxdeploy-x86_64.AppImage --appdir AppDir --output appimage</code>
</p>
<h3>Other useful commands</h3>
<p>
Update submodules to the latest commit:<br>
<code>git submodule update --remote</code><br>
Dump PostgreSQL database:<br>
<code>pg_dump fuze_mediaboard &gt; mediaboard_dump.sql</code><br>
Restore PostgreSQL database backup:<br>
<code>psql -X --set ON_ERROR_STOP=on fuze_mediaboard &lt; mediaboard_dump.sql</code>
</p>
