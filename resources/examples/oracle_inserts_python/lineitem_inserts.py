import duckdb
from os import path
import time

scale_factor = 0.001
batch_size = 16
platform = "linux_amd64"
oracle_conn_str = "Driver={Oracle Driver};DBQ=//127.0.0.1:1521/XE;UID=system;PWD=tiger;"

script_dir = path.dirname(path.abspath(__file__))

def read_sql(filename):
  filepath = path.join(script_dir, filename)
  with open(filepath, "r") as fd:
    sql = fd.read()
    return sql.replace("'", "''")

def init_oracle_conn(cur):
  # cur.execute(f"INSTALL 'http://nightly-extensions.duckdb.org/v1.2.0/{platform}/odbc_scanner.duckdb_extension.gz';")
  cur.execute("LOAD odbc_scanner")
  cur.execute(f"SET VARIABLE conn = odbc_connect('{oracle_conn_str}')")
  cur.execute("FROM odbc_query(getvariable('conn'), 'SELECT * FROM PRODUCT_COMPONENT_VERSION')")
  print(cur.fetchone())
  return cur

def create_lineitem_table(cur):
  cur.execute("FROM odbc_query(getvariable('conn'), 'SELECT count(*) FROM USER_TABLES WHERE TABLE_NAME = ''LINEITEM''')")
  if cur.fetchone()[0] > 0:
    cur.execute("FROM odbc_query(getvariable('conn'), 'DROP TABLE LINEITEM')")
  lineitem_create_sql = read_sql("ora_lineitem_create.sql")
  cur.execute(f"FROM odbc_query(getvariable('conn'), '{lineitem_create_sql}')")

def open_lineitem_reader():
  cur_reader = duckdb.connect()
  cur_reader.execute("INSTALL tpch")
  cur_reader.execute("LOAD tpch")
  cur_reader.execute(f"CALL dbgen(sf = {scale_factor})")
  cur_reader.execute("SELECT count(*) FROM lineitem")
  count = cur_reader.fetchone()[0]
  print(f"Records: {count}")
  cur_reader.execute("SELECT * FROM lineitem")
  return cur_reader

def lineitem_questionmarks():
  return "?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?"

def insert_records_one_by_one_direct_params(cur_reader, cur):
  print("Inserting records one by one using direct params")
  lineitem_insert_single_sql = read_sql("ora_lineitem_insert_single.sql")
  questionmarks = lineitem_questionmarks()

  cur.execute("SELECT odbc_begin_transaction(getvariable('conn'))")

  count = 0
  while True:
    row = cur_reader.fetchone()
    if row is None:
      break
    cur.execute(f"FROM odbc_query(getvariable('conn'), 'INSERT {lineitem_insert_single_sql}', params=row({questionmarks}))", row)
    count += 1
    if 0 == count % 1024:
      print(f"Inserted: {count}")

  cur.execute("SELECT odbc_commit(getvariable('conn'))")
  return count

def insert_records_batched_direct_params(cur_reader, cur):
  print("Inserting records batched using direct params")
  lineitem_insert_single_sql = read_sql("ora_lineitem_insert_single.sql")
  questionmarks = lineitem_questionmarks()

  cur.execute("SELECT odbc_begin_transaction(getvariable('conn'))")

  count = 0
  part_count = 0
  while True:
    batch = cur_reader.fetchmany(batch_size)
    if len(batch) == 0:
      break
    cur.executemany(f"FROM odbc_query(getvariable('conn'), 'INSERT {lineitem_insert_single_sql}', params=row({questionmarks}))", batch)
    count += len(batch)
    part_count += len(batch)
    if part_count >= 1024:
      print(f"Inserted: {count}")
      part_count = 0

  cur.execute("SELECT odbc_commit(getvariable('conn'))")
  return count

def insert_records_one_by_one_params_handle(cur_reader, cur):
  print("Inserting records one by one using params handle")
  cur.execute("SET VARIABLE params = odbc_create_params()")
  lineitem_insert_single_sql = read_sql("ora_lineitem_insert_single.sql")
  cur.execute(f"PREPARE prep_stmt AS FROM odbc_query(getvariable('conn'), 'INSERT {lineitem_insert_single_sql}', params_handle=getvariable('params'))")
  questionmarks = lineitem_questionmarks()

  cur.execute("SELECT odbc_begin_transaction(getvariable('conn'))")

  count = 0
  while True:
    row = cur_reader.fetchone()
    if row is None:
      break
    cur.execute(f"SELECT odbc_bind_params(getvariable('conn'), getvariable('params'), row({questionmarks}))", row)
    cur.execute("EXECUTE prep_stmt")
    count += 1
    if 0 == count % 1024:
      print(f"Inserted: {count}")

  cur.execute("SELECT odbc_commit(getvariable('conn'))")
  return count

def insert_records_batched_params_handle(cur_reader, cur):
  print("Inserting records batched using params handle")
  lineitem_insert_single_sql = read_sql("ora_lineitem_insert_single.sql")
  questionmarks_single = lineitem_questionmarks()

  sql_arr = ["INSERT ALL"]
  for i in range(batch_size):
    sql_arr.append(lineitem_insert_single_sql)
  sql_arr.append("SELECT 1 FROM DUAL")
  sql_full_batch = "\n".join(sql_arr)
  cur.execute("SET VARIABLE params = odbc_create_params()")
  cur.execute(f"PREPARE prep_stmt AS FROM odbc_query(getvariable('conn'), '{sql_full_batch}', params_handle=getvariable('params'))")

  cur.execute("SELECT odbc_begin_transaction(getvariable('conn'))")

  count = 0
  part_count = 0
  while True:
    batch = cur_reader.fetchmany(batch_size)
    if len(batch) == 0:
      break
    row = []
    qmarks_arr = []
    for i in range(len(batch)):
      row.extend(batch[i])
      qmarks_arr.append(questionmarks_single)
    questionmarks = ",".join(qmarks_arr)

    if len(batch) < batch_size:
      sql_arr = ["INSERT ALL"]
      for i in range(len(batch)):
        sql_arr.append(lineitem_insert_single_sql)
      sql_arr.append("SELECT 1 FROM DUAL")
      sql = "\n".join(sql_arr)
      cur.execute(f"DEALLOCATE prep_stmt")
      cur.execute("SET VARIABLE params = odbc_create_params()")
      cur.execute(f"PREPARE prep_stmt AS FROM odbc_query(getvariable('conn'), '{sql}', params_handle=getvariable('params'))")
    else:
      sql = sql_full_batch

    cur.execute(f"SELECT odbc_bind_params(getvariable('conn'), getvariable('params'), row({questionmarks}))", row)
    cur.execute("EXECUTE prep_stmt")

    count += len(batch)
    part_count += len(batch)
    if part_count >= 1024:
      print(f"Inserted: {count}")
      part_count = 0

  cur.execute("SELECT odbc_commit(getvariable('conn'))")
  return count

def list_inserted(cur):
  cur.execute("FROM odbc_query(getvariable('conn'), 'SELECT count(*) FROM LINEITEM')")
  count = cur.fetchone()[0]
  print(f"Inserted committed: {count}")
  print("One inserted record:")
  cur.execute("FROM odbc_query(getvariable('conn'), 'SELECT * FROM LINEITEM ORDER BY L_ORDERKEY OFFSET 4096 ROWS FETCH NEXT 1 ROWS ONLY')")
  while True:
    row = cur.fetchone()
    if row is None:
      break
    print(row)

if __name__ == "__main__":
  cur = duckdb.connect()
  init_oracle_conn(cur)
  create_lineitem_table(cur)
  cur_reader = open_lineitem_reader()

  start = time.time()

  # Records/second: 403
  # count = insert_records_one_by_one_direct_params(cur_reader, cur)
  # Records/second: 418
  # count = insert_records_batched_direct_params(cur_reader, cur)
  # Records/second: 566
  count = insert_records_one_by_one_params_handle(cur_reader, cur)
  # Records/second: 1703
  # count = insert_records_batched_params_handle(cur_reader, cur)

  elapsed = time.time() - start
  print(f"Elapsed seconds: {elapsed // 1}")
  print(f"Records/second: {count // elapsed}")

  list_inserted(cur)
  cur.execute("DEALLOCATE prep_stmt")
  cur.execute("SELECT odbc_close(getvariable('conn'))")
