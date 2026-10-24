# DuckDB ODBC extension

[Extension documentation](https://duckdb.org/docs/current/core_extensions/odbc/overview).

[ODBC functions API reference](https://duckdb.org/docs/current/core_extensions/odbc/functions).

The extension is no longer compatible with older versions of DuckDB. To install the latest published versiobn that is still binary compatible with DuckDB v1.2.0+ use the following command:

```sql
FORCE INSTALL 'http://nightly-extensions.duckdb.org/odbc_scanner/02132ae/v1.2.0/<platform>/odbc_scanner.duckdb_extension.gz'
```

Where the `<platform>` is one of:

 - `linux_amd64`
 - `linux_arm64`
 - `linux_amd64_musl`
 - `linux_arm64_musl`
 - `osx_amd64`
 - `osx_arm64`
 - `windows_amd64`
 - `windows_arm64`

To install the latest available version on DuckDB v2.0+ use:

```
FORCE INSTALL odbc FROM core_nightly;
```

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
