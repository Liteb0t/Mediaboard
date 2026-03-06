cmake_minimum_required(VERSION 3.23)

# set(initdb OFF)
set(port "5400" CACHE STRING "TCP port which the database uses. Must not be occupied by another process.")
set(host "localhost")

if (${initdb})
  message("[start_database.cmake] Initialising database")
  execute_process(COMMAND initdb -D database/cluster
    RESULT_VARIABLE ret
  )
  message("[start_database.cmake] result variable: ${ret}")
  if (NOT ret EQUAL 0)
    message("[start_database.cmake] Exited with an error.")
    return()
  endif()
endif()
if(NOT EXISTS "database/cluster/PG_VERSION")
  message("[start_database.cmake] Could not find database cluster folder.
  Are you running from the Mediaboard root directory?
  Did you initialise the database? If not, run:
    cmake -Dinitdb=ON -P start_database.cmake")
  return()
endif()
execute_process(COMMAND pg_ctl -D database/cluster -o "-p ${port}" -l database/log.txt start
  RESULT_VARIABLE ret
)
if (NOT ret EQUAL 0)
  message("[start_database.cmake] Exited with an error.")
  return()
endif()
if (${initdb})
  execute_process(COMMAND createuser --host=${host} -p ${port} mediaboard_server
    RESULT_VARIABLE ret
  )
  if (NOT ret EQUAL 0)
    message("[start_database.cmake] Failed; createuser returned an error.")
    return()
  else()
    message("[start_database.cmake] Successfully created user `mediaboard_server`.")
  endif()
  execute_process(COMMAND createdb --host=${host} --port=${port} fuze_mediaboard
    RESULT_VARIABLE ret
  )
  if (NOT ret EQUAL 0)
    message("[start_database.cmake] Failed; createdb returned an error.")
    return()
  else()
    message("[start_database.cmake] Successfully created database `fuze_mediaboard`.")
  endif()
  execute_process(COMMAND psql --host=${host} --port=${port} fuze_mediaboard -f database_template.sql)
  if (NOT ret EQUAL 0)
    message("[start_database.cmake] Failed; psql returned an error when trying to import data from `database_template.sql`.")
    return()
  else()
    message("[start_database.cmake] Successfully imported data from `database_template.sql`.")
  endif()
  execute_process(COMMAND psql --host=${host} --port=${port} fuze_mediaboard -f default_groups.sql)
  if (NOT ret EQUAL 0)
    message("[start_database.cmake] Failed; psql returned an error when trying to import data from `default_groups.sql`.")
    return()
  else()
    message("[start_database.cmake] Successfully imported data from `default_groups.sql`.")
  endif()
endif()
message("[start_database.cmake] Started database at port ${port}")
message("[start_database.cmake] Run `pg_ctl -D database/cluster stop` if you intend on stopping or deleting the DB.")
