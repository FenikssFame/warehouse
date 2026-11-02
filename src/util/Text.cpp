#include "util/Text.hpp"

#include <windows.h>

#include <stdexcept>

namespace warehouse {

std::wstring utf8ToWide(const std::string& value) {
    if (value.empty()) {
        return L"";
    }
    const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                                         static_cast<int>(value.size()), nullptr, 0);
    if (size <= 0) {
        // Некорректный UTF-8 не должен ронять интерфейс: показываем замену.
        const int fallbackSize = MultiByteToWideChar(
            CP_ACP, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
        std::wstring fallback(static_cast<std::size_t>(fallbackSize), L'\0');
        MultiByteToWideChar(CP_ACP, 0, value.data(), static_cast<int>(value.size()),
                            fallback.data(), fallbackSize);
        return fallback;
    }
    std::wstring result(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                        static_cast<int>(value.size()), result.data(), size);
    return result;
}

std::string wideToUtf8(const std::wstring& value) {
    if (value.empty()) {
        return "";
    }
    const int size = WideCharToMultiByte(CP_UTF8, 0, value.data(),
                                         static_cast<int>(value.size()), nullptr, 0,
                                         nullptr, nullptr);
    if (size <= 0) {
        throw std::runtime_error("Не удалось преобразовать строку в UTF-8.");
    }
    std::string result(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
                        result.data(), size, nullptr, nullptr);
    return result;
}

std::wstring errorToWide(const std::exception& error) {
    try {
        return utf8ToWide(error.what());
    } catch (...) {
        return L"Неизвестная ошибка.";
    }
}

}  // namespace warehouse
