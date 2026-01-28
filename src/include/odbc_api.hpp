#pragma once

#include <memory>

extern "C" {

#ifdef _MSC_VER
#define VC_EXTRALEAN
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif // _MSC_VER

#include <sql.h>
#include <sqlext.h>
}

namespace odbcscanner {

struct SqlBit {
	SQLCHAR val = 0;
};

using EnvHandlePtr = std::unique_ptr<void, void (*)(SQLHANDLE)>;

inline void EnvHandleDeleter(SQLHANDLE env) {
	SQLFreeHandle(SQL_HANDLE_ENV, env);
}

using StmtHandlePtr = std::unique_ptr<void, void (*)(HSTMT)>;

inline void StmtHandleDeleter(HSTMT hstmt) {
	SQLFreeStmt(hstmt, SQL_CLOSE);
	SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
}

} // namespace odbcscanner
