-- INSTALL 'http://nightly-extensions.duckdb.org/v1.2.0/{platform}/odbc_scanner.duckdb_extension.gz';
LOAD odbc_scanner;

-- Records/second: 10678
-- SET VARIABLE conn = odbc_connect('Driver={Oracle Driver};DBQ=//127.0.0.1:1521/XE;UID=system;PWD=tiger;');
-- Records/second: 1840
-- SET VARIABLE conn = odbc_connect('Driver={MSSQL Driver};Server=tcp:127.0.0.1,1433;UID=sa;PWD=P@ssword2;TrustServerCertificate=Yes;Database=odbcscanner_test_db;');
-- Records/second: 22704
-- SET VARIABLE conn = odbc_connect('Driver={DB2 Driver};HostName=127.0.0.1;Port=50000;Database=testdb;UID=db2inst1;PWD=testpwd;');

CALL odbc_query(getvariable('conn'), 'DROP TABLE "lineitem"', ignore_exec_failure=TRUE);

SELECT * FROM odbc_copy(getvariable('conn'), 
  dest_table='lineitem', 
  create_table=TRUE,
  source_queries=[
    'INSTALL tpch',
    'LOAD tpch',
    'CALL dbgen(sf=0.01)',
    'SELECT * FROM lineitem'
  ]);

SELECT odbc_close(getvariable('conn'))
