#pragma once

#include <filesystem>
#include <string>

namespace warehouse {

struct DbConfig {
    std::string host = "localhost";
    int port = 5432;
    std::string database = "warehouse";
    std::string user = "postgres";
    std::string password;
    std::string sslMode = "prefer";

    bool isComplete() const;
};

class ConfigStore {
public:
    static std::filesystem::path path();
    static DbConfig load();
    static void save(const DbConfig& config);
};

}  // namespace warehouse
