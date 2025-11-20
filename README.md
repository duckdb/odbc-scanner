# DuckDB ODBC Scanner

ODBC Scanner extension allows to connect to other DBMSes (using their ODBC drivers) and run queries with `odbc_query()` table function.

Outline:

 - [Installation](#installation)
 - [Usage example](#usage-example)
 - [DBMS-specific types support status](#dbms-specific-types-support-status)
 - [DBMS-specific options](#dbms-specific-options)
 - [Connection string examples](#connection-string-examples)
 - [Query parameters](#query-parameters)
 - [Connections and concurrency](#connections-and-concurrency)
 - [Performance](#performance)
 - [Building](#building)

## Installation

`odbc_scanner` is built on DuckDB C API and can be installed on DuckDB version 1.2.0 or any newer version the following way:

```sql
INSTALL 'http://nightly-extensions.duckdb.org/v1.2.0/<platform>/odbc_scanner.duckdb_extension.gz';

SELECT * FROM duckdb_extensions() WHERE extension_name = 'odbc_scanner';
```

Where the platform is one of:

 - `linux_amd64`
 - `linux_arm64`
 - `linux_amd64_musl`
 - `osx_amd64`
 - `osx_arm64`
 - `windows_amd64`

## Usage example

```sql
-- load extension
LOAD odbc_scanner;

-- open ODBC connection to a remote DB
SET VARIABLE conn = odbc_connect('Driver={Oracle Driver};DBQ=//127.0.0.1:1521/XE;UID=scott;PWD=tiger;');

-- simple query
SELECT * FROM odbc_query(getvariable('conn'), 'SELECT SYSTIMESTAMP FROM dual');

-- query with parameters
SELECT * FROM odbc_query(getvariable('conn'), 
  'SELECT CAST(? AS NVARCHAR2(2)) || CAST(? AS VARCHAR2(5)) FROM dual',
  params=row('🦆', 'quack')
);

-- close connection
SELECT odbc_close(getvariable('conn'));
```

## DBMS-specific types support status

 - Oracle: [types coverage status](https://github.com/duckdb/odbc-scanner/tree/main/test/sql/oracle/README.md)
 - SQL Server: [types coverage status](https://github.com/duckdb/odbc-scanner/blob/main/test/sql/mssql/README.md)
 - Snowflake: [types coverage status](https://github.com/duckdb/odbc-scanner/blob/main/test/sql/snowflake/README.md)
 - DB2: basic types covered
 - PostgreSQL: basic types covered
 - MySQL/MariaDB: basic types covered
 - ClickHouse: basic types covered
 - Spark: basic types covered
 - Arrow Flight SQL: basic types covered

## DBMS-specific options

`odbc_scanner` supports a number of options that can be used to change how the query parameters are passed and how the resulting data is handled. For known DBMSes these options are set automatically. They also can be passed as named parameters to `odbc_query()` function to override the autoconfiguration:

 - `decimal_columns_as_chars` (`BOOLEAN`, default: `false`): read `DECIMAL` values as `VARCHAR`s that are parsed back into `DECIMAL`s before returning them to client
 - `decimal_columns_precision_through_ard` (`BOOLEAN`, default: `false`): when reading a `DECIMAL` specify its `precision` and `scale` through "Application Row Descriptor"
 - `decimal_params_as_chars` (`BOOLEAN`, default: `false`): pass `DECIMAL` parameters as `VARCHAR`s
 - `reset_stmt_before_execute` (`BOOLEAN`, default: `false`): reset the prepared statement (using `SQLFreeStmt(h, SQL_CLOSE)`) before executing it
 - `time_params_as_ss_time2` (`BOOLEAN`, default: `false`): pass `TIME` parameters as SQL Server's `TIME2` values
 - `timestamp_columns_as_timestamp_ns` (`BOOLEAN`, default: `false`): read `TIMESTAMP`-like (`TIMESTAMP WITH LOCAL TIME ZONE`, `DATETIME2`, `TIMESTAMP_NTZ` etc) columns with nanosecond precision (with nine fractional digits)
 - `timestamp_columns_with_typename_date_as_date` (`BOOLEAN`, default: `false`): read `TIMESTAMP` columns that have a type name `DATE` as DuckDB `DATE`s
 - `timestamp_max_fraction_precision` (`UTINYINT`, default: `9`): maximum number of fractional digits to use when reading a `TIMESTAMP` column with nanosecond precision
 - `timestamp_params_as_sf_timestamp_ntz` (`BOOLEAN`, default: `false`): pass `TIMESTAMP` parameters as Snowflake's `TIMESTAMP_NTZ`
 - `timestamptz_params_as_ss_timestampoffset` (`BOOLEAN`, default: `false`): pass `TIMESTAMP_TZ` parameters as SQL Server's `DATETIMEOFFSET`
 - `var_len_data_single_part` (`BOOLEAN`, default: `false`): read long `VARCHAR` or `VARBINARY` values as a single read (used when a driver does not support [Retrieving Variable-Length Data in Parts](https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlgetdata-function?view=sql-server-ver17#retrieving-variable-length-data-in-parts))
 - `var_len_params_long_threshold_bytes` (`UINTEGER`, default: `4000`): a lenght threshold after that `SQL_WVARCHAR` parameters are passed as `SQL_WLONGVARCHAR`

## Connection string examples

Oracle: 
 
 ```
 Driver={Oracle Driver};DBQ=//127.0.0.1:1521/XE;UID=scott;PWD=tiger;
 ```

SQL Server: 
 
 ```
 Driver={MSSQL Driver};Server=tcp:127.0.0.1,1433;UID=sa;PWD=pwd;TrustServerCertificate=Yes;Database=test_db;
 ```

Snowflake: 

```
Driver={Snowflake Driver};Server=foobar-ab12345.snowflakecomputing.com;Database=SNOWFLAKE_SAMPLE_DATA;UID=username;PWD=pwd;
```

DB2: 

```
Driver={DB2 Driver};HostName=127.0.0.1;Port=50000;Database=testdb;UID=db2inst1;PWD=pwd;
```

PostgreSQL: 

```
Driver={PostgreSQL Driver};Server=127.0.0.1;Port=5432;Username=postgres;Password=postgres;Database=test_db;
```

MySQL/MariaDB: 

```
Driver={MariaDB Driver};SERVER=127.0.0.1;PORT=3306;USER=root;PASSWORD=root;DATABASE=test_db;
```

ClickHouse: 

```
Driver={ClickHouse Driver};Server=127.0.0.1;Port=8123;
```

Spark: 

```
Driver={Spark Driver};Host=127.0.0.1;Port=10000;
```

Arrow Flight SQL (Dremio ODBC + GizmoSQL): 

```
Driver={FlightSQL Driver};Host=127.0.0.1;Port=31337;UID=gizmosql_username;PWD=gizmosql_password;useEncryption=true;
```

Note: the `Driver` parameter depends on how the driver is registered in `odbcinst.ini` (or Windows registry) and may be different.

## Query parameters

When a DuckDB query is run using prepared statement, it is possible to pass the input parameters from the client code. `odbc_scanner` allows to forward such input parameters over ODBC API to the queries to remote databases.

`odbc_scanner` supports 2 methods of passing query parameters, using either `params` or `params_handle` named argument to `odbc_query()` function.

`params` argument takes a `STRUCT` value as an input. Struct field names are ignored, so the `row()` function can be used to create a `STRUCT` value inline:

```sql
SELECT * FROM odbc_query(
  getvariable('conn'),
  '
    SELECT CAST(? AS VARCHAR2(3)) || CAST(? AS VARCHAR2(3)) FROM dual
  ', 
  params=row(?, ?))
```

If we prepare this query with `duckdb_prepare()`, bind `foo` and `bar` `VARCHAR` values to it with `duckdb_bind_value()` and
execute it with `duckdb_execute_prepared()` - the input parameters `foo` and `bar` will be forwarded to the ODBC query in the remote DB.

The problem with this approach, is that DuckDB is unable to resolve parameter types (specified in the outer query) before `duckdb_execute_prepared()` is called - such types may be different in subsequent invocations of `duckdb_execute_prepared()` and there is no way to specify these types explicitly.

This will result in re-preparing the inner query in remote DB every time `duckdb_execute_prepared()` is called.

To avoid this problem is it possible to use 2-step parameter binding with `params_handle` named argument to `odbc_query()`:

```sql
-- create parameters handle
SET VARIABLE params = odbc_create_params();

-- when 'duckdb_prepare()' is called, the inner query will be prepared in the remote DB
SELECT * FROM odbc_query(
  getvariable('conn'),
  '
    SELECT CAST(? AS VARCHAR2(3)) || CAST(? AS VARCHAR2(3)) FROM dual
  ', 
  params_handle=getvariable('params'));

-- now we can repeatedly bind new parameters to the handle using 'odbc_bind_params()'
-- and call 'duckdb_execute_prepared()' to run the prepared query with
-- these new parameters in remote DB
SELECT odbc_bind_params(getvariable('conn'), getvariable('params'), row(?, ?));
```

Parameter handle is tied to the prepared statement and will be freed when the statement is destroyed.

## Connections and concurrency

DuckDB uses a multi-threaded execution engine to run parts of the query in parallel. ODBC drivers may or may not support
using the same connection from different threads concurrently. To prevent possible concurrency problems `odbc_scanner` does not
allow to use the same connection from multiple threads. For example, the following query:

```sql
SELECT * FROM odbc_query(getvariable('conn'), 'SELECT ''foo'' col1 FROM dual')
UNION ALL
SELECT * FROM odbc_query(getvariable('conn'), 'SELECT ''bar'' col1 FROM dual')
```

will fail with:

```
Invalid Input Error:
'odbc_query' error: ODBC connection not found on global init, id: 139760181976192
```

This can be avoided by using multiple ODBC connections:

```sql
SELECT * FROM odbc_query(getvariable('conn1'), 'SELECT ''foo'' col1 FROM dual')
UNION ALL
SELECT * FROM odbc_query(getvariable('conn2'), 'SELECT ''bar'' col1 FROM dual')
```

Or by disabling mutli-threaded execution setting `threads` DuckDB option to `1`.

## Performance

ODBC is not a high-performance API, `odbc_scanner` uses multiple API calls per-row and performs `UCS-2` to `UTF-8` conversion for every `VARCHAR` value. Besides that, `odbc_query()` processing is strictly single-threaded.

When submitting issues related only to performance please check the performance in comparable scenarios, for example with [pyodbc](https://pypi.org/project/pyodbc/).

## Building

Dependencies: 

- Python3
- Python3-venv
- GNU Make
- CMake
- unixODBC

Building is a two-step process. Firstly run:

```shell
make configure
```
This will ensure a Python venv is set up with DuckDB and DuckDB's test runner installed. Additionally, depending on configuration,
DuckDB will be used to determine the correct platform for which you are compiling.

Then, to build the extension run:
```shell
make debug
```
This produces a shared library in `target/debug/<shared_lib_name>`. After this step, 
a script is run to transform the shared library into a loadable extension by appending a binary footer. The resulting extension is written
to the `build/debug` directory.

To create optimized release binaries, simply run `make release` instead.
