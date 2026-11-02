#pragma once

#include <string>

namespace warehouse {

// Все строки предметной области — UTF-8. Преобразование выполняется только
// на границе с Win32 API.
std::wstring utf8ToWide(const std::string& value);
std::string wideToUtf8(const std::wstring& value);
std::wstring errorToWide(const std::exception& error);

}  // namespace warehouse
