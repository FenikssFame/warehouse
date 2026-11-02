#include "util/Format.hpp"

#include <algorithm>
#include <cctype>
#include <climits>
#include <cstdio>
#include <ctime>
#include <cwctype>
#include <iomanip>
#include <sstream>

namespace warehouse {
namespace {

std::wstring trim(const std::wstring& value) {
    const auto first = value.find_first_not_of(L" \t\r\n");
    if (first == std::wstring::npos) {
        return L"";
    }
    const auto last = value.find_last_not_of(L" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::wstring groupThousands(const std::wstring& digits) {
    std::wstring result;
    result.reserve(digits.size() + digits.size() / 3);
    int counter = 0;
    for (auto it = digits.rbegin(); it != digits.rend(); ++it) {
        if (counter != 0 && counter % 3 == 0) {
            result.push_back(L' ');
        }
        result.push_back(*it);
        ++counter;
    }
    std::reverse(result.begin(), result.end());
    return result;
}

// Разбирает неотрицательное десятичное число и возвращает значение,
// умноженное на 10^scale. Например, "12,34" при scale=2 -> 1234.
std::optional<long long> parseScaled(const std::wstring& text, int scale) {
    std::wstring value = trim(text);
    if (value.empty()) {
        return std::nullopt;
    }

    value.erase(std::remove_if(value.begin(), value.end(),
                               [](wchar_t ch) {
                                   return ch == L' ' || ch == L'\u00A0' ||
                                          ch == L'\u20BD' || ch == L'%';
                               }),
                value.end());

    // Пользователь может вставить сумму вместе с обозначением валюты.
    for (const std::wstring suffix :
         {std::wstring(L"BYN"), std::wstring(L"byn"), std::wstring(L"Br"),
          std::wstring(L"br"), std::wstring(L"BR"), std::wstring(L"РУБ."),
          std::wstring(L"руб."), std::wstring(L"РУБ"), std::wstring(L"руб")}) {
        if (value.size() >= suffix.size() &&
            value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0) {
            value.resize(value.size() - suffix.size());
            break;
        }
    }

    bool negative = false;
    if (!value.empty() && (value.front() == L'-' || value.front() == L'+')) {
        negative = value.front() == L'-';
        value.erase(value.begin());
    }
    if (value.empty()) {
        return std::nullopt;
    }

    std::replace(value.begin(), value.end(), L',', L'.');
    if (std::count(value.begin(), value.end(), L'.') > 1) {
        return std::nullopt;
    }

    std::wstring integerPart;
    std::wstring fractionPart;
    const auto dot = value.find(L'.');
    if (dot == std::wstring::npos) {
        integerPart = value;
    } else {
        integerPart = value.substr(0, dot);
        fractionPart = value.substr(dot + 1);
    }
    if (integerPart.empty()) {
        integerPart = L"0";
    }
    if (integerPart.empty() ||
        !std::all_of(integerPart.begin(), integerPart.end(),
                     [](wchar_t ch) { return std::iswdigit(ch) != 0; })) {
        return std::nullopt;
    }
    if (!fractionPart.empty() &&
        !std::all_of(fractionPart.begin(), fractionPart.end(),
                     [](wchar_t ch) { return std::iswdigit(ch) != 0; })) {
        return std::nullopt;
    }

    const int fracLength = static_cast<int>(fractionPart.size());
    bool roundUp = false;
    if (fracLength > scale) {
        roundUp = fracLength > scale && fractionPart[static_cast<std::size_t>(scale)] >= L'5';
        fractionPart.resize(static_cast<std::size_t>(scale));
    }
    fractionPart.append(static_cast<std::size_t>(scale) - fractionPart.size(), L'0');

    long long integerValue = 0;
    try {
        std::size_t used = 0;
        integerValue = std::stoll(integerPart, &used);
        if (used != integerPart.size()) {
            return std::nullopt;
        }
    } catch (...) {
        return std::nullopt;
    }

    long long fractionValue = 0;
    try {
        fractionValue = fractionPart.empty() ? 0 : std::stoll(fractionPart);
    } catch (...) {
        return std::nullopt;
    }

    long long multiplier = 1;
    for (int i = 0; i < scale; ++i) {
        multiplier *= 10;
    }
    if (integerValue > (LLONG_MAX - fractionValue) / multiplier) {
        return std::nullopt;
    }

    long long result = integerValue * multiplier + fractionValue;
    if (roundUp) {
        result += 1;
    }
    if (negative) {
        result = -result;
    }
    return result;
}

std::string formatIsoDate(const std::tm& value) {
    char buffer[16]{};
    std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d", value.tm_year + 1900,
                  value.tm_mon + 1, value.tm_mday);
    return buffer;
}

}  // namespace

std::wstring formatMoney(long long cents, bool withCurrency) {
    const bool negative = cents < 0;
    const unsigned long long absolute =
        negative ? static_cast<unsigned long long>(-(cents + 1)) + 1ULL
                 : static_cast<unsigned long long>(cents);
    const auto rubles = absolute / 100ULL;
    const auto kopecks = absolute % 100ULL;

    std::wstring result = negative ? L"-" : L"";
    result += groupThousands(std::to_wstring(rubles));
    result += L",";
    result += kopecks < 10 ? L"0" : L"";
    result += std::to_wstring(kopecks);
    if (withCurrency) {
        result += L" Br";
    }
    return result;
}

std::wstring formatMoneyShort(long long cents) {
    return formatMoney(cents, false);
}

std::optional<long long> parseMoney(const std::wstring& text) {
    return parseScaled(text, 2);
}

std::wstring formatPercent(int basisPoints) {
    const bool negative = basisPoints < 0;
    const int absolute = negative ? -basisPoints : basisPoints;
    std::wstring result = negative ? L"-" : L"";
    result += std::to_wstring(absolute / 100);
    result += L",";
    result += absolute % 100 < 10 ? L"0" : L"";
    result += std::to_wstring(absolute % 100);
    result += L" %";
    return result;
}

std::optional<int> parsePercentToBasisPoints(const std::wstring& text) {
    const auto value = parseScaled(text, 2);
    if (!value || *value < 0 || *value > 10000) {
        return std::nullopt;
    }
    return static_cast<int>(*value);
}

std::wstring formatInt(long long value) {
    const bool negative = value < 0;
    const unsigned long long absolute =
        negative ? static_cast<unsigned long long>(-(value + 1)) + 1ULL
                 : static_cast<unsigned long long>(value);
    return (negative ? L"-" : L"") + groupThousands(std::to_wstring(absolute));
}

std::optional<int> parseInt(const std::wstring& text) {
    const std::wstring value = trim(text);
    if (value.empty()) {
        return std::nullopt;
    }
    try {
        std::size_t used = 0;
        const long long parsed = std::stoll(value, &used);
        if (used != value.size() || parsed < INT_MIN || parsed > INT_MAX) {
            return std::nullopt;
        }
        return static_cast<int>(parsed);
    } catch (...) {
        return std::nullopt;
    }
}

std::string todayIso() {
    const std::time_t now = std::time(nullptr);
    std::tm local{};
    localtime_s(&local, &now);
    return formatIsoDate(local);
}

std::string firstDayOfMonthIso() {
    const std::time_t now = std::time(nullptr);
    std::tm local{};
    localtime_s(&local, &now);
    local.tm_mday = 1;
    return formatIsoDate(local);
}

std::string formatDateRu(const std::string& isoDate) {
    if (isoDate.size() < 10 || isoDate[4] != '-' || isoDate[7] != '-') {
        return isoDate;
    }
    return isoDate.substr(8, 2) + "." + isoDate.substr(5, 2) + "." +
           isoDate.substr(0, 4);
}

std::string formatDateTimeRu(const std::string& dbValue) {
    if (dbValue.size() >= 16 && dbValue[4] == '-' && dbValue[7] == '-') {
        return formatDateRu(dbValue) + " " + dbValue.substr(11, 5);
    }
    return dbValue;
}

}  // namespace warehouse
