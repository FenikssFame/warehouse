#pragma once

#include <string>
#include <vector>

#include "db/Database.hpp"
#include "models.hpp"

namespace warehouse {

class ReportRepository {
public:
    explicit ReportRepository(Database& database) : database_(database) {}

    ProfitReport profitReport(const std::string& from, const std::string& to);
    std::vector<ExpiredWriteOffRow> expiredWriteOffs(const std::string& from,
                                                     const std::string& to);
    std::vector<ExpiringRow> expiringSoon(int days);

private:
    Database& database_;
};

}  // namespace warehouse
