#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "db/Database.hpp"
#include "models.hpp"

namespace warehouse {

class CatalogRepository {
public:
    explicit CatalogRepository(Database& database) : database_(database) {}

    std::vector<Product> listProducts();
    Product getProduct(std::int64_t productId);
    std::vector<Category> listCategories();
    std::vector<Supplier> listSuppliers();

    std::int64_t getOrCreateCategory(const std::string& name);
    std::int64_t getOrCreateSupplier(const std::string& name);
    std::int64_t createProduct(const ProductInput& input);

private:
    Database& database_;
};

}  // namespace warehouse
