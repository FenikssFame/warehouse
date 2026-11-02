#include "repository/ReportRepository.hpp"

namespace warehouse {

ProfitReport ReportRepository::profitReport(const std::string& from,
                                             const std::string& to) {
    const auto result = database_.exec(
        "SELECT p.id, p.name,"
        "       COALESCE(SUM(si.quantity), 0)::int,"
        "       COALESCE(SUM(si.revenue_cents), 0)::bigint,"
        "       COALESCE(SUM(si.tax_cents), 0)::bigint,"
        "       COALESCE(SUM(si.cost_cents), 0)::bigint,"
        "       COALESCE(SUM(si.profit_cents), 0)::bigint "
        "FROM sale_items si "
        "JOIN sales s ON s.id = si.sale_id "
        "JOIN products p ON p.id = si.product_id "
        "WHERE s.sold_at >= $1::date "
        "  AND s.sold_at < ($2::date + 1) "
        "GROUP BY p.id, p.name "
        "ORDER BY SUM(si.profit_cents) DESC, p.name",
        {from, to});

    ProfitReport report;
    report.rows.reserve(static_cast<std::size_t>(result.rowCount()));
    for (int row = 0; row < result.rowCount(); ++row) {
        ProfitRow item;
        item.productId = result.int64(row, 0);
        item.product = result.text(row, 1);
        item.quantity = result.integer(row, 2);
        item.revenueCents = result.int64(row, 3);
        item.taxCents = result.int64(row, 4);
        item.costCents = result.int64(row, 5);
        item.profitCents = result.int64(row, 6);
        item.profitPerUnitCents =
            item.quantity > 0 ? item.profitCents / item.quantity : 0;
        report.totals.quantity += item.quantity;
        report.totals.revenueCents += item.revenueCents;
        report.totals.taxCents += item.taxCents;
        report.totals.costCents += item.costCents;
        report.totals.profitCents += item.profitCents;
        report.rows.push_back(item);
    }
    return report;
}

std::vector<ExpiredWriteOffRow> ReportRepository::expiredWriteOffs(
    const std::string& from, const std::string& to) {
    const auto result = database_.exec(
        "SELECT w.id,"
        "       to_char(w.written_off_at, 'DD.MM.YYYY HH24:MI'),"
        "       p.name, w.quantity, w.cost_cents,"
        "       to_char(si.expiry_date, 'DD.MM.YYYY'), w.comment "
        "FROM write_offs w "
        "JOIN products p ON p.id = w.product_id "
        "JOIN supply_items si ON si.id = w.supply_item_id "
        "WHERE w.reason = 'EXPIRED' "
        "  AND w.written_off_at >= $1::date "
        "  AND w.written_off_at < ($2::date + 1) "
        "ORDER BY w.written_off_at DESC, w.id DESC",
        {from, to});

    std::vector<ExpiredWriteOffRow> rows;
    rows.reserve(static_cast<std::size_t>(result.rowCount()));
    for (int row = 0; row < result.rowCount(); ++row) {
        rows.push_back(ExpiredWriteOffRow{
            result.int64(row, 0), result.text(row, 1), result.text(row, 2),
            result.integer(row, 3), result.int64(row, 4), result.text(row, 5),
            result.text(row, 6)});
    }
    return rows;
}

std::vector<ExpiringRow> ReportRepository::expiringSoon(int days) {
    const auto result = database_.exec(
        "SELECT si.id, p.name, sup.name,"
        "       to_char(s.supplied_at, 'DD.MM.YYYY'),"
        "       si.remaining_quantity,"
        "       to_char(si.expiry_date, 'YYYY-MM-DD'),"
        "       (si.expiry_date - CURRENT_DATE)::int,"
        "       si.unit_cost_cents "
        "FROM supply_items si "
        "JOIN supplies s ON s.id = si.supply_id "
        "JOIN suppliers sup ON sup.id = s.supplier_id "
        "JOIN products p ON p.id = si.product_id "
        "WHERE si.remaining_quantity > 0 "
        "  AND si.expiry_date >= CURRENT_DATE "
        "  AND si.expiry_date <= CURRENT_DATE + $1::int "
        "ORDER BY si.expiry_date, p.name",
        {std::to_string(days)});

    std::vector<ExpiringRow> rows;
    rows.reserve(static_cast<std::size_t>(result.rowCount()));
    for (int row = 0; row < result.rowCount(); ++row) {
        rows.push_back(ExpiringRow{
            result.int64(row, 0), result.text(row, 1), result.text(row, 2),
            result.text(row, 3), result.integer(row, 4), result.text(row, 5),
            result.integer(row, 6), result.int64(row, 7)});
    }
    return rows;
}

}  // namespace warehouse
