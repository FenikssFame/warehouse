#include "repository/StockRepository.hpp"

#include <optional>
#include <stdexcept>

namespace warehouse {
namespace {

std::string reasonToDb(WriteOffReason reason) {
    switch (reason) {
        case WriteOffReason::Expired:
            return "EXPIRED";
        case WriteOffReason::Damaged:
            return "DAMAGED";
        case WriteOffReason::Other:
            return "OTHER";
    }
    return "OTHER";
}

}  // namespace

std::int64_t StockRepository::insertSupply(const SupplyInput& input,
                                            std::int64_t supplierId,
                                            std::int64_t productId) {
    std::optional<std::string> suppliedAt;
    if (!input.suppliedAt.empty()) {
        suppliedAt = input.suppliedAt;
    }

    const auto supplyResult = database_.exec(
        "INSERT INTO supplies(supplier_id, invoice_no, supplied_at, note)"
        " VALUES($1::bigint, $2, COALESCE($3::date::timestamptz, now()), $4)"
        " RETURNING id",
        {std::to_string(supplierId), input.invoiceNo, suppliedAt, input.note});
    const auto supplyId = supplyResult.int64(0, 0);

    database_.exec(
        "INSERT INTO supply_items("
        "  supply_id, product_id, quantity, remaining_quantity,"
        "  unit_cost_cents, expiry_date"
        ") VALUES($1::bigint, $2::bigint, $3::int, $3::int, $4::bigint, $5::date)",
        {std::to_string(supplyId), std::to_string(productId),
         std::to_string(input.quantity), std::to_string(input.unitCostCents),
         input.expiryDate});
    return supplyId;
}

std::vector<StockBatch> StockRepository::lockBatches(std::int64_t productId,
                                                      bool expiredOnly) {
    const std::string comparison = expiredOnly ? "<" : ">=";
    const auto result = database_.exec(
        "SELECT id, remaining_quantity, unit_cost_cents, "
        "       to_char(expiry_date, 'YYYY-MM-DD') "
        "FROM supply_items "
        "WHERE product_id = $1::bigint "
        "  AND remaining_quantity > 0 "
        "  AND expiry_date " +
            comparison +
            " CURRENT_DATE "
            "ORDER BY expiry_date, id "
            "FOR UPDATE",
        {std::to_string(productId)});

    std::vector<StockBatch> batches;
    batches.reserve(static_cast<std::size_t>(result.rowCount()));
    for (int row = 0; row < result.rowCount(); ++row) {
        batches.push_back(StockBatch{result.int64(row, 0), result.integer(row, 1),
                                     result.int64(row, 2), result.text(row, 3)});
    }
    return batches;
}

void StockRepository::decreaseBatch(std::int64_t batchId, int quantity) {
    const auto result = database_.exec(
        "UPDATE supply_items "
        "SET remaining_quantity = remaining_quantity - $1::int "
        "WHERE id = $2::bigint AND remaining_quantity >= $1::int "
        "RETURNING id",
        {std::to_string(quantity), std::to_string(batchId)});
    if (result.rowCount() == 0) {
        throw std::runtime_error(
            "Остаток партии изменился. Обновите данные и повторите операцию.");
    }
}

std::int64_t StockRepository::insertSale(std::int64_t productId, int quantity,
                                          Cents unitPriceCents, Cents revenueCents,
                                          Cents taxCents, Cents costCents,
                                          Cents profitCents,
                                          const std::string& soldAt) {
    std::optional<std::string> saleDate;
    if (!soldAt.empty()) {
        saleDate = soldAt;
    }

    const auto saleResult = database_.exec(
        "INSERT INTO sales(sold_at) "
        "VALUES(COALESCE($1::date::timestamptz, now())) RETURNING id",
        {saleDate});
    const auto saleId = saleResult.int64(0, 0);

    database_.exec(
        "INSERT INTO sale_items("
        "  sale_id, product_id, quantity, unit_price_cents,"
        "  revenue_cents, tax_cents, cost_cents, profit_cents"
        ") VALUES($1::bigint, $2::bigint, $3::int, $4::bigint, $5::bigint,"
        "         $6::bigint, $7::bigint, $8::bigint)",
        {std::to_string(saleId), std::to_string(productId),
         std::to_string(quantity), std::to_string(unitPriceCents),
         std::to_string(revenueCents), std::to_string(taxCents),
         std::to_string(costCents), std::to_string(profitCents)});
    return saleId;
}

std::int64_t StockRepository::insertWriteOff(std::int64_t productId,
                                              std::int64_t batchId, int quantity,
                                              WriteOffReason reason, Cents costCents,
                                              const std::string& comment) {
    const auto result = database_.exec(
        "INSERT INTO write_offs("
        "  product_id, supply_item_id, quantity, reason, cost_cents, comment"
        ") VALUES($1::bigint, $2::bigint, $3::int, $4, $5::bigint, $6)"
        " RETURNING id",
        {std::to_string(productId), std::to_string(batchId),
         std::to_string(quantity), reasonToDb(reason), std::to_string(costCents),
         comment});
    return result.int64(0, 0);
}

std::vector<SupplyRow> StockRepository::listSupplies(int limit) {
    const auto result = database_.exec(
        "SELECT si.id,"
        "       to_char(s.supplied_at, 'DD.MM.YYYY HH24:MI'),"
        "       sup.name, s.invoice_no, p.name, si.quantity, si.remaining_quantity,"
        "       si.unit_cost_cents, si.quantity::bigint * si.unit_cost_cents,"
        "       to_char(si.expiry_date, 'DD.MM.YYYY') "
        "FROM supply_items si "
        "JOIN supplies s ON s.id = si.supply_id "
        "JOIN suppliers sup ON sup.id = s.supplier_id "
        "JOIN products p ON p.id = si.product_id "
        "ORDER BY s.supplied_at DESC, si.id DESC "
        "LIMIT $1::int",
        {std::to_string(limit)});

    std::vector<SupplyRow> rows;
    rows.reserve(static_cast<std::size_t>(result.rowCount()));
    for (int row = 0; row < result.rowCount(); ++row) {
        rows.push_back(SupplyRow{
            result.int64(row, 0), result.text(row, 1), result.text(row, 2),
            result.text(row, 3), result.text(row, 4), result.integer(row, 5),
            result.integer(row, 6), result.int64(row, 7), result.int64(row, 8),
            result.text(row, 9)});
    }
    return rows;
}

std::vector<SaleRow> StockRepository::listSales(int limit) {
    const auto result = database_.exec(
        "SELECT si.id,"
        "       to_char(s.sold_at, 'DD.MM.YYYY HH24:MI'),"
        "       p.name, si.quantity, si.unit_price_cents, si.revenue_cents,"
        "       si.tax_cents, si.cost_cents, si.profit_cents "
        "FROM sale_items si "
        "JOIN sales s ON s.id = si.sale_id "
        "JOIN products p ON p.id = si.product_id "
        "ORDER BY s.sold_at DESC, si.id DESC "
        "LIMIT $1::int",
        {std::to_string(limit)});

    std::vector<SaleRow> rows;
    rows.reserve(static_cast<std::size_t>(result.rowCount()));
    for (int row = 0; row < result.rowCount(); ++row) {
        rows.push_back(SaleRow{
            result.int64(row, 0), result.text(row, 1), result.text(row, 2),
            result.integer(row, 3), result.int64(row, 4), result.int64(row, 5),
            result.int64(row, 6), result.int64(row, 7), result.int64(row, 8)});
    }
    return rows;
}

std::vector<WriteOffRow> StockRepository::listWriteOffs(int limit) {
    const auto result = database_.exec(
        "SELECT w.id,"
        "       to_char(w.written_off_at, 'DD.MM.YYYY HH24:MI'),"
        "       p.name, w.quantity, w.reason, w.cost_cents,"
        "       to_char(si.expiry_date, 'DD.MM.YYYY'), w.comment "
        "FROM write_offs w "
        "JOIN products p ON p.id = w.product_id "
        "JOIN supply_items si ON si.id = w.supply_item_id "
        "ORDER BY w.written_off_at DESC, w.id DESC "
        "LIMIT $1::int",
        {std::to_string(limit)});

    std::vector<WriteOffRow> rows;
    rows.reserve(static_cast<std::size_t>(result.rowCount()));
    for (int row = 0; row < result.rowCount(); ++row) {
        rows.push_back(WriteOffRow{
            result.int64(row, 0), result.text(row, 1), result.text(row, 2),
            result.integer(row, 3), result.text(row, 4), result.int64(row, 5),
            result.text(row, 6), result.text(row, 7)});
    }
    return rows;
}

}  // namespace warehouse
