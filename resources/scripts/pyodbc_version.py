#!/usr/bin/env python
# -*- coding: utf-8 -*-

from argparse import ArgumentParser
import time
import pyodbc

parser = ArgumentParser()
parser.add_argument("--dbms", required=True)
parser.add_argument("--conn-str", required=True)
args = parser.parse_args()
print(f"DBMS: {args.dbms}")
print(args.conn_str)

def exec_sql(cur, sql):
  print(sql)
  cur.execute(sql)

def connect_await_db_startup(attempts=16):
  for i in range(attempts):
    try:
      return pyodbc.connect(args.conn_str, autocommit=True)
    except Exception as e:
      print(e)
      time.sleep(10)

def run_version():
  conn = connect_await_db_startup()
  cur = conn.cursor()
  exec_sql(cur, "SELECT version()")
  print(cur.fetchone()[0])

def run_mssql():
  conn = connect_await_db_startup()
  cur = conn.cursor()
  exec_sql(cur, "SELECT @@version")
  print(cur.fetchone())

def run_oracle():
  conn = connect_await_db_startup()
  cur = conn.cursor()
  exec_sql(cur, "SELECT * FROM PRODUCT_COMPONENT_VERSION")
  print(cur.fetchone())

def run_db2():
  conn = connect_await_db_startup()
  cur = conn.cursor()
  exec_sql(cur, "SELECT * FROM SYSIBMADM.ENV_INST_INFO")
  print(cur.fetchone())

def run_snowflake():
  conn = connect_await_db_startup()
  cur = conn.cursor()
  exec_sql(cur, "SELECT current_version()")
  print(cur.fetchone())

def run_firebird():
  conn = connect_await_db_startup()
  cur = conn.cursor()
  exec_sql(cur, "SELECT rdb$get_context('SYSTEM', 'ENGINE_VERSION') as version FROM rdb$database")
  print(cur.fetchone())

if __name__ == "__main__":
  if args.dbms in [
    "DuckDB",
    "PostgreSQL",
    "MySQL",
    "MariaDB",
    "ClickHouse",
    "Spark",
    "FlightSQL",
    ]:
    run_version()
  elif "MSSQL" == args.dbms:
    run_mssql()
  elif "PostgreSQL" == args.dbms:
    run_postgres()
  elif "Oracle" == args.dbms:
    run_oracle()
  elif "DB2" == args.dbms:
    run_db2()
  elif "Snowflake" == args.dbms:
    run_snowflake()
  elif "Firebird" == args.dbms:
    run_firebird()
  else:
    raise Exception("Unsupported DBMS: " + args.dbms)
