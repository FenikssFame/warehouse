#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace warehouse {

using Cents = std::int64_t;

// Статус остатка, который возвращает представление v_product_stock.
// OK / LOW / SOON / EXPIRED / OUT.
struct Product {
    std::int64_t id = 0;
    std::string sku;
    std::string name;
    std::string category;
    std::string unit;
    Cents salePriceCents = 0;
    int taxRateBp = 0;
    int shelfLifeDays = 0;
    int minStock = 0;
    int stockQuantity = 0;
    Cents stockCostCents = 0;
    std::string nearestExpiry;  // YYYY-MM-DD или пустая строка
    std::string status;         // OK / LOW / SOON / EXPIRED / OUT
};

struct ProductInput {
    std::string sku;
    std::string name;
    std::string category;
    std::string unit = "шт";
    Cents salePriceCents = 0;
    int taxRateBp = 2000;
    int shelfLifeDays = 30;
    int minStock = 0;
};

struct Category {
    std::int64_t id = 0;
    std::string name;
};

struct Supplier {
    std::int64_t id = 0;
    std::string name;
    std::string inn;
    std::string contact;
};

struct SupplyInput {
    std::string supplierName;
    std::string invoiceNo;
    std::int64_t productId = 0;
    int quantity = 0;
    Cents unitCostCents = 0;
    std::string expiryDate;  // YYYY-MM-DD
    std::string suppliedAt;  // YYYY-MM-DD, пустая строка = now()
    std::string note;
};

struct SupplyRow {
    std::int64_t id = 0;
    std::string suppliedAt;
    std::string supplier;
    std::string invoiceNo;
    std::string product;
    int quantity = 0;
    int remaining = 0;
    Cents unitCostCents = 0;
    Cents totalCostCents = 0;
    std::string expiryDate;
};

enum class WriteOffReason {
    Expired,
    Damaged,
    Other,
};

struct WriteOffInput {
    std::int64_t productId = 0;
    int quantity = 0;
    WriteOffReason reason = WriteOffReason::Expired;
    std::string comment;
};

struct WriteOffRow {
    std::int64_t id = 0;
    std::string writtenOffAt;
    std::string product;
    int quantity = 0;
    std::string reason;  // EXPIRED / DAMAGED / OTHER
    Cents costCents = 0;
    std::string batchExpiry;
    std::string comment;
};

struct SaleInput {
    std::int64_t productId = 0;
    int quantity = 0;
    Cents unitPriceCents = 0;
};

struct SaleRow {
    std::int64_t id = 0;
    std::string soldAt;
    std::string product;
    int quantity = 0;
    Cents unitPriceCents = 0;
    Cents revenueCents = 0;
    Cents taxCents = 0;
    Cents costCents = 0;
    Cents profitCents = 0;
};

struct SaleResult {
    std::int64_t saleId = 0;
    int quantity = 0;
    Cents revenueCents = 0;
    Cents taxCents = 0;
    Cents costCents = 0;
    Cents profitCents = 0;
};

struct WriteOffResult {
    int batchCount = 0;
    int quantity = 0;
    Cents costCents = 0;
};

struct StockBatch {
    std::int64_t id = 0;
    int remaining = 0;
    Cents unitCostCents = 0;
    std::string expiryDate;
};

struct ProfitRow {
    std::int64_t productId = 0;
    std::string product;
    int quantity = 0;
    Cents revenueCents = 0;
    Cents taxCents = 0;
    Cents costCents = 0;
    Cents profitCents = 0;
    Cents profitPerUnitCents = 0;
};

struct ProfitTotals {
    int quantity = 0;
    Cents revenueCents = 0;
    Cents taxCents = 0;
    Cents costCents = 0;
    Cents profitCents = 0;
};

struct ProfitReport {
    std::vector<ProfitRow> rows;
    ProfitTotals totals;
};

struct ExpiredWriteOffRow {
    std::int64_t id = 0;
    std::string writtenOffAt;
    std::string product;
    int quantity = 0;
    Cents costCents = 0;
    std::string batchExpiry;
    std::string comment;
};

struct ExpiringRow {
    std::int64_t supplyItemId = 0;
    std::string product;
    std::string supplier;
    std::string batchReceivedAt;
    int quantity = 0;
    std::string expiryDate;
    int daysLeft = 0;
    Cents unitCostCents = 0;
};

}  // namespace warehouse
