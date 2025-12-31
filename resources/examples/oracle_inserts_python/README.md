Oracle ODBC inserts examples
----------------------------

Example script that explores multiple methods to insert [TPC-H](https://www.tpc.org/tpch/)
data (`lineitem` table) into Oracle DB over ODBC using `odbc_scanner` extension.

In all examples records are inserted using prepared statements and query parameters - without inlining data into the query.
