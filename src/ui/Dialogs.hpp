#pragma once

#include <windows.h>

#include <string>

#include "service/WarehouseService.hpp"
#include "util/Config.hpp"

namespace warehouse {

bool showConnectionDialog(HWND parent, DbConfig& config, const std::string& initialError);
bool showProductDialog(HWND parent, WarehouseService& service);
bool showSupplyDialog(HWND parent, WarehouseService& service);
bool showSaleDialog(HWND parent, WarehouseService& service);
bool showWriteOffDialog(HWND parent, WarehouseService& service);

void showError(HWND parent, const std::exception& error);
void showInfo(HWND parent, const std::wstring& message);

}  // namespace warehouse
