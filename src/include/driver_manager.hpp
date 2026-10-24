#pragma once

namespace odbcscanner {

// The extension is not linked with the ODBC driver manager library (unixODBC on Linux
// and macOS, odbc32.dll on Windows), instead the library is loaded dynamically on the
// first use and all ODBC API calls are forwarded to it.
struct DriverManager {
	// Loads the driver manager library if it is not loaded yet, throws ScannerException
	// with the details of the failure if the library cannot be loaded. Must be called
	// before allocating ODBC ENV handle.
	static void Load();
};

} // namespace odbcscanner
