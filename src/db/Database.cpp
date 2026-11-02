#include "db/Database.hpp"

#include <cstdlib>
#include <limits>
#include <utility>

namespace warehouse {

Result::Result(PGresult* result) : result_(result) {}

Result::~Result() {
    if (result_ != nullptr) {
        PQclear(result_);
    }
}

Result::Result(Result&& other) noexcept : result_(std::exchange(other.result_, nullptr)) {}

Result& Result::operator=(Result&& other) noexcept {
    if (this != &other) {
        if (result_ != nullptr) {
            PQclear(result_);
        }
        result_ = std::exchange(other.result_, nullptr);
    }
    return *this;
}

int Result::rowCount() const {
    return result_ == nullptr ? 0 : PQntuples(result_);
}

int Result::columnCount() const {
    return result_ == nullptr ? 0 : PQnfields(result_);
}

void Result::requireCell(int row, int column) const {
    if (result_ == nullptr || row < 0 || row >= PQntuples(result_) || column < 0 ||
        column >= PQnfields(result_)) {
        throw DbError("Некорректный доступ к результату запроса.");
    }
}

bool Result::isNull(int row, int column) const {
    requireCell(row, column);
    return PQgetisnull(result_, row, column) != 0;
}

std::string Result::text(int row, int column) const {
    requireCell(row, column);
    if (PQgetisnull(result_, row, column) != 0) {
        return "";
    }
    return PQgetvalue(result_, row, column);
}

std::int64_t Result::int64(int row, int column) const {
    const std::string value = text(row, column);
    try {
        std::size_t used = 0;
        const auto parsed = std::stoll(value, &used);
        if (used != value.size()) {
            throw std::invalid_argument("not a number");
        }
        return parsed;
    } catch (...) {
        throw DbError("В ответе PostgreSQL ожидалось целое число, получено: " + value);
    }
}

int Result::integer(int row, int column) const {
    const auto value = int64(row, column);
    if (value < std::numeric_limits<int>::min() ||
        value > std::numeric_limits<int>::max()) {
        throw DbError("Число не помещается в int.");
    }
    return static_cast<int>(value);
}

bool Result::boolean(int row, int column) const {
    const std::string value = text(row, column);
    return value == "t" || value == "true" || value == "1";
}

Database::~Database() {
    disconnect();
}

void Database::connect(const DbConfig& config) {
    disconnect();

    std::string port = std::to_string(config.port);
    std::string sslMode = config.sslMode.empty() ? "prefer" : config.sslMode;

    const char* keys[] = {"host", "port", "dbname", "user", "password",
                          "sslmode", "connect_timeout", "application_name", nullptr};
    const char* values[] = {config.host.c_str(),
                            port.c_str(),
                            config.database.c_str(),
                            config.user.c_str(),
                            config.password.c_str(),
                            sslMode.c_str(),
                            "7",
                            "warehouse_cpp",
                            nullptr};

    connection_ = PQconnectdbParams(keys, values, 0);
    if (connection_ == nullptr || PQstatus(connection_) != CONNECTION_OK) {
        const std::string message =
            connection_ != nullptr ? PQerrorMessage(connection_)
                                   : "Не удалось создать подключение к PostgreSQL.";
        if (connection_ != nullptr) {
            PQfinish(connection_);
            connection_ = nullptr;
        }
        throw DbError("Не удалось подключиться к PostgreSQL: " + message);
    }

    if (PQsetClientEncoding(connection_, "UTF8") != 0) {
        const std::string message = PQerrorMessage(connection_);
        disconnect();
        throw DbError("Не удалось установить UTF-8: " + message);
    }
}

void Database::disconnect() {
    if (connection_ != nullptr) {
        PQfinish(connection_);
        connection_ = nullptr;
    }
}

bool Database::isConnected() const {
    return connection_ != nullptr && PQstatus(connection_) == CONNECTION_OK;
}

Result Database::exec(const std::string& sql,
                      const std::vector<std::optional<std::string>>& params) {
    if (connection_ == nullptr) {
        throw DbError("Нет подключения к PostgreSQL.");
    }

    std::vector<const char*> values;
    values.reserve(params.size());
    for (const auto& param : params) {
        values.push_back(param ? param->c_str() : nullptr);
    }

    PGresult* raw = PQexecParams(connection_, sql.c_str(),
                                 static_cast<int>(values.size()), nullptr,
                                 values.empty() ? nullptr : values.data(), nullptr,
                                 nullptr, 0);
    if (raw == nullptr) {
        throw DbError("Ошибка выполнения запроса: " + std::string(PQerrorMessage(connection_)));
    }

    const ExecStatusType status = PQresultStatus(raw);
    if (status != PGRES_TUPLES_OK && status != PGRES_COMMAND_OK) {
        const char* state = PQresultErrorField(raw, PG_DIAG_SQLSTATE);
        std::string sqlState = state != nullptr ? state : "";
        std::string message = PQresultErrorMessage(raw);
        PQclear(raw);
        if (message.empty()) {
            message = "Запрос завершился с ошибкой.";
        }
        throw DbError(message, sqlState);
    }

    return Result(raw);
}

void Database::begin() {
    exec("BEGIN");
}

void Database::commit() {
    exec("COMMIT");
}

void Database::rollback() {
    if (connection_ != nullptr) {
        try {
            exec("ROLLBACK");
        } catch (...) {
        }
    }
}

Transaction::Transaction(Database& database) : database_(database) {
    database_.begin();
}

Transaction::~Transaction() {
    if (active_) {
        database_.rollback();
    }
}

void Transaction::commit() {
    database_.commit();
    active_ = false;
}

}  // namespace warehouse
