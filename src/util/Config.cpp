#include "util/Config.hpp"

#include <windows.h>

#include <fstream>
#include <iterator>
#include <system_error>

namespace warehouse {
namespace {

std::string readValue(const std::string& line, const std::string& key) {
    if (line.rfind(key + "=", 0) != 0) {
        return "";
    }
    return line.substr(key.size() + 1);
}

std::wstring executableDirectory() {
    wchar_t buffer[MAX_PATH * 4]{};
    const DWORD length = GetModuleFileNameW(nullptr, buffer,
                                            static_cast<DWORD>(std::size(buffer)));
    if (length == 0 || length >= std::size(buffer)) {
        return L".";
    }
    return std::filesystem::path(std::wstring(buffer, length)).parent_path().wstring();
}

}  // namespace

bool DbConfig::isComplete() const {
    return !host.empty() && port > 0 && port <= 65535 && !database.empty() &&
           !user.empty();
}

std::filesystem::path ConfigStore::path() {
    return std::filesystem::path(executableDirectory()) / L"warehouse.ini";
}

DbConfig ConfigStore::load() {
    DbConfig config;
    std::ifstream file(path(), std::ios::binary);
    if (!file) {
        return config;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (const auto host = readValue(line, "host"); !host.empty()) {
            config.host = host;
        } else if (const auto port = readValue(line, "port"); !port.empty()) {
            try {
                config.port = std::stoi(port);
            } catch (...) {
            }
        } else if (const auto database = readValue(line, "database"); !database.empty()) {
            config.database = database;
        } else if (const auto user = readValue(line, "user"); !user.empty()) {
            config.user = user;
        } else if (line.rfind("password=", 0) == 0) {
            config.password = line.substr(9);
        } else if (const auto sslMode = readValue(line, "sslmode"); !sslMode.empty()) {
            config.sslMode = sslMode;
        }
    }
    return config;
}

void ConfigStore::save(const DbConfig& config) {
    std::error_code error;
    std::filesystem::create_directories(path().parent_path(), error);

    std::ofstream file(path(), std::ios::binary | std::ios::trunc);
    if (!file) {
        throw std::runtime_error("Не удалось сохранить настройки подключения.");
    }
    file << "host=" << config.host << "\n";
    file << "port=" << config.port << "\n";
    file << "database=" << config.database << "\n";
    file << "user=" << config.user << "\n";
    file << "password=" << config.password << "\n";
    file << "sslmode=" << config.sslMode << "\n";
    if (!file) {
        throw std::runtime_error("Не удалось сохранить настройки подключения.");
    }
}

}  // namespace warehouse
