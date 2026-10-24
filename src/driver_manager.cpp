#include "driver_manager.hpp"

#include <string>
#include <vector>

#include "odbc_api.hpp"
#include "scanner_exception.hpp"

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif // _WIN32

// Forwarding functions below have the same names as ODBC API functions, they must not
// be visible outside of the extension to not interpose the functions of the driver
// manager itself (or of any other library that is linked with the driver manager).
#if defined(__GNUC__) || defined(__clang__)
#define ODBCSCANNER_HIDDEN __attribute__((visibility("hidden")))
#else // __GNUC__ || __clang__
#define ODBCSCANNER_HIDDEN
#endif // __GNUC__ || __clang__

namespace odbcscanner {

namespace {

#ifdef _WIN32
using LibHandle = HMODULE;
#else  // !_WIN32
using LibHandle = void *;
#endif // _WIN32

struct DriverManagerApi {
	bool loaded = false;
	std::string error;

	decltype(&::SQLAllocHandle) SQLAllocHandle = nullptr;
	decltype(&::SQLBindCol) SQLBindCol = nullptr;
	decltype(&::SQLBindParameter) SQLBindParameter = nullptr;
	decltype(&::SQLCancelHandle) SQLCancelHandle = nullptr;
	decltype(&::SQLColAttributeW) SQLColAttributeW = nullptr;
	decltype(&::SQLDataSourcesW) SQLDataSourcesW = nullptr;
	decltype(&::SQLDescribeParam) SQLDescribeParam = nullptr;
	decltype(&::SQLDisconnect) SQLDisconnect = nullptr;
	decltype(&::SQLDriverConnectW) SQLDriverConnectW = nullptr;
	decltype(&::SQLDriversW) SQLDriversW = nullptr;
	decltype(&::SQLEndTran) SQLEndTran = nullptr;
	decltype(&::SQLExecDirectW) SQLExecDirectW = nullptr;
	decltype(&::SQLExecute) SQLExecute = nullptr;
	decltype(&::SQLFetch) SQLFetch = nullptr;
	decltype(&::SQLFreeHandle) SQLFreeHandle = nullptr;
	decltype(&::SQLFreeStmt) SQLFreeStmt = nullptr;
	decltype(&::SQLGetConnectAttr) SQLGetConnectAttr = nullptr;
	decltype(&::SQLGetData) SQLGetData = nullptr;
	decltype(&::SQLGetDiagRecW) SQLGetDiagRecW = nullptr;
	decltype(&::SQLGetInfo) SQLGetInfo = nullptr;
	decltype(&::SQLGetStmtAttr) SQLGetStmtAttr = nullptr;
	decltype(&::SQLNumParams) SQLNumParams = nullptr;
	decltype(&::SQLNumResultCols) SQLNumResultCols = nullptr;
	decltype(&::SQLPrepareW) SQLPrepareW = nullptr;
	decltype(&::SQLRowCount) SQLRowCount = nullptr;
	decltype(&::SQLSetConnectAttr) SQLSetConnectAttr = nullptr;
	decltype(&::SQLSetConnectAttrW) SQLSetConnectAttrW = nullptr;
	decltype(&::SQLSetDescField) SQLSetDescField = nullptr;
	decltype(&::SQLSetEnvAttr) SQLSetEnvAttr = nullptr;
};

} // namespace

static std::vector<std::string> LibraryNames() {
#if defined(_WIN32)
	return {"odbc32.dll"};
#elif defined(__APPLE__)
	// Default search paths of dlopen do not include the prefixes unixODBC is installed
	// under: Homebrew on Apple silicon and on Intel, and MacPorts.
	return {"libodbc.2.dylib", "/opt/homebrew/opt/unixodbc/lib/libodbc.2.dylib",
	        "/usr/local/opt/unixodbc/lib/libodbc.2.dylib", "/opt/local/lib/libodbc.2.dylib"};
#else
	return {"libodbc.so.2", "libodbc.so"};
#endif
}

static std::string InstallHint() {
#if defined(_WIN32)
	return "please check that ODBC Driver Manager is available in the system";
#elif defined(__APPLE__)
	return "please install unixODBC, for example: 'brew install unixodbc'";
#else
	return "please install unixODBC, for example: 'apt install unixodbc' or 'dnf install unixODBC'";
#endif
}

#ifdef _WIN32

static LibHandle OpenLibrary(const std::string &name, std::string &error) {
	std::wstring wname(name.begin(), name.end());
	HMODULE lib = LoadLibraryExW(wname.c_str(), nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
	if (lib == nullptr) {
		error = "error code: " + std::to_string(GetLastError());
	}
	return lib;
}

static void *FindSymbol(LibHandle lib, const char *name) {
	return reinterpret_cast<void *>(GetProcAddress(lib, name));
}

#else // !_WIN32

static LibHandle OpenLibrary(const std::string &name, std::string &error) {
	void *lib = dlopen(name.c_str(), RTLD_NOW | RTLD_LOCAL);
	if (lib == nullptr) {
		const char *err = dlerror();
		error = err != nullptr ? std::string(err) : "N/A";
	}
	return lib;
}

static void *FindSymbol(LibHandle lib, const char *name) {
	return dlsym(lib, name);
}

#endif // _WIN32

template <typename FUNC>
static bool ResolveFunction(LibHandle lib, const std::string &lib_name, const char *func_name, FUNC &func,
                            std::string &error) {
	func = reinterpret_cast<FUNC>(FindSymbol(lib, func_name));
	if (func == nullptr) {
		error = "ODBC driver manager library loaded from: '" + lib_name + "' does not provide function: '" +
		        std::string(func_name) + "'";
		return false;
	}
	return true;
}

#define ODBCSCANNER_RESOLVE(func)                                                                                      \
	if (!ResolveFunction(lib, lib_name, #func, api.func, api.error)) {                                                 \
		return api;                                                                                                    \
	}

static DriverManagerApi LoadApi() {
	DriverManagerApi api;

	LibHandle lib = nullptr;
	std::string lib_name;
	std::string attempts;
	std::string last_error;
	for (const std::string &name : LibraryNames()) {
		lib = OpenLibrary(name, last_error);
		if (lib != nullptr) {
			lib_name = name;
			break;
		}
		if (attempts.length() > 0) {
			attempts += ", ";
		}
		attempts += "'" + name + "'";
	}

	if (lib == nullptr) {
		api.error = "ODBC driver manager library cannot be loaded, " + InstallHint() + ", attempted: " + attempts +
		            ", last error: " + last_error;
		return api;
	}

	// The library is never unloaded, extensions stay loaded until the end of the process
	ODBCSCANNER_RESOLVE(SQLAllocHandle)
	ODBCSCANNER_RESOLVE(SQLBindCol)
	ODBCSCANNER_RESOLVE(SQLBindParameter)
	ODBCSCANNER_RESOLVE(SQLCancelHandle)
	ODBCSCANNER_RESOLVE(SQLColAttributeW)
	ODBCSCANNER_RESOLVE(SQLDataSourcesW)
	ODBCSCANNER_RESOLVE(SQLDescribeParam)
	ODBCSCANNER_RESOLVE(SQLDisconnect)
	ODBCSCANNER_RESOLVE(SQLDriverConnectW)
	ODBCSCANNER_RESOLVE(SQLDriversW)
	ODBCSCANNER_RESOLVE(SQLEndTran)
	ODBCSCANNER_RESOLVE(SQLExecDirectW)
	ODBCSCANNER_RESOLVE(SQLExecute)
	ODBCSCANNER_RESOLVE(SQLFetch)
	ODBCSCANNER_RESOLVE(SQLFreeHandle)
	ODBCSCANNER_RESOLVE(SQLFreeStmt)
	ODBCSCANNER_RESOLVE(SQLGetConnectAttr)
	ODBCSCANNER_RESOLVE(SQLGetData)
	ODBCSCANNER_RESOLVE(SQLGetDiagRecW)
	ODBCSCANNER_RESOLVE(SQLGetInfo)
	ODBCSCANNER_RESOLVE(SQLGetStmtAttr)
	ODBCSCANNER_RESOLVE(SQLNumParams)
	ODBCSCANNER_RESOLVE(SQLNumResultCols)
	ODBCSCANNER_RESOLVE(SQLPrepareW)
	ODBCSCANNER_RESOLVE(SQLRowCount)
	ODBCSCANNER_RESOLVE(SQLSetConnectAttr)
	ODBCSCANNER_RESOLVE(SQLSetConnectAttrW)
	ODBCSCANNER_RESOLVE(SQLSetDescField)
	ODBCSCANNER_RESOLVE(SQLSetEnvAttr)

	api.loaded = true;
	return api;
}

static const DriverManagerApi &Api() {
	// thread-safe initialization of the function-local static
	static DriverManagerApi api = LoadApi();
	return api;
}

void DriverManager::Load() {
	const DriverManagerApi &api = Api();
	if (!api.loaded) {
		throw ScannerException(api.error);
	}
}

} // namespace odbcscanner

// Forwarding functions must not throw, if the driver manager is not loaded they report
// a failure the same way as the driver manager would do it for an invalid input.

#define ODBCSCANNER_FORWARD(func, ...)                                                                                 \
	const odbcscanner::DriverManagerApi &api = odbcscanner::Api();                                                     \
	if (!api.loaded) {                                                                                                 \
		return SQL_ERROR;                                                                                              \
	}                                                                                                                  \
	return api.func(__VA_ARGS__);

extern "C" {

ODBCSCANNER_HIDDEN SQLRETURN SQL_API SQLAllocHandle(SQLSMALLINT HandleType, SQLHANDLE InputHandle,
                                                    SQLHANDLE *OutputHandle) {
    ODBCSCANNER_FORWARD(SQLAllocHandle, HandleType, InputHandle, OutputHandle)}

ODBCSCANNER_HIDDEN SQLRETURN SQL_API
    SQLBindCol(SQLHSTMT StatementHandle, SQLUSMALLINT ColumnNumber, SQLSMALLINT TargetType, SQLPOINTER TargetValue,
               SQLLEN BufferLength,
               SQLLEN *StrLen_or_Ind) {ODBCSCANNER_FORWARD(SQLBindCol, StatementHandle, ColumnNumber, TargetType,
                                                           TargetValue, BufferLength, StrLen_or_Ind)}

ODBCSCANNER_HIDDEN SQLRETURN SQL_API
    SQLBindParameter(SQLHSTMT hstmt, SQLUSMALLINT ipar, SQLSMALLINT fParamType, SQLSMALLINT fCType,
                     SQLSMALLINT fSqlType, SQLULEN cbColDef, SQLSMALLINT ibScale, SQLPOINTER rgbValue,
                     SQLLEN cbValueMax,
                     SQLLEN *pcbValue) {ODBCSCANNER_FORWARD(SQLBindParameter, hstmt, ipar, fParamType, fCType, fSqlType,
                                                            cbColDef, ibScale, rgbValue, cbValueMax, pcbValue)}

ODBCSCANNER_HIDDEN SQLRETURN SQL_API SQLCancelHandle(SQLSMALLINT HandleType, SQLHANDLE InputHandle) {
    ODBCSCANNER_FORWARD(SQLCancelHandle, HandleType, InputHandle)}

ODBCSCANNER_HIDDEN SQLRETURN SQL_API
    SQLColAttributeW(SQLHSTMT hstmt, SQLUSMALLINT iCol, SQLUSMALLINT iField, SQLPOINTER pCharAttr,
                     SQLSMALLINT cbCharAttrMax, SQLSMALLINT *pcbCharAttr, SQLLEN *pNumAttr) {
        ODBCSCANNER_FORWARD(SQLColAttributeW, hstmt, iCol, iField, pCharAttr, cbCharAttrMax, pcbCharAttr, pNumAttr)}

ODBCSCANNER_HIDDEN SQLRETURN SQL_API
    SQLDataSourcesW(SQLHENV henv, SQLUSMALLINT fDirection, SQLWCHAR *szDSN, SQLSMALLINT cbDSNMax, SQLSMALLINT *pcbDSN,
                    SQLWCHAR *szDescription, SQLSMALLINT cbDescriptionMax, SQLSMALLINT *pcbDescription) {
        ODBCSCANNER_FORWARD(SQLDataSourcesW, henv, fDirection, szDSN, cbDSNMax, pcbDSN, szDescription, cbDescriptionMax,
                            pcbDescription)}

ODBCSCANNER_HIDDEN SQLRETURN SQL_API
    SQLDescribeParam(SQLHSTMT hstmt, SQLUSMALLINT ipar, SQLSMALLINT *pfSqlType, SQLULEN *pcbParamDef,
                     SQLSMALLINT *pibScale, SQLSMALLINT *pfNullable) {
        ODBCSCANNER_FORWARD(SQLDescribeParam, hstmt, ipar, pfSqlType, pcbParamDef, pibScale, pfNullable)}

ODBCSCANNER_HIDDEN SQLRETURN SQL_API
    SQLDisconnect(SQLHDBC ConnectionHandle) {ODBCSCANNER_FORWARD(SQLDisconnect, ConnectionHandle)}

ODBCSCANNER_HIDDEN SQLRETURN SQL_API
    SQLDriverConnectW(SQLHDBC hdbc, SQLHWND hwnd, SQLWCHAR *szConnStrIn, SQLSMALLINT cbConnStrIn,
                      SQLWCHAR *szConnStrOut, SQLSMALLINT cbConnStrOutMax, SQLSMALLINT *pcbConnStrOut,
                      SQLUSMALLINT fDriverCompletion) {ODBCSCANNER_FORWARD(SQLDriverConnectW, hdbc, hwnd, szConnStrIn,
                                                                           cbConnStrIn, szConnStrOut, cbConnStrOutMax,
                                                                           pcbConnStrOut, fDriverCompletion)}

ODBCSCANNER_HIDDEN SQLRETURN SQL_API
    SQLDriversW(SQLHENV henv, SQLUSMALLINT fDirection, SQLWCHAR *szDriverDesc, SQLSMALLINT cbDriverDescMax,
                SQLSMALLINT *pcbDriverDesc, SQLWCHAR *szDriverAttributes, SQLSMALLINT cbDrvrAttrMax,
                SQLSMALLINT *pcbDrvrAttr) {ODBCSCANNER_FORWARD(SQLDriversW, henv, fDirection, szDriverDesc,
                                                               cbDriverDescMax, pcbDriverDesc, szDriverAttributes,
                                                               cbDrvrAttrMax, pcbDrvrAttr)}

ODBCSCANNER_HIDDEN SQLRETURN SQL_API SQLEndTran(SQLSMALLINT HandleType, SQLHANDLE Handle, SQLSMALLINT CompletionType) {
    ODBCSCANNER_FORWARD(SQLEndTran, HandleType, Handle, CompletionType)}

ODBCSCANNER_HIDDEN SQLRETURN SQL_API SQLExecDirectW(SQLHSTMT hstmt, SQLWCHAR *szSqlStr, SQLINTEGER cbSqlStr) {
    ODBCSCANNER_FORWARD(SQLExecDirectW, hstmt, szSqlStr, cbSqlStr)}

ODBCSCANNER_HIDDEN SQLRETURN SQL_API
    SQLExecute(SQLHSTMT StatementHandle) {ODBCSCANNER_FORWARD(SQLExecute, StatementHandle)}

ODBCSCANNER_HIDDEN SQLRETURN SQL_API SQLFetch(SQLHSTMT StatementHandle) {ODBCSCANNER_FORWARD(SQLFetch, StatementHandle)}

ODBCSCANNER_HIDDEN SQLRETURN SQL_API
    SQLFreeHandle(SQLSMALLINT HandleType, SQLHANDLE Handle) {ODBCSCANNER_FORWARD(SQLFreeHandle, HandleType, Handle)}

ODBCSCANNER_HIDDEN SQLRETURN SQL_API SQLFreeStmt(SQLHSTMT StatementHandle, SQLUSMALLINT Option) {
    ODBCSCANNER_FORWARD(SQLFreeStmt, StatementHandle, Option)}

ODBCSCANNER_HIDDEN SQLRETURN SQL_API SQLGetConnectAttr(SQLHDBC ConnectionHandle, SQLINTEGER Attribute, SQLPOINTER Value,
                                                       SQLINTEGER BufferLength, SQLINTEGER *StringLength) {
    ODBCSCANNER_FORWARD(SQLGetConnectAttr, ConnectionHandle, Attribute, Value, BufferLength, StringLength)}

ODBCSCANNER_HIDDEN SQLRETURN SQL_API
    SQLGetData(SQLHSTMT StatementHandle, SQLUSMALLINT ColumnNumber, SQLSMALLINT TargetType, SQLPOINTER TargetValue,
               SQLLEN BufferLength,
               SQLLEN *StrLen_or_Ind) {ODBCSCANNER_FORWARD(SQLGetData, StatementHandle, ColumnNumber, TargetType,
                                                           TargetValue, BufferLength, StrLen_or_Ind)}

ODBCSCANNER_HIDDEN SQLRETURN SQL_API
    SQLGetDiagRecW(SQLSMALLINT fHandleType, SQLHANDLE handle, SQLSMALLINT iRecord, SQLWCHAR *szSqlState,
                   SQLINTEGER *pfNativeError, SQLWCHAR *szErrorMsg, SQLSMALLINT cbErrorMsgMax,
                   SQLSMALLINT *pcbErrorMsg) {ODBCSCANNER_FORWARD(SQLGetDiagRecW, fHandleType, handle, iRecord,
                                                                  szSqlState, pfNativeError, szErrorMsg, cbErrorMsgMax,
                                                                  pcbErrorMsg)}

ODBCSCANNER_HIDDEN SQLRETURN SQL_API SQLGetInfo(SQLHDBC ConnectionHandle, SQLUSMALLINT InfoType, SQLPOINTER InfoValue,
                                                SQLSMALLINT BufferLength, SQLSMALLINT *StringLength) {
    ODBCSCANNER_FORWARD(SQLGetInfo, ConnectionHandle, InfoType, InfoValue, BufferLength, StringLength)}

ODBCSCANNER_HIDDEN SQLRETURN SQL_API SQLGetStmtAttr(SQLHSTMT StatementHandle, SQLINTEGER Attribute, SQLPOINTER Value,
                                                    SQLINTEGER BufferLength, SQLINTEGER *StringLength) {
    ODBCSCANNER_FORWARD(SQLGetStmtAttr, StatementHandle, Attribute, Value, BufferLength, StringLength)}

ODBCSCANNER_HIDDEN SQLRETURN SQL_API
    SQLNumParams(SQLHSTMT hstmt, SQLSMALLINT *pcpar) {ODBCSCANNER_FORWARD(SQLNumParams, hstmt, pcpar)}

ODBCSCANNER_HIDDEN SQLRETURN SQL_API SQLNumResultCols(SQLHSTMT StatementHandle, SQLSMALLINT *ColumnCount) {
    ODBCSCANNER_FORWARD(SQLNumResultCols, StatementHandle, ColumnCount)}

ODBCSCANNER_HIDDEN SQLRETURN SQL_API SQLPrepareW(SQLHSTMT hstmt, SQLWCHAR *szSqlStr, SQLINTEGER cbSqlStr) {
    ODBCSCANNER_FORWARD(SQLPrepareW, hstmt, szSqlStr, cbSqlStr)}

ODBCSCANNER_HIDDEN SQLRETURN SQL_API SQLRowCount(SQLHSTMT StatementHandle, SQLLEN *RowCount) {
    ODBCSCANNER_FORWARD(SQLRowCount, StatementHandle, RowCount)}

ODBCSCANNER_HIDDEN SQLRETURN SQL_API
    SQLSetConnectAttr(SQLHDBC ConnectionHandle, SQLINTEGER Attribute, SQLPOINTER Value, SQLINTEGER StringLength) {
        ODBCSCANNER_FORWARD(SQLSetConnectAttr, ConnectionHandle, Attribute, Value, StringLength)}

ODBCSCANNER_HIDDEN SQLRETURN SQL_API
    SQLSetConnectAttrW(SQLHDBC hdbc, SQLINTEGER fAttribute, SQLPOINTER rgbValue, SQLINTEGER cbValue) {
        ODBCSCANNER_FORWARD(SQLSetConnectAttrW, hdbc, fAttribute, rgbValue, cbValue)}

ODBCSCANNER_HIDDEN SQLRETURN SQL_API
    SQLSetDescField(SQLHDESC DescriptorHandle, SQLSMALLINT RecNumber, SQLSMALLINT FieldIdentifier, SQLPOINTER Value,
                    SQLINTEGER BufferLength) {
        ODBCSCANNER_FORWARD(SQLSetDescField, DescriptorHandle, RecNumber, FieldIdentifier, Value, BufferLength)}

ODBCSCANNER_HIDDEN SQLRETURN SQL_API
    SQLSetEnvAttr(SQLHENV EnvironmentHandle, SQLINTEGER Attribute, SQLPOINTER Value, SQLINTEGER StringLength) {
	ODBCSCANNER_FORWARD(SQLSetEnvAttr, EnvironmentHandle, Attribute, Value, StringLength)
}

} // extern "C"
