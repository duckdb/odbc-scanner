-- INSTALL 'http://nightly-extensions.duckdb.org/v1.2.0/{platform}/odbc_scanner.duckdb_extension.gz';
LOAD odbc_scanner;

SET VARIABLE conn = odbc_connect('Driver={Oracle Driver};DBQ=//127.0.0.1:1521/XE;UID=system;PWD=tiger;');

CALL odbc_query(getvariable('conn'), 'DROP TABLE "lineitem"', ignore_exec_failure=TRUE);

-- Records/second: 10192
SELECT * FROM odbc_copy_from(getvariable('conn'), 'lineitem', 
  create_table=TRUE,
  source_queries=[
    'LOAD tpch',
    'CALL dbgen(sf=0.01)',
    'SELECT * FROM lineitem'
  ]);

SELECT odbc_close(getvariable('conn'))
