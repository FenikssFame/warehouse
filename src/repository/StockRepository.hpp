#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "db/Database.hpp"
#include "models.hpp"

namespace warehouse {

class StockRepository {
public:
    explicit StockRepository(Database& database) : database_(database) {}

    std::int64_t insertSupply(const SupplyInput& input, std::int64_t supplierId,
                              std::int64_t productId);
    std::vector<StockBatch> lockBatches(std::int64_t productId, bool expiredOnly);
    void decreaseBatch(std::int64_t batchId, int quantity);

    std::int64_t insertSale(std::int64_t productId, int quantity,
                            Cents unitPriceCents, Cents revenueCents,
                            Cents taxCents, Cents costCents, Cents profitCents,
                            const std::string& soldAt = "");
    std::int64_t insertWriteOff(std::int64_t productId, std::int64_t batchId,
                                int quantity, WriteOffReason reason, Cents costCents,
                                const std::string& comment);

    std::vector<SupplyRow> listSupplies(int limit = 300);
    std::vector<SaleRow> listSales(int limit = 300);
    std::vector<WriteOffRow> listWriteOffs(int limit = 300);

private:
    Database& database_;
};

}  // namespace warehouse
