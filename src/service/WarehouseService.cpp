#include "service/WarehouseService.hpp"

#include <algorithm>
#include <cctype>
#include <limits>

#include "util/Format.hpp"

namespace warehouse {
namespace {

std::string trim(const std::string& value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "";
    }
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

bool isIsoDate(const std::string& value) {
    if (value.size() != 10 || value[4] != '-' || value[7] != '-') {
        return false;
    }
    for (std::size_t index = 0; index < value.size(); ++index) {
        if (index == 4 || index == 7) {
            continue;
        }
        if (std::isdigit(static_cast<unsigned char>(value[index])) == 0) {
            return false;
        }
    }
    return true;
}

long long calculateTax(Cents revenue, int taxRateBp) {
    if (revenue <= 0 || taxRateBp <= 0) {
        return 0;
    }
    // Цена продажи включает налог, поэтому налог выделяется по формуле
    // revenue * rate / (10000 + rate). Для учебных объёмов достаточно
    // 64-битной арифметики.
    const long long numerator = revenue * static_cast<long long>(taxRateBp);
    const long long denominator = 10000 + taxRateBp;
    return (numerator + denominator / 2) / denominator;
}

Cents multiplyCost(Cents unitCost, int quantity) {
    return unitCost * static_cast<Cents>(quantity);
}

void requirePositive(int value, const std::string& field) {
    if (value <= 0) {
        throw AppError(field + " должно быть больше нуля.");
    }
}

void requireNonNegative(Cents value, const std::string& field) {
    if (value < 0) {
        throw AppError(field + " не может быть отрицательным.");
    }
}

std::string requireText(const std::string& value, const std::string& field) {
    const auto cleaned = trim(value);
    if (cleaned.empty()) {
        throw AppError("Поле «" + field + "» не может быть пустым.");
    }
    return cleaned;
}

std::string translateDatabaseError(const DbError& error,
                                   const std::string& duplicateMessage) {
    if (error.sqlState() == "23505") {
        return duplicateMessage;
    }
    return error.what();
}

}  // namespace

WarehouseService::WarehouseService(Database& database)
    : database_(database), catalog_(database), stock_(database), reports_(database) {}

std::vector<Product> WarehouseService::listProducts() {
    return catalog_.listProducts();
}

std::vector<Category> WarehouseService::listCategories() {
    return catalog_.listCategories();
}

std::vector<Supplier> WarehouseService::listSuppliers() {
    return catalog_.listSuppliers();
}

std::int64_t WarehouseService::createProduct(const ProductInput& input) {
    ProductInput normalized = input;
    normalized.sku = requireText(input.sku, "Артикул");
    normalized.name = requireText(input.name, "Название товара");
    normalized.category = requireText(input.category, "Категория");
    normalized.unit = requireText(input.unit, "Единица измерения");
    if (normalized.unit.size() > 20) {
        throw AppError("Единица измерения слишком длинная.");
    }
    requireNonNegative(normalized.salePriceCents, "Цена продажи");
    if (normalized.taxRateBp < 0 || normalized.taxRateBp > 10000) {
        throw AppError("Ставка налога должна быть от 0 до 100 %.");
    }
    requirePositive(normalized.shelfLifeDays, "Срок хранения");
    if (normalized.minStock < 0) {
        throw AppError("Минимальный остаток не может быть отрицательным.");
    }

    try {
        Transaction transaction(database_);
        const auto id = catalog_.createProduct(normalized);
        transaction.commit();
        return id;
    } catch (const DbError& error) {
        throw AppError(translateDatabaseError(
            error, "Товар с таким артикулом уже существует."));
    }
}

void WarehouseService::receiveSupply(const SupplyInput& input) {
    SupplyInput normalized = input;
    normalized.supplierName = requireText(input.supplierName, "Поставщик");
    normalized.invoiceNo = requireText(input.invoiceNo, "Номер накладной");
    requirePositive(input.quantity, "Количество");
    requireNonNegative(input.unitCostCents, "Закупочная цена");
    if (!isIsoDate(input.expiryDate)) {
        throw AppError("Укажите корректный срок годности.");
    }
    if (input.expiryDate < todayIso()) {
        throw AppError("Срок годности не может быть в прошлом.");
    }
    if (!input.suppliedAt.empty() && !isIsoDate(input.suppliedAt)) {
        throw AppError("Укажите корректную дату поступления.");
    }

    try {
        Transaction transaction(database_);
        const auto supplierId = catalog_.getOrCreateSupplier(normalized.supplierName);
        const auto product = catalog_.getProduct(input.productId);
        (void)product;
        stock_.insertSupply(normalized, supplierId, input.productId);
        transaction.commit();
    } catch (const DbError& error) {
        throw AppError(translateDatabaseError(
            error, "Поставка с такой накладной уже существует у этого поставщика."));
    }
}

SaleResult WarehouseService::sell(const SaleInput& input) {
    requirePositive(input.quantity, "Количество");
    requireNonNegative(input.unitPriceCents, "Цена продажи");

    try {
        const auto product = catalog_.getProduct(input.productId);

        Transaction transaction(database_);
        const auto batches = stock_.lockBatches(input.productId, false);
        int available = 0;
        for (const auto& batch : batches) {
            available += batch.remaining;
        }
        if (available < input.quantity) {
            throw AppError("Недостаточно непросроченного товара. Доступно: " +
                           std::to_string(available) + " " + product.unit + ".");
        }

        int remaining = input.quantity;
        Cents cost = 0;
        for (const auto& batch : batches) {
            if (remaining == 0) {
                break;
            }
            const int taken = std::min(remaining, batch.remaining);
            cost += multiplyCost(batch.unitCostCents, taken);
            stock_.decreaseBatch(batch.id, taken);
            remaining -= taken;
        }

        const Cents revenue = input.unitPriceCents * static_cast<Cents>(input.quantity);
        const Cents tax = calculateTax(revenue, product.taxRateBp);
        const Cents profit = revenue - tax - cost;
        const auto saleId =
            stock_.insertSale(input.productId, input.quantity, input.unitPriceCents,
                              revenue, tax, cost, profit);
        transaction.commit();

        return SaleResult{saleId, input.quantity, revenue, tax, cost, profit};
    } catch (const DbError& error) {
        throw AppError(translateDatabaseError(error, "Не удалось оформить продажу."));
    }
}

WriteOffResult WarehouseService::writeOff(const WriteOffInput& input) {
    requirePositive(input.quantity, "Количество");

    try {
        const auto product = catalog_.getProduct(input.productId);
        const bool expiredOnly = input.reason == WriteOffReason::Expired;

        Transaction transaction(database_);
        const auto batches = stock_.lockBatches(input.productId, expiredOnly);
        int available = 0;
        for (const auto& batch : batches) {
            available += batch.remaining;
        }
        if (available < input.quantity) {
            const std::string what = expiredOnly ? "просроченного товара" : "товара";
            throw AppError("Недостаточно " + what + ". Доступно: " +
                           std::to_string(available) + " " + product.unit + ".");
        }

        int remaining = input.quantity;
        int batchCount = 0;
        Cents cost = 0;
        for (const auto& batch : batches) {
            if (remaining == 0) {
                break;
            }
            const int taken = std::min(remaining, batch.remaining);
            const Cents batchCost = multiplyCost(batch.unitCostCents, taken);
            stock_.insertWriteOff(input.productId, batch.id, taken, input.reason,
                                  batchCost, input.comment);
            stock_.decreaseBatch(batch.id, taken);
            cost += batchCost;
            ++batchCount;
            remaining -= taken;
        }
        transaction.commit();

        return WriteOffResult{batchCount, input.quantity, cost};
    } catch (const DbError& error) {
        throw AppError(translateDatabaseError(error, "Не удалось оформить списание."));
    }
}

std::vector<SupplyRow> WarehouseService::listSupplies(int limit) {
    return stock_.listSupplies(limit);
}

std::vector<SaleRow> WarehouseService::listSales(int limit) {
    return stock_.listSales(limit);
}

std::vector<WriteOffRow> WarehouseService::listWriteOffs(int limit) {
    return stock_.listWriteOffs(limit);
}

ProfitReport WarehouseService::profitReport(const std::string& from,
                                             const std::string& to) {
    if (!isIsoDate(from) || !isIsoDate(to) || from > to) {
        throw AppError("Укажите корректный период отчёта.");
    }
    return reports_.profitReport(from, to);
}

std::vector<ExpiredWriteOffRow> WarehouseService::expiredWriteOffs(
    const std::string& from, const std::string& to) {
    if (!isIsoDate(from) || !isIsoDate(to) || from > to) {
        throw AppError("Укажите корректный период отчёта.");
    }
    return reports_.expiredWriteOffs(from, to);
}

std::vector<ExpiringRow> WarehouseService::expiringSoon(int days) {
    if (days <= 0 || days > 3650) {
        throw AppError("Количество дней должно быть от 1 до 3650.");
    }
    return reports_.expiringSoon(days);
}

}  // namespace warehouse
