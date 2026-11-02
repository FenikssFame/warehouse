#pragma once

#include <optional>
#include <string>

namespace warehouse {

// Денежные значения хранятся в сотых долях белорусского рубля (капейках),
// проценты — в базисных пунктах (2000 = 20,00 %). Это исключает накопление ошибок округления.

std::wstring formatMoney(long long cents, bool withCurrency = true);
std::wstring formatMoneyShort(long long cents);
std::optional<long long> parseMoney(const std::wstring& text);

std::wstring formatPercent(int basisPoints);
std::optional<int> parsePercentToBasisPoints(const std::wstring& text);

std::wstring formatInt(long long value);
std::optional<int> parseInt(const std::wstring& text);

std::string todayIso();
std::string firstDayOfMonthIso();
std::string formatDateRu(const std::string& isoDate);
std::string formatDateTimeRu(const std::string& dbValue);

}  // namespace warehouse
