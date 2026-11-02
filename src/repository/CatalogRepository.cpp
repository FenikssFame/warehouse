#include "repository/CatalogRepository.hpp"

#include <stdexcept>

namespace warehouse {
namespace {

Product readProduct(const Result& result, int row) {
    Product product;
    product.id = result.int64(row, 0);
    product.sku = result.text(row, 1);
    product.name = result.text(row, 2);
    product.category = result.text(row, 3);
    product.unit = result.text(row, 4);
    product.salePriceCents = result.int64(row, 5);
    product.taxRateBp = result.integer(row, 6);
    product.shelfLifeDays = result.integer(row, 7);
    product.minStock = result.integer(row, 8);
    product.stockQuantity = result.integer(row, 9);
    product.stockCostCents = result.int64(row, 10);
    product.nearestExpiry = result.text(row, 11);
    product.status = result.text(row, 12);
    return product;
}

constexpr const char* kProductColumns =
    "product_id, sku, name, category, unit, sale_price_cents, tax_rate_bp, "
    "shelf_life_days, min_stock, stock_quantity, stock_cost_cents, "
    "COALESCE(to_char(nearest_expiry, 'YYYY-MM-DD'), ''), stock_status";

}  // namespace

std::vector<Product> CatalogRepository::listProducts() {
    const auto result = database_.exec(
        std::string("SELECT ") + kProductColumns +
        " FROM v_product_stock ORDER BY name");
    std::vector<Product> products;
    products.reserve(static_cast<std::size_t>(result.rowCount()));
    for (int row = 0; row < result.rowCount(); ++row) {
        products.push_back(readProduct(result, row));
    }
    return products;
}

Product CatalogRepository::getProduct(std::int64_t productId) {
    const auto result = database_.exec(
        std::string("SELECT ") + kProductColumns +
            " FROM v_product_stock WHERE product_id = $1::bigint",
        {std::to_string(productId)});
    if (result.rowCount() == 0) {
        throw std::runtime_error("Товар не найден.");
    }
    return readProduct(result, 0);
}

std::vector<Category> CatalogRepository::listCategories() {
    const auto result = database_.exec(
        "SELECT id, name FROM product_categories ORDER BY name");
    std::vector<Category> categories;
    categories.reserve(static_cast<std::size_t>(result.rowCount()));
    for (int row = 0; row < result.rowCount(); ++row) {
        categories.push_back(
            Category{result.int64(row, 0), result.text(row, 1)});
    }
    return categories;
}

std::vector<Supplier> CatalogRepository::listSuppliers() {
    const auto result = database_.exec(
        "SELECT id, name, inn, contact FROM suppliers ORDER BY name");
    std::vector<Supplier> suppliers;
    suppliers.reserve(static_cast<std::size_t>(result.rowCount()));
    for (int row = 0; row < result.rowCount(); ++row) {
        suppliers.push_back(Supplier{result.int64(row, 0), result.text(row, 1),
                                     result.text(row, 2), result.text(row, 3)});
    }
    return suppliers;
}

std::int64_t CatalogRepository::getOrCreateCategory(const std::string& name) {
    const auto result = database_.exec(
        "WITH inserted AS ("
        "  INSERT INTO product_categories(name) VALUES($1)"
        "  ON CONFLICT (name) DO NOTHING"
        "  RETURNING id"
        ") "
        "SELECT id FROM inserted "
        "UNION ALL "
        "SELECT id FROM product_categories WHERE name = $1 "
        "LIMIT 1",
        {name});
    if (result.rowCount() == 0) {
        throw std::runtime_error("Не удалось создать категорию товара.");
    }
    return result.int64(0, 0);
}

std::int64_t CatalogRepository::getOrCreateSupplier(const std::string& name) {
    const auto result = database_.exec(
        "WITH inserted AS ("
        "  INSERT INTO suppliers(name) VALUES($1)"
        "  ON CONFLICT (name) DO NOTHING"
        "  RETURNING id"
        ") "
        "SELECT id FROM inserted "
        "UNION ALL "
        "SELECT id FROM suppliers WHERE name = $1 "
        "LIMIT 1",
        {name});
    if (result.rowCount() == 0) {
        throw std::runtime_error("Не удалось создать поставщика.");
    }
    return result.int64(0, 0);
}

std::int64_t CatalogRepository::createProduct(const ProductInput& input) {
    const auto categoryId = getOrCreateCategory(input.category);
    const auto result = database_.exec(
        "INSERT INTO products("
        "  sku, name, category_id, unit, sale_price_cents, tax_rate_bp,"
        "  shelf_life_days, min_stock"
        ") VALUES($1, $2, $3::bigint, $4, $5::bigint, $6::int, $7::int, $8::int)"
        " RETURNING id",
        {input.sku,
         input.name,
         std::to_string(categoryId),
         input.unit,
         std::to_string(input.salePriceCents),
         std::to_string(input.taxRateBp),
         std::to_string(input.shelfLifeDays),
         std::to_string(input.minStock)});
    return result.int64(0, 0);
}

}  // namespace warehouse
