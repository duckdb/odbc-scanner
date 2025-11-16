#pragma once

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