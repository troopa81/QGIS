#!/usr/bin/env bash

set -e

# Debug env
echo "::group::Print env"
env
echo "::endgroup::"

# Temporarily uncomment to debug ccache issues
# cat /tmp/cache.debug

if [ $# -eq 1 ] && [ $1 = "HANA" ]; then
  LABELS_TO_RUN="HANA"
  RUN_HANA=YES

elif [ $# -eq 1 ] && [ $1 = "POSTGRES" ]; then
  LABELS_TO_RUN="POSTGRES"
  RUN_POSTGRES=YES

elif [ $# -eq 1 ] && [ $1 = "ORACLE" ]; then
  LABELS_TO_RUN="ORACLE"
  RUN_ORACLE=YES

elif [ $# -eq 1 ] && [ $1 = "SQLSERVER" ]; then
  LABELS_TO_RUN="SQLSERVER"
  RUN_SQLSERVER=YES

elif [ $# -eq 1 ] && [ $1 = "ALL_BUT_PROVIDERS" ]; then
  LABELS_TO_EXCLUDE="HANA|POSTGRES|ORACLE|SQLSERVER"

elif [ $# -gt 0 ] &&  [ $1 != "ALL" ]; then
  echo "Invalid argument, expected values: ALL, ALL_BUT_PROVIDERS, POSTGRES, HANA, ORACLE, SQLSERVER"
  exit 1

else
  RUN_HANA=YES
  RUN_POSTGRES=YES
  RUN_ORACLE=YES
  RUN_SQLSERVER=YES
fi

if [ -n "$LABELS_TO_RUN" ]; then
  echo "Only following test labels will be run: $LABELS_TO_RUN"
  CTEST_OPTIONS="-L $LABELS_TO_RUN"
fi

if [ -n "$LABELS_TO_EXCLUDE" ]; then
  echo "Following test labels will be excluded: $LABELS_TO_EXCLUDE"
  CTEST_OPTIONS="$CTEST_OPTIONS -LE $LABELS_TO_EXCLUDE"
fi

if [ ${RUN_HANA:-"NO"} == "YES" ]; then

  ##################################
  # Prepare HANA database connection
  ##################################

  echo "::group::hana"
  echo "${bold}Load HANA database...${endbold}"

  export HANA_HOST=917df316-4e01-4a10-be54-eac1b6ab15fb.hana.prod-us10.hanacloud.ondemand.com
  export HANA_PORT=443
  export HANA_USER=QGISCI
  export HANA_PASSWORD="tQ&7W3Klr9!p"

  export QGIS_HANA_TEST_DB='driver='/usr/sap/hdbclient/libodbcHDB.so' host='${HANA_HOST}' port='${HANA_PORT}' user='${HANA_USER}' password='${HANA_PASSWORD}' sslEnabled=true sslValidateCertificate=False'

  # wait for the DB to be available
  echo "Wait a moment while trying to connect to a HANA database."
  while ! echo exit | hdbsql -n '${HANA_HOST}:${HANA_PORT}' -u '${HANA_USER}' -p '${HANA_PASSWORD}' &> /dev/null
  do
    printf "⚘"
    sleep 1
  done
  echo "🌊 done"

  echo "::endgroup::"
fi

if [ ${RUN_POSTGRES:-"NO"} == "YES" ]; then

  ############################
  # Restore postgres test data
  ############################
  echo "${bold}Load Postgres database...🐘${endbold}"

  printf "[qgis_test]\nhost=postgres\nport=5432\ndbname=qgis_test\nuser=docker\npassword=docker" > ~/.pg_service.conf
  export PGUSER=docker
  export PGHOST=postgres
  export PGPASSWORD=docker
  export PGDATABASE=qgis_test

  # wait for the DB to be available
  echo "Wait a moment while loading PostGreSQL database."
  while ! PGPASSWORD='docker' psql -h postgres -U docker -p 5432 -l &> /dev/null
  do
    printf "🐘"
    sleep 1
  done
  echo " done 🥩"

  pushd /root/QGIS > /dev/null
  echo "Restoring postgres test data ..."
  /root/QGIS/tests/testdata/provider/testdata_pg.sh
  echo "Postgres test data restored ..."
  popd > /dev/null # /root/QGIS

fi

if [ ${RUN_ORACLE:-"NO"} == "YES" ]; then

  ##############################
  # Restore Oracle test data
  ##############################

  echo "${bold}Load Oracle database...🙏${endbold}"

  export ORACLE_HOST="oracle"
  export QGIS_ORACLETEST_DBNAME="${ORACLE_HOST}/XEPDB1"
  export QGIS_ORACLETEST_DB="host=${QGIS_ORACLETEST_DBNAME} port=1521 user='QGIS' password='qgis'"

  echo "Wait a moment while loading Oracle database."
  COUNT=0
  while ! echo exit | sqlplus -L SYSTEM/adminpass@$QGIS_ORACLETEST_DBNAME &> /dev/null
  do
    printf "🙏"
    sleep 5
    if [[ $(( COUNT++ )) -eq 40 ]]; then
      break
    fi
  done
  if [[ ${COUNT} -eq 41 ]]; then
    echo "timeout, no oracle, no 🙏"
  else
    echo " done 👀"
    pushd /root/QGIS > /dev/null
    /root/QGIS/tests/testdata/provider/testdata_oracle.sh $ORACLE_HOST
    popd > /dev/null # /root/QGIS
  fi

fi

if [ ${RUN_SQLSERVER:-"NO"} == "YES" ]; then

  ##############################
  # Restore SQL Server test data
  ##############################

  echo "Importing SQL Server test data..."

  export SQLUSER=sa
  export SQLHOST=mssql
  export SQLPORT=1433
  export SQLPASSWORD='<YourStrong!Passw0rd>'
  export SQLDATABASE=qgis_test

  export PATH=$PATH:/opt/mssql-tools/bin

  pushd /root/QGIS > /dev/null
  /root/QGIS/tests/testdata/provider/testdata_mssql.sh
  popd > /dev/null # /root/QGIS

  echo "Setting up DSN for test SQL Server"

  cat <<EOT > /etc/odbc.ini
[ODBC Data Sources]
testsqlserver = ODBC Driver 17 for SQL Server

[testsqlserver]
Driver       = ODBC Driver 17 for SQL Server
Description  = Test SQL Server
Server       = mssql
EOT

fi

#######################################
# Wait for WebDAV container to be ready
#######################################

echo "Wait for webdav to be ready..."
while ! curl -f -X GET -u qgis:myPasswd! http://$QGIS_WEBDAV_HOST:$QGIS_WEBDAV_PORT/webdav_tests/ &> /dev/null;
do
  sleep 1
  printf "."
done
echo "done"

###########
# Run tests
###########
EXCLUDE_TESTS=$(cat /root/QGIS/.ci/test_blocklist_qt${QT_VERSION}.txt | sed -r '/^(#.*?)?$/d' | paste -sd '|' -)
if ! [[ ${RUN_FLAKY_TESTS} == true ]]; then
  echo "Flaky tests are skipped!"
  EXCLUDE_TESTS=${EXCLUDE_TESTS}"|"$(cat /root/QGIS/.ci/test_flaky.txt | sed -r '/^(#.*?)?$/d' | paste -sd '|' -)
else
  echo "Flaky tests are run!"
fi
echo "List of skipped tests: $EXCLUDE_TESTS"

echo "Print disk space"
df -h

python3 /root/QGIS/.ci/ctest2ci.py xvfb-run ctest -V $CTEST_OPTIONS -E "${EXCLUDE_TESTS}" -S /root/QGIS/.ci/config_test.ctest --output-on-failure

echo "Print disk space"
df -h
