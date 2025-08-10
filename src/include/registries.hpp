#pragma once

#include "connection.hpp"
#include "params.hpp"

#include <cstdint>
#include <memory>
#include <vector>

namespace odbcscanner {

int64_t AddConnectionToRegistry(std::unique_ptr<OdbcConnection> conn);

std::unique_ptr<OdbcConnection> RemoveConnectionFromRegistry(int64_t conn_id);

int64_t AddParamsToRegistry(std::unique_ptr<std::vector<ScannerParam>> params);

std::unique_ptr<std::vector<ScannerParam>> RemoveParamsFromRegistry(int64_t params_id);

} // namespace odbcscanner
