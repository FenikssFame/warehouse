#pragma once

#include <stdexcept>
#include <string>
#include <vector>

#include "db/Database.hpp"
#include "models.hpp"
#include "repository/CatalogRepository.hpp"
#include "repository/ReportRepository.hpp"
#include "repository/StockRepository.hpp"

namespace warehouse {

class AppError : public std::runtime_error {
public:
    explicit AppError(const std::string& message) : std::runtime_error(message) {}
};

class WarehouseService {
public:
    explicit WarehouseService(Database& database);

    std::vector<Product> listProducts();
    std::vector<Category> listCategories();
    std::vector<Supplier> listSuppliers();

    std::int64_t createProduct(const ProductInput& input);
    void receiveSupply(const SupplyInput& input);
    SaleResult sell(const SaleInput& input);
    WriteOffResult writeOff(const WriteOffInput& input);

    std::vector<SupplyRow> listSupplies(int limit = 300);
    std::vector<SaleRow> listSales(int limit = 300);
    std::vector<WriteOffRow> listWriteOffs(int limit = 300);

    ProfitReport profitReport(const std::string& from, const std::string& to);
    std::vector<ExpiredWriteOffRow> expiredWriteOffs(const std::string& from,
                                                     const std::string& to);
    std::vector<ExpiringRow> expiringSoon(int days);

private:
    Database& database_;
    CatalogRepository catalog_;
    StockRepository stock_;
    ReportRepository reports_;
};

}  // namespace warehouse
