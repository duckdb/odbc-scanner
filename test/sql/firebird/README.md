# Firebird Types Coverage Status

This directory contains tests for Firebird database types support in the ODBC Scanner extension.

Reference: [link](https://firebirdsql.org/file/documentation/html/en/refdocs/fblangref50/firebird-50-language-reference.html#fblangref50-datatypes)

## Test Environment

Tests use the official Firebird Docker image: https://github.com/FirebirdSQL/firebird-docker  
Docker Hub: `firebirdsql/firebird:5`

Default connection settings:
- Server: 127.0.0.1
- Port: 3050
- Database: /var/lib/firebird/data/test.fdb
- User: testuser
- Password: testpass

## Firebird Data Types

### Integer Types
- [x] SMALLINT - 16-bit signed integer (-32,768 to 32,767)
- [x] INTEGER - 32-bit signed integer (-2,147,483,648 to 2,147,483,647)
- [x] BIGINT - 64-bit signed integer
- [ ] INT128 - 128-bit signed integer (Firebird 4.0+)

### Fixed-Point Types
- [x] NUMERIC(precision, scale) - Exact numeric with declared precision
- [x] DECIMAL(precision, scale) - Exact numeric (precision >= declared)

### Floating-Point Types
- [x] FLOAT - 32-bit single precision (~7 digits)
- [x] REAL - Synonym for FLOAT
- [x] DOUBLE PRECISION - 64-bit double precision (~15 digits)
- [x] DECFLOAT(16) - 16-digit decimal floating point (Firebird 4.0+)
- [x] DECFLOAT(34) - 34-digit decimal floating point (Firebird 4.0+, default)

### Character Types
- [x] VARCHAR(n) - Variable-length character string (up to 32,765 bytes)
- [x] CHAR(n) - Fixed-length character string (up to 32,767 bytes)
- [x] NCHAR(n) - Fixed-length with ISO8859_1 character set
- [ ] BLOB SUB_TYPE TEXT - Large text objects (up to 4GB)

### Binary Types
- [X] BINARY(n) - Fixed-length binary data (synonym for CHAR CHARACTER SET OCTETS)
- [X] VARBINARY(n) - Variable-length binary data (synonym for VARCHAR CHARACTER SET OCTETS)
- [ ] BLOB - Binary large object (up to 4GB)

### Date/Time Types
- [x] DATE - Date only (0001-01-01 to 9999-12-31)
- [x] TIME [WITHOUT TIME ZONE] - Time only (00:00:00.0000 to 23:59:59.9999)
- [x] TIME WITH TIME ZONE - Time with timezone offset or named zone (Firebird 4.0+)
- [x] TIMESTAMP [WITHOUT TIME ZONE] - Date and time combined
- [x] TIMESTAMP WITH TIME ZONE - Timestamp with timezone (Firebird 4.0+)

### Boolean Type
- [x] BOOLEAN - True/False/Unknown value

