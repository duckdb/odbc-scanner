#!/usr/bin/env python
# -*- coding: utf-8 -*-

import gc, os, platform, sys
from argparse import ArgumentParser, BooleanOptionalAction
from os import path

script_dir = path.dirname(path.abspath(__file__))
project_dir = path.dirname(path.dirname(script_dir))
if platform.system() != "Windows":
  venv_sitepackages_path = path.join(project_dir, "configure", "venv", "lib", f"python{sys.version_info.major}.{sys.version_info.minor}", "site-packages")
else:
  venv_sitepackages_path = path.join(project_dir, "configure", "venv", "Lib", "site-packages")
sys.path.append(venv_sitepackages_path)

from duckdb_sqllogictest import SQLLogicParser, SQLParserException
from duckdb_sqllogictest.python_runner import SQLLogicTestExecutor
from duckdb_sqllogictest.result import ExecuteResult

parser = ArgumentParser()
parser.add_argument("--dbms", required=False)
parser.add_argument("--debug", action=BooleanOptionalAction)
parser.add_argument("--test-file", required=False)
args = parser.parse_args()

build_mode = "debug" if args.debug else "release"
build_dir = path.join(project_dir, "build", build_mode)
ext_path = path.join(build_dir, "odbc_scanner.duckdb_extension")
sqllogic_tests_dir = path.join(project_dir, "test", "sql")

sql_parser = SQLLogicParser()
executor = SQLLogicTestExecutor(build_dir, sqllogic_tests_dir)
executor.register_external_extension(ext_path)

file_paths = []

if args.test_file is not None:
  print(f"Running SQLLogic tests from file: '{args.test_file}'")
  file_paths.append(path.abspath(args.test_file))
elif args.dbms is not None:
  dbms_dir = path.join(project_dir, "test", "sql", args.dbms.lower())
  print(f"Running SQLLogic tests from directory: '{dbms_dir}'")
  for dirpath, _, fnames in os.walk(dbms_dir):
      for name in fnames:
          if name.endswith(".test"):
            file_paths.append(path.join(dirpath, name))
  file_paths.append(path.join(sqllogic_tests_dir, "close.test"))
  file_paths.append(path.join(sqllogic_tests_dir, "connect.test"))
else:
  print(f"Running connect/close SQLLogic tests")

odbc_conn_string = os.environ.get("ODBC_CONN_STRING", "Driver={DuckDB Driver};")
print(f"ODBC_CONN_STRING: '{odbc_conn_string}'")

file_paths.sort()

total_tests = len(file_paths)
for i, file_path in enumerate(file_paths):
    try:
        test = sql_parser.parse(file_path)
    except SQLParserException as e:
        executor.skip_log.append(str(e.message))
        continue

    file_path_rel = path.relpath(file_path, project_dir)
    print(f'[{i+1}/{total_tests}] {file_path_rel}')
    # This is necessary to clean up databases/connections
    # So previously created databases are not still cached in the instance_cache
    gc.collect()
    result = executor.execute_test(test)
    if result.type == ExecuteResult.Type.SKIPPED:
        print("SKIPPED")
        continue
    if result.type == ExecuteResult.Type.ERROR:
        print("ERROR")
        exit(1)

print("SUCCESS")

for item in executor.skip_log:
    print(item)
executor.skip_log.clear()
