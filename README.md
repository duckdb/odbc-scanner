# DuckDB ODBC Scanner

ODBC Scanner extension allows to connect to other databases (using their ODBC drivers) and run queries with [odbc_query](#odbc_query) table function.

Outline:

 - [Installation](#installation)
 - [Usage example](#usage-example)
 - [DBMS-specific types support status](#dbms-specific-types-support-status)
 - [Functions](#functions)
 - [Connection string examples](#connection-string-examples)
 - [Query parameters](#query-parameters)
 - [Connections and concurrency](#connections-and-concurrency)
 - [Transactions management](#transactions-management)
 - [Performance](#performance)
 - [Building](#building)

## Installation

`odbc_scanner` is built on DuckDB C API and can be installed on DuckDB version `1.2.0` or any newer version the following way (use the URL with `1.2.0` version in it even if you are running later version of DuckDB):

```sql
INSTALL 'http://nightly-extensions.duckdb.org/v1.2.0/<platform>/odbc_scanner.duckdb_extension.gz';
```

Where the `<platform>` is one of:

 - `linux_amd64`
 - `linux_arm64`
 - `linux_amd64_musl`
 - `osx_amd64`
 - `osx_arm64`
 - `windows_amd64`
 - `windows_arm64`

To update installed extension to the latest version run:

```sql
FORCE INSTALL 'http://nightly-extensions.duckdb.org/v1.2.0/<platform>/odbc_scanner.duckdb_extension.gz';
```

Installed version (commit ID) can be checked using the following query:

```sql
SELECT * FROM duckdb_extensions() WHERE extension_name = 'odbc_scanner';
```

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
 - DB2: [types coverage status](https://github.com/duckdb/odbc-scanner/blob/main/test/sql/db2/README.md)
 - PostgreSQL: basic types covered
 - MySQL/MariaDB: basic types covered
 - ClickHouse: basic types covered
 - Spark: basic types covered
 - Arrow Flight SQL: basic types covered

## Functions 

 - [odbc_begin_transaction](#odbc_begin_transaction)
 - [odbc_bind_params](#odbc_bind_params)
 - [odbc_close](#odbc_close)
 - [odbc_commit](#odbc_commit)
 - [odbc_connect](#odbc_connect)
 - [odbc_create_params](#odbc_create_params)
 - [odbc_list_data_sources](#odbc_list_data_sources)
 - [odbc_list_drivers](#odbc_list_drivers)
 - [odbc_query](#odbc_query)
 - [odbc_rollback](#odbc_rollback)

### odbc_begin_transaction

```sql
odbc_begin_transaction(conn_handle BIGINT) -> VARCHAR
```

Sets the `SQL_ATTR_AUTOCOMMIT` attribute to `SQL_AUTOCOMMIT_OFF` on the specified connection thus effectively starting an implicit transaction. [odbc_commit](#odbc_commit) or [odbc_rollback](#odbc_rollback) must be called on such connection to complete the transaction. The completion starts another implicit transaction on this connection. See [Transactions management](#transactions-management) for details.

#### Parameters:

 - `conn_handle` (`BIGINT`): ODBC connection handle created with [odbc_connect](#odbc_connect)

#### Returns:

Always returns `NULL` (`VARCHAR`).

#### Example:

```sql
SELECT odbc_begin_transaction(getvariable('conn'))
```

### odbc_bind_params

```sql
odbc_bind_params(conn_handle BIGINT, params_handle BIGINT, params STRUCT) -> BIGINT
```

Binds specified parameter values to the specified parameters handle. Only necessary with 2-step parameters binding, see [Query parameters](#query-parameters) for details.

#### Parameters:

 - `conn_handle` (`BIGINT`): ODBC connection handle created with [odbc_connect](#odbc_connect)
 - `params_handle` (`BIGINT`): parameters handle created with [odbc_create_params](#odbc_create_params)
 - `params` (`STRUCT`): parameters values

#### Returns:

Parameters handle (`BIGINT`), the same one that was passed as a second argument.

#### Example:

```sql
SELECT odbc_bind_params(getvariable('conn'), getvariable('params1'), row(42, 'foo'))
```

### odbc_close

```sql
odbc_close(conn_handle BIGINT) -> VARCHAR
```

Closes specified ODBC connection to a remote DBMS. Does not throw errors if the connection is already closed.

#### Parameters:

 - `conn_handle` (`BIGINT`): ODBC connection handle created with [odbc_connect](#odbc_connect)

#### Returns:

Always returns `NULL` (`VARCHAR`).

#### Example:

```sql
SELECT odbc_close(getvariable('conn'))
```

### odbc_commit

```sql
odbc_commit(conn_handle BIGINT) -> VARCHAR
```

Calls `SQLEndTran` with `SQL_COMMIT` argument on the specified connection, completing the current transaction. [odbc_begin_transaction](#odbc_begin_transaction) must be called on this connection before this call for the completion to be effective. See [Transactions management](#transactions-management) for details.

#### Parameters:

 - `conn_handle` (`BIGINT`): ODBC connection handle created with [odbc_connect](#odbc_connect)

#### Returns:

Always returns `NULL` (`VARCHAR`).

#### Example:

```sql
SELECT odbc_commit(getvariable('conn'))
```

### odbc_connect

```sql
odbc_connect(conn_string VARCHAR) -> BIGINT
```

Opens an ODBC connection to a remote DBMS.

#### Parameters:

 - `conn_string` (`VARCHAR`): ODBC connection string, passed to the Driver Manager.

#### Returns:

Connection handle that can be placed into a `VARIABLE`. Connection is not closed automatically, must be closed with [odbc_close](#odbc_close).

#### Example:

```sql
SET VARIABLE conn = odbc_connect('Driver={Oracle Driver};DBQ=//127.0.0.1:1521/XE;UID=system;PWD=tiger;')
```

### odbc_create_params

```sql
odbc_create_params() -> BIGINT
```

Creates a parameters handle. Only necessary with 2-step parameters binding, see [Query parameters](#query-parameters) for details.

#### Parameters:

None.

#### Returns:

Parameters handle (`BIGINT`). When the handle is passed to [odbc_query](#odbc_query) it gets tied to the underlying prepared statement and is closed automatically when the statement is closed.

#### Example:

```sql
SET VARIABLE params1 = odbc_create_params()
```

### odbc_list_data_sources

```sql
odbc_list_data_sources() -> TABLE(name VARCHAR, description VARCHAR, type VARCHAR)
```

Returns the list of ODBC data sources registered in the OS. Uses driver manager call `SQLDataSources`.

#### Parameters:

None.

#### Returns:

A table with the following columns:

 - `name` (`VARCHAR`): data source name
 - `description` (`VARCHAR`): data source description
 - `type` (`VARCHAR`): data source type, `USER` or `SYSTEM`

#### Example:

```sql
SELECT * FROM odbc_list_data_sources()
```

### odbc_list_drivers

```sql
odbc_list_drivers() -> TABLE(description VARCHAR, attributes MAP(VARCHAR, VARCHAR))
```

Returns the list of ODBC drivers registered in the OS. Uses driver manager call `SQLDrivers`.

#### Parameters:

None.

#### Returns:

A table with the following columns:

 - `description` (`VARCHAR`): driver description
 - `attributes` (`MAP(VARCHAR, VARCHAR)`): driver attributes as a `name->value` map

#### Example:

```sql
SELECT * FROM odbc_list_drivers()
```

### odbc_query

```sql
odbc_query(conn_handle BIGINT, query VARCHAR[, <optional named parameters>]) -> TABLE
```
```sql
odbc_query(conn_string VARCHAR, query VARCHAR[, <optional named parameters>]) -> TABLE
```

Runs specified query in a remote DBMS and returns the query results table.

#### Parameters:

 - `conn_handle_or_string` (`BIGINT` or `VARCHAR`), one of:
   - ODBC connection handle created with [odbc_connect](#odbc_connect)
   - ODBC connection string, intended for for one-off queries, in this case new ODBC connection will be opened and will be closed automatically after the query is complete
 - `query` (`VARCHAR`): SQL query, passed to the remote DBMS

Optional named parameters that can be used to pass query parameters:

 - `params` (`STRUCT`): query parameters to pass to remote DBMS
 - `params_handle` (`BIGINT`): parameters handle created with [odbc_create_params](#odbc_create_params). Only used with 2-step parameters binding, see [Query parameters](#query-parameters) for details.

Optional named parameters that can change types mapping:

`odbc_scanner` supports a number of options that can be used to change how the query parameters are passed and how the resulting data is handled. For known DBMSes these options are set automatically. They also can be passed as named parameters to [odbc_query](#odbc_query) function to override the autoconfiguration:

 - `decimal_columns_as_chars` (`BOOLEAN`, default: `false`): read `DECIMAL` values as `VARCHAR`s that are parsed back into `DECIMAL`s before returning them to client
 - `decimal_columns_precision_through_ard` (`BOOLEAN`, default: `false`): when reading a `DECIMAL` specify its `precision` and `scale` through "Application Row Descriptor"
 - `decimal_columns_precision_through_ard_bind` (`BOOLEAN`, default: `false`) allow binding a `DECIMAL` column to a result buffer when its `precision` and `scale` are specified through "Application Row Descriptor"
 - `decimal_params_as_chars` (`BOOLEAN`, default: `false`): pass `DECIMAL` parameters as `VARCHAR`s
 - `integral_params_as_decimals` (`BOOLEAN`, default: `false`): pass (unsigned) `TINYINT`, `SMALLINT`, `INTEGER` and `BIGINT` parameters as `SQL_C_NUMERIC`.
 - `reset_stmt_before_execute` (`BOOLEAN`, default: `false`): reset the prepared statement (using `SQLFreeStmt(h, SQL_CLOSE)`) before executing it
 - `time_params_as_ss_time2` (`BOOLEAN`, default: `false`): pass `TIME` parameters as SQL Server's `TIME2` values
 - `timestamp_columns_as_timestamp_ns` (`BOOLEAN`, default: `false`): read `TIMESTAMP`-like (`TIMESTAMP WITH LOCAL TIME ZONE`, `DATETIME2`, `TIMESTAMP_NTZ` etc) columns with nanosecond precision (with nine fractional digits)
 - `timestamp_columns_with_typename_date_as_date` (`BOOLEAN`, default: `false`): read `TIMESTAMP` columns that have a type name `DATE` as DuckDB `DATE`s
 - `timestamp_max_fraction_precision` (`UTINYINT`, default: `9`): maximum number of fractional digits to use when reading a `TIMESTAMP` column with nanosecond precision
 - `timestamp_params_as_sf_timestamp_ntz` (`BOOLEAN`, default: `false`): pass `TIMESTAMP` parameters as Snowflake's `TIMESTAMP_NTZ`
 - `timestamptz_params_as_ss_timestampoffset` (`BOOLEAN`, default: `false`): pass `TIMESTAMP_TZ` parameters as SQL Server's `DATETIMEOFFSET`
 - `var_len_data_single_part` (`BOOLEAN`, default: `false`): read long `VARCHAR` or `VARBINARY` values as a single read (used when a driver does not support [Retrieving Variable-Length Data in Parts](https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlgetdata-function?view=sql-server-ver17#retrieving-variable-length-data-in-parts))
 - `var_len_params_long_threshold_bytes` (`UINTEGER`, default: `4000`): a length threshold after that `SQL_WVARCHAR` parameters are passed as `SQL_WLONGVARCHAR`

Other optional named parameters:

 - `ignore_exec_failure` (`BOOLEAN`, default: `false`): when a query, that is run in remote DB, can be prepared successfully, but may or may not fail at execution time (for example, because of schema state like table existence), then this flag an be used to not thow an error when query execution fails. Empty result set is returned if query execution fails.
 - `close_connection` (`BOOLEAN`, default: `false`): closes the passed connection after the function call is completed, intended to be used with one-shot invocations of the `odbc_query`, example:

 ```sql
 FROM odbc_query(
    odbc_connect('Driver={Oracle Driver};DBQ=//127.0.0.1:1521/XE;UID=system;PWD=tiger;'),
    'SELECT 42 FROM dual',
    close_connection=TRUE);
 ```

#### Returns:

A table with the query result.

#### Example:

```sql
SELECT * FROM odbc_query(getvariable('conn'), 
  'SELECT CAST(? AS NVARCHAR2(2)) || CAST(? AS VARCHAR2(5)) FROM dual',
  params=row('🦆', 'quack')
)
```

### odbc_rollback

```sql
odbc_rollback(conn_handle BIGINT) -> VARCHAR
```

Calls `SQLEndTran` with `SQL_ROLLBACK` argument on the specified connection, completing the current transaction. [odbc_begin_transaction](#odbc_begin_transaction) must be called on this connection before this call for the completion to be effective. See [Transactions management](#transactions-management) for details.

#### Parameters:

 - `conn_handle` (`BIGINT`): ODBC connection handle created with [odbc_connect](#odbc_connect)

#### Returns:

Always returns `NULL` (`VARCHAR`).

#### Example:

```sql
SELECT odbc_rollback(getvariable('conn'))
```

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

Note: the `Driver` parameter depends on how the driver is registered in `odbcinst.ini` (or Windows registry) and may be different. [odbc_list_drivers](#odbc_list_drivers) can be used to get drivers names.

## Query parameters

When a DuckDB query is run using prepared statement, it is possible to pass input parameters from the client code. `odbc_scanner` allows to forward such input parameters over ODBC API to the queries to remote databases.

`odbc_scanner` supports 2 methods of passing query parameters, using either `params` or `params_handle` named argument to [odbc_query](#odbc_query) function.

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

To avoid this problem is it possible to use 2-step parameter binding with `params_handle` named argument to [odbc_query](#odbc_query):

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

## Transactions management

According to ODBC specification, connections to remote DB are expected to have auto-commit mode enabled by default.

As a general rule, transaction commands `BEGIN TRANSACTION`/`COMMIT`/`ROLLBACK` are not supposed to be send over ODBC as SQL commands. Doing so may or may not be supported by the particular driver. Instead ODBC provides the API to manage transactions.

This API is exposed in `odbc_scanner` in the following functions:

 - [odbc_begin_transaction](#odbc_begin_transaction)
 - [odbc_commit](#odbc_commit)
 - [odbc_rollback](#odbc_rollback)

When [odbc_begin_transaction](#odbc_begin_transaction) is called on the connection, the auto-commit mode on this connection is disabled and an implicit transaction is started. There is currently no support for enabling auto-commit back on such connection.

Afer the transaction is started, [odbc_commit](#odbc_commit) or [odbc_rollback](#odbc_rollback) to complete this transaction. After the completion is performed, new implicit transaction is started in this connection automatically.

## Performance

ODBC is not a high-performance API, `odbc_scanner` uses multiple API calls per-row and performs `UCS-2` to `UTF-8` conversion for every `VARCHAR` value. Besides that, [odbc_query](#odbc_query) processing is strictly single-threaded.

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
