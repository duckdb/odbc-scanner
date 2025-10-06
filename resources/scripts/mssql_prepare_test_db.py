#!/usr/bin/env python
# -*- coding: utf-8 -*-

from argparse import ArgumentParser
import pyodbc

def exec_sql(cur, sql):
  print(sql)
  cur.execute(sql)

parser = ArgumentParser()
parser.add_argument("conn_str")
args = parser.parse_args()
print(args.conn_str)

conn = pyodbc.connect(args.conn_str)
# https://github.com/mkleehammer/pyodbc/issues/503
conn.autocommit = True
cur = conn.cursor()

exec_sql(cur, "SELECT @@version")
print(cur.fetchone()[0])

exec_sql(cur, "DROP DATABASE IF EXISTS odbcscanner_test_db")
exec_sql(cur, "CREATE DATABASE odbcscanner_test_db")
