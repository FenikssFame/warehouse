#pragma once

#include <libpq-fe.h>

#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "util/Config.hpp"

namespace warehouse {

class DbError : public std::runtime_error {
public:
    explicit DbError(const std::string& message, std::string sqlState = {})
        : std::runtime_error(message), sqlState_(std::move(sqlState)) {}

    const std::string& sqlState() const noexcept { return sqlState_; }

private:
    std::string sqlState_;
};

class Result {
public:
    Result() = default;
    explicit Result(PGresult* result);
    ~Result();

    Result(const Result&) = delete;
    Result& operator=(const Result&) = delete;
    Result(Result&& other) noexcept;
    Result& operator=(Result&& other) noexcept;

    int rowCount() const;
    int columnCount() const;
    bool isNull(int row, int column) const;
    std::string text(int row, int column) const;
    std::int64_t int64(int row, int column) const;
    int integer(int row, int column) const;
    bool boolean(int row, int column) const;

private:
    PGresult* result_ = nullptr;

    void requireCell(int row, int column) const;
};

class Database {
public:
    Database() = default;
    ~Database();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    void connect(const DbConfig& config);
    void disconnect();
    bool isConnected() const;

    Result exec(const std::string& sql,
                const std::vector<std::optional<std::string>>& params = {});

    void begin();
    void commit();
    void rollback();

private:
    PGconn* connection_ = nullptr;
};

class Transaction {
public:
    explicit Transaction(Database& database);
    ~Transaction();

    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;

    void commit();

private:
    Database& database_;
    bool active_ = true;
};

}  // namespace warehouse
