#include <chrono>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <string>

#include "db/Database.hpp"
#include "service/WarehouseService.hpp"
#include "util/Format.hpp"

namespace {

std::string addDaysIso(int days) {
    const std::time_t now = std::time(nullptr);
    std::tm local{};
    localtime_s(&local, &now);
    local.tm_mday += days;
    const std::time_t shifted = std::mktime(&local);
    std::tm result{};
    localtime_s(&result, &shifted);
    char buffer[16]{};
    std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d", result.tm_year + 1900,
                  result.tm_mon + 1, result.tm_mday);
    return buffer;
}

std::string envOr(const char* name, const std::string& fallback) {
    const char* value = std::getenv(name);
    return value != nullptr && *value != '\0' ? value : fallback;
}

warehouse::Product findProduct(const std::vector<warehouse::Product>& products,
                               std::int64_t id) {
    for (const auto& product : products) {
        if (product.id == id) {
            return product;
        }
    }
    throw std::runtime_error("Созданный товар не найден.");
}

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

}  // namespace

int main() {
    try {
        require(warehouse::formatMoney(12345) == L"123,45 Br",
                "Неверный формат белорусского рубля.");
        require(warehouse::parseMoney(L"123,45 Br") == 12345,
                "Не разбирается сумма с обозначением Br.");
        require(warehouse::parseMoney(L"123,45 BYN") == 12345,
                "Не разбирается сумма с обозначением BYN.");
        warehouse::DbConfig config;
        config.host = envOr("WAREHOUSE_DB_HOST", "127.0.0.1");
        config.port = std::stoi(envOr("WAREHOUSE_DB_PORT", "55434"));
        config.database = envOr("WAREHOUSE_DB_NAME", "warehouse");
        config.user = envOr("WAREHOUSE_DB_USER", "postgres");
        config.password = envOr("WAREHOUSE_DB_PASSWORD", "");
        config.sslMode = "prefer";

        warehouse::Database database;
        database.connect(config);
        warehouse::WarehouseService service(database);

        const auto productsBefore = service.listProducts();
        require(productsBefore.size() >= 6, "Ожидались демонстрационные товары.");

        const auto suffix = std::to_string(
            std::chrono::system_clock::now().time_since_epoch().count());

        warehouse::ProductInput product;
        product.sku = "TEST-" + suffix;
        product.name = "Тестовый товар";
        product.category = "Тестовая категория";
        product.unit = "шт";
        product.salePriceCents = 20000;
        product.taxRateBp = 2000;
        product.shelfLifeDays = 30;
        product.minStock = 0;
        const auto productId = service.createProduct(product);

        warehouse::SupplyInput supply;
        supply.supplierName = "Тестовый поставщик";
        supply.invoiceNo = "SMOKE-" + suffix;
        supply.productId = productId;
        supply.quantity = 5;
        supply.unitCostCents = 10000;
        supply.expiryDate = addDaysIso(10);
        supply.suppliedAt = warehouse::todayIso();
        supply.note = "smoke test";
        service.receiveSupply(supply);

        auto current = findProduct(service.listProducts(), productId);
        require(current.stockQuantity == 5, "Остаток после поступления должен быть 5.");

        warehouse::SaleInput sale;
        sale.productId = productId;
        sale.quantity = 2;
        sale.unitPriceCents = 20000;
        const auto saleResult = service.sell(sale);
        require(saleResult.revenueCents == 40000, "Неверная выручка.");
        require(saleResult.taxCents == 6667, "Неверный налог.");
        require(saleResult.costCents == 20000, "Неверная себестоимость.");
        require(saleResult.profitCents == 13333, "Неверная прибыль.");

        current = findProduct(service.listProducts(), productId);
        require(current.stockQuantity == 3, "Остаток после продажи должен быть 3.");

        warehouse::WriteOffInput damaged;
        damaged.productId = productId;
        damaged.quantity = 1;
        damaged.reason = warehouse::WriteOffReason::Damaged;
        damaged.comment = "smoke test";
        const auto writeOffResult = service.writeOff(damaged);
        require(writeOffResult.costCents == 10000, "Неверная стоимость списания.");

        current = findProduct(service.listProducts(), productId);
        require(current.stockQuantity == 2, "Остаток после списания должен быть 2.");

        const auto report = service.profitReport(warehouse::firstDayOfMonthIso(),
                                                 warehouse::todayIso());
        bool found = false;
        for (const auto& row : report.rows) {
            if (row.productId == productId) {
                found = true;
                require(row.quantity == 2, "В отчёте неверное количество.");
                require(row.profitCents == 13333, "В отчёте неверная прибыль.");
            }
        }
        require(found, "Тестовый товар отсутствует в отчёте о прибыли.");

        const auto expired =
            service.expiredWriteOffs(warehouse::firstDayOfMonthIso(),
                                     warehouse::todayIso());
        require(!expired.empty(), "Ожидалось списание по истечении срока.");

        const auto expiring = service.expiringSoon(10);
        require(!expiring.empty(), "Ожидались товары с близким сроком годности.");

        std::cout << "SMOKE OK: products=" << productsBefore.size()
                  << " sale=" << saleResult.saleId
                  << " profit=" << saleResult.profitCents
                  << " stock=" << current.stockQuantity
                  << " reportRows=" << report.rows.size()
                  << " expired=" << expired.size()
                  << " expiring=" << expiring.size() << "\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "SMOKE FAILED: " << error.what() << "\n";
        return 1;
    }
}
