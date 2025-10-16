#!/usr/bin/env python
# -*- coding: utf-8 -*-

from argparse import ArgumentParser
import time
import pyodbc

def exec_sql(cur, sql):
  print(sql)
  cur.execute(sql)

def run_mssql():
  conn = pyodbc.connect(args.conn_str)
  # https://github.com/mkleehammer/pyodbc/issues/503
  conn.autocommit = True
  cur = conn.cursor()
  exec_sql(cur, "SELECT @@version")
  print(cur.fetchone()[0])
  exec_sql(cur, "DROP DATABASE IF EXISTS odbcscanner_test_db")
  exec_sql(cur, "CREATE DATABASE odbcscanner_test_db")

def run_postgres():
  conn = pyodbc.connect(args.conn_str)
  # https://github.com/mkleehammer/pyodbc/issues/503
  conn.autocommit = True
  cur = conn.cursor()
  exec_sql(cur, "SELECT version()")
  print(cur.fetchone()[0])
  exec_sql(cur, "DROP DATABASE IF EXISTS odbcscanner_test_db")
  exec_sql(cur, "CREATE DATABASE odbcscanner_test_db")

def run_mysql():
  conn = pyodbc.connect(args.conn_str)
  cur = conn.cursor()
  exec_sql(cur, "SELECT version()")
  print(cur.fetchone()[0])
  exec_sql(cur, "DROP DATABASE IF EXISTS odbcscanner_test_db")
  exec_sql(cur, "CREATE DATABASE odbcscanner_test_db")

def run_oracle():
  # waiting for DB startup
  for i in range(32):
    try:
      conn = pyodbc.connect(args.conn_str)
      break
    except Exception as e:
      print(e)
      time.sleep(10)

  cur = conn.cursor()
  exec_sql(cur, "SELECT * FROM PRODUCT_COMPONENT_VERSION")
  print(cur.fetchone())

def run_db2():
  # waiting for DB startup
  for i in range(32):
    try:
      conn = pyodbc.connect(args.conn_str)
      break
    except Exception as e:
      print(e)
      time.sleep(10)

  cur = conn.cursor()
  exec_sql(cur, "SELECT * FROM SYSIBMADM.ENV_INST_INFO")
  print(cur.fetchone())

def run_clickhouse():
  conn = pyodbc.connect(args.conn_str)
  cur = conn.cursor()
  exec_sql(cur, "SELECT version()")
  print(cur.fetchone()[0])

parser = ArgumentParser()
parser.add_argument("--dbms", required=True)
parser.add_argument("--conn-str", required=True)
args = parser.parse_args()
print(args.dbms)
print(args.conn_str)

if __name__ == "__main__":
  if "MSSQL" == args.dbms:
    run_mssql()
  elif "PostgreSQL" == args.dbms:
    run_postgres()
  elif "MySQL" == args.dbms or "MariaDB" == args.dbms:
    run_mysql()
  elif "Oracle" == args.dbms:
    run_oracle()
  elif "DB2" == args.dbms:
    run_db2()
  elif "ClickHouse" == args.dbms:
    run_clickhouse()
  else:
    raise Exception("Unsupported DBMS: " + args.dbms)
