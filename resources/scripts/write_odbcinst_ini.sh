#!/bin/bash

set -x
set -e

if [ -z "$1" ] || [ -z "$2" ]; then
  echo "Usage: $0 <dbms_name> <odbc_driver_lib_path>"
  exit 1
fi

DBMS_NAME="$1"
ODBC_SHARED_LIB="$2"
ODBCINST_INI=~/.odbcinst.ini

if [ ! -f "${ODBC_SHARED_LIB}" ]; then
  echo "Error: specified file not found: ${ODBC_SHARED_LIB}"
  exit 1
fi

echo "[${DBMS_NAME} Driver]" > "${ODBCINST_INI}"
echo "Driver = ${ODBC_SHARED_LIB}" >> "${ODBCINST_INI}"
echo "" >> "${ODBCINST_INI}"

cat "${ODBCINST_INI}"
