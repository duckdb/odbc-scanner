#include "diagnostics.hpp"

#include "scanner_exception.hpp"
#include "widechar.hpp"

#include <string>
#include <vector>

DUCKDB_EXTENSION_EXTERN

namespace odbcscanner {

std::string ReadDiagnostics(SQLHANDLE handle, SQLSMALLINT handle_type) {
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
		return state + "(" + std::to_string(native_error) + "): " + message;
	}
	return "N/A";
}

} // namespace odbcscanner
