#include "diagnostics.hpp"

#include "scanner_exception.hpp"
#include "widechar.hpp"

#include <string>
#include <vector>

DUCKDB_EXTENSION_EXTERN

namespace odbcscanner {

struct DiagMsg {
	std::string state;
	SQLINTEGER native_error;
	std::string message;

	DiagMsg(std::string state_in, SQLINTEGER native_error_in, std::string message_in)
	    : state(std::move(state_in)), native_error(native_error_in), message(std::move(message_in)) {
	}

	DiagMsg(DiagMsg &other) = delete;
	DiagMsg(DiagMsg &&other) = default;

	DiagMsg &operator=(DiagMsg &other) = delete;
	DiagMsg &operator=(DiagMsg &&other) = default;
};

static DiagMsg ReadDiagInternal(SQLHANDLE handle, SQLSMALLINT handle_type) {
	idx_t state_len = 5;
	std::vector<SQLWCHAR> sqlstate;
	sqlstate.resize(state_len + 1);
	SQLINTEGER native_error;
	std::vector<SQLWCHAR> message_text;
	message_text.resize(4096);
	SQLRETURN ret = SQL_SUCCESS;

	std::string state;
	std::string message;

	for (SQLSMALLINT rec_num = 1; SQL_SUCCEEDED(ret); rec_num++) {
		SQLSMALLINT text_len;
		ret = SQLGetDiagRecW(handle_type, handle, rec_num, sqlstate.data(), &native_error, message_text.data(),
		                     static_cast<SQLSMALLINT>(message_text.size()), &text_len);
		if (SQL_SUCCEEDED(ret)) {
			state = utf16_to_utf8_lenient(sqlstate.data(), state_len);
			message += utf16_to_utf8_lenient(message_text.data(), text_len);
		}
	}

	if (state.length() > 0) {
		return DiagMsg(std::move(state), native_error, std::move(message));
	}

	return DiagMsg("01000", 0, "N/A");
}

std::string ReadDiagnostics(SQLHANDLE handle, SQLSMALLINT handle_type) {
	DiagMsg dm = ReadDiagInternal(handle, handle_type);
	return dm.state + "(" + std::to_string(dm.native_error) + "): " + dm.message;
}

std::string ReadDiagnosticsCode(SQLHANDLE handle, SQLSMALLINT handle_type) {
	DiagMsg dm = ReadDiagInternal(handle, handle_type);
	return dm.state;
}

} // namespace odbcscanner
