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

using EnvHandlePtr = std::unique_ptr<void, void (*)(SQLHANDLE)>;

inline void EnvHandleDeleter(SQLHANDLE env) {
	SQLFreeHandle(SQL_HANDLE_ENV, env);
}

} // namespace odbcscanner
