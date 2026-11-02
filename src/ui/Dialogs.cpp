#include "ui/Dialogs.hpp"

#include <commctrl.h>

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <string>
#include <vector>

#include "db/Database.hpp"
#include "resource.h"
#include "util/Format.hpp"
#include "util/Text.hpp"

namespace warehouse {
namespace {

constexpr int kTextBuffer = 4096;

std::wstring getWideText(HWND dialog, int controlId) {
    wchar_t buffer[kTextBuffer]{};
    GetDlgItemTextW(dialog, controlId, buffer, kTextBuffer);
    return buffer;
}

std::string getUtf8Text(HWND dialog, int controlId) {
    return wideToUtf8(getWideText(dialog, controlId));
}

void setWideText(HWND dialog, int controlId, const std::wstring& text) {
    SetDlgItemTextW(dialog, controlId, text.c_str());
}

void setUtf8Text(HWND dialog, int controlId, const std::string& text) {
    setWideText(dialog, controlId, utf8ToWide(text));
}

std::int64_t selectedComboId(HWND dialog, int controlId) {
    const auto index = static_cast<int>(
        SendDlgItemMessageW(dialog, controlId, CB_GETCURSEL, 0, 0));
    if (index == CB_ERR) {
        return 0;
    }
    return static_cast<std::int64_t>(
        SendDlgItemMessageW(dialog, controlId, CB_GETITEMDATA, index, 0));
}

void addComboText(HWND dialog, int controlId, const std::wstring& text,
                  std::int64_t data = 0) {
    const auto index =
        static_cast<int>(SendDlgItemMessageW(dialog, controlId, CB_ADDSTRING, 0,
                                             reinterpret_cast<LPARAM>(text.c_str())));
    if (index != CB_ERR && data != 0) {
        SendDlgItemMessageW(dialog, controlId, CB_SETITEMDATA, index, data);
    }
}

SYSTEMTIME systemTimeFromIso(const std::string& iso, bool* ok = nullptr) {
    SYSTEMTIME value{};
    int year = 0;
    int month = 0;
    int day = 0;
    const bool valid = iso.size() == 10 &&
                       sscanf_s(iso.c_str(), "%4d-%2d-%2d", &year, &month, &day) == 3;
    if (valid) {
        value.wYear = static_cast<WORD>(year);
        value.wMonth = static_cast<WORD>(month);
        value.wDay = static_cast<WORD>(day);
    }
    if (ok != nullptr) {
        *ok = valid;
    }
    return value;
}

std::string isoFromSystemTime(const SYSTEMTIME& value) {
    char buffer[16]{};
    std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d", value.wYear, value.wMonth,
                  value.wDay);
    return buffer;
}

SYSTEMTIME addDays(const SYSTEMTIME& value, int days) {
    std::tm tm{};
    tm.tm_year = value.wYear - 1900;
    tm.tm_mon = value.wMonth - 1;
    tm.tm_mday = value.wDay + days;
    tm.tm_hour = 12;
    const std::time_t time = std::mktime(&tm);
    std::tm normalized{};
    localtime_s(&normalized, &time);
    SYSTEMTIME result{};
    result.wYear = static_cast<WORD>(normalized.tm_year + 1900);
    result.wMonth = static_cast<WORD>(normalized.tm_mon + 1);
    result.wDay = static_cast<WORD>(normalized.tm_mday);
    return result;
}

void setDateControl(HWND dialog, int controlId, const SYSTEMTIME& value) {
    SendDlgItemMessageW(dialog, controlId, DTM_SETSYSTEMTIME, GDT_VALID,
                        reinterpret_cast<LPARAM>(&value));
}

std::string getDateControl(HWND dialog, int controlId) {
    SYSTEMTIME value{};
    if (SendDlgItemMessageW(dialog, controlId, DTM_GETSYSTEMTIME, 0,
                            reinterpret_cast<LPARAM>(&value)) != GDT_VALID) {
        return "";
    }
    return isoFromSystemTime(value);
}

std::wstring productLabel(const Product& product) {
    return utf8ToWide(product.name) + L" [" + utf8ToWide(product.sku) + L"]";
}

void fillCategories(HWND dialog, WarehouseService& service) {
    for (const auto& category : service.listCategories()) {
        addComboText(dialog, IDC_PROD_CATEGORY, utf8ToWide(category.name));
    }
}

void fillSuppliers(HWND dialog, WarehouseService& service) {
    for (const auto& supplier : service.listSuppliers()) {
        addComboText(dialog, IDC_SUP_SUPPLIER, utf8ToWide(supplier.name));
    }
}

void fillProducts(HWND dialog, int controlId, WarehouseService& service,
                  bool onlyInStock) {
    int selected = 0;
    for (const auto& product : service.listProducts()) {
        if (onlyInStock && product.stockQuantity <= 0) {
            continue;
        }
        addComboText(dialog, controlId, productLabel(product), product.id);
        ++selected;
    }
    if (selected > 0) {
        SendDlgItemMessageW(dialog, controlId, CB_SETCURSEL, 0, 0);
    }
}

void updateSupplyInfo(HWND dialog, WarehouseService& service) {
    const auto productId = selectedComboId(dialog, IDC_SUP_PRODUCT);
    if (productId == 0) {
        setWideText(dialog, IDC_SUP_INFO, L"Выберите товар.");
        return;
    }
    try {
        for (const auto& product : service.listProducts()) {
            if (product.id != productId) {
                continue;
            }
            const std::wstring text =
                L"Остаток: " + formatInt(product.stockQuantity) + L" " +
                utf8ToWide(product.unit) + L"; цена продажи: " +
                formatMoney(product.salePriceCents) + L"; НДС: " +
                formatPercent(product.taxRateBp);
            setWideText(dialog, IDC_SUP_INFO, text);
            return;
        }
    } catch (const std::exception& error) {
        setWideText(dialog, IDC_SUP_INFO, errorToWide(error));
    }
}

void updateSaleInfo(HWND dialog, WarehouseService& service) {
    const auto productId = selectedComboId(dialog, IDC_SALE_PRODUCT);
    if (productId == 0) {
        setWideText(dialog, IDC_SALE_INFO, L"Выберите товар.");
        return;
    }
    try {
        for (const auto& product : service.listProducts()) {
            if (product.id != productId) {
                continue;
            }
            setWideText(dialog, IDC_SALE_PRICE,
                        formatMoneyShort(product.salePriceCents));
            const std::wstring text =
                L"В наличии: " + formatInt(product.stockQuantity) + L" " +
                utf8ToWide(product.unit) + L". НДС включён в цену: " +
                formatPercent(product.taxRateBp);
            setWideText(dialog, IDC_SALE_INFO, text);
            return;
        }
    } catch (const std::exception& error) {
        setWideText(dialog, IDC_SALE_INFO, errorToWide(error));
    }
}

void updateWriteOffInfo(HWND dialog, WarehouseService& service) {
    const auto productId = selectedComboId(dialog, IDC_WO_PRODUCT);
    if (productId == 0) {
        setWideText(dialog, IDC_WO_INFO, L"Выберите товар.");
        return;
    }
    try {
        for (const auto& product : service.listProducts()) {
            if (product.id != productId) {
                continue;
            }
            const std::wstring text =
                L"Остаток: " + formatInt(product.stockQuantity) + L" " +
                utf8ToWide(product.unit) + L". Ближайший срок: " +
                (product.nearestExpiry.empty()
                     ? L"нет"
                     : utf8ToWide(formatDateRu(product.nearestExpiry)));
            setWideText(dialog, IDC_WO_INFO, text);
            return;
        }
    } catch (const std::exception& error) {
        setWideText(dialog, IDC_WO_INFO, errorToWide(error));
    }
}

std::wstring autoInvoiceNumber() {
    SYSTEMTIME now{};
    GetLocalTime(&now);
    wchar_t buffer[64]{};
    std::swprintf(buffer, 64, L"AUTO-%04d%02d%02d-%02d%02d%02d", now.wYear,
                  now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond);
    return buffer;
}

struct ConnectionContext {
    DbConfig* config = nullptr;
    std::string initialError;
};

struct ServiceContext {
    WarehouseService* service = nullptr;
};

DbConfig readConnectionConfig(HWND dialog) {
    DbConfig config;
    config.host = getUtf8Text(dialog, IDC_DB_HOST);
    const auto portText = getWideText(dialog, IDC_DB_PORT);
    if (const auto port = parseInt(portText)) {
        config.port = *port;
    } else {
        config.port = 0;
    }
    config.database = getUtf8Text(dialog, IDC_DB_NAME);
    config.user = getUtf8Text(dialog, IDC_DB_USER);
    config.password = getUtf8Text(dialog, IDC_DB_PASSWORD);
    const auto sslIndex = static_cast<int>(
        SendDlgItemMessageW(dialog, IDC_DB_SSLMODE, CB_GETCURSEL, 0, 0));
    if (sslIndex != CB_ERR) {
        wchar_t ssl[64]{};
        SendDlgItemMessageW(dialog, IDC_DB_SSLMODE, CB_GETLBTEXT, sslIndex,
                            reinterpret_cast<LPARAM>(ssl));
        config.sslMode = wideToUtf8(ssl);
    }
    return config;
}

bool testConnection(HWND dialog, const DbConfig& config) {
    try {
        Database database;
        database.connect(config);
        setWideText(dialog, IDC_DB_STATUS, L"Подключение успешно.");
        return true;
    } catch (const std::exception& error) {
        setWideText(dialog, IDC_DB_STATUS, L"Ошибка: " + errorToWide(error));
        return false;
    }
}

INT_PTR CALLBACK ConnectionDlgProc(HWND dialog, UINT message, WPARAM wParam,
                                   LPARAM lParam) {
    auto* context =
        reinterpret_cast<ConnectionContext*>(GetWindowLongPtrW(dialog, DWLP_USER));

    switch (message) {
        case WM_INITDIALOG: {
            context = reinterpret_cast<ConnectionContext*>(lParam);
            SetWindowLongPtrW(dialog, DWLP_USER,
                              reinterpret_cast<LONG_PTR>(context));
            if (context == nullptr || context->config == nullptr) {
                EndDialog(dialog, IDCANCEL);
                return TRUE;
            }
            setUtf8Text(dialog, IDC_DB_HOST, context->config->host);
            setWideText(dialog, IDC_DB_PORT,
                        std::to_wstring(context->config->port));
            setUtf8Text(dialog, IDC_DB_NAME, context->config->database);
            setUtf8Text(dialog, IDC_DB_USER, context->config->user);
            setUtf8Text(dialog, IDC_DB_PASSWORD, context->config->password);

            for (const wchar_t* mode :
                 {L"disable", L"allow", L"prefer", L"require", L"verify-ca",
                  L"verify-full"}) {
                addComboText(dialog, IDC_DB_SSLMODE, mode);
            }
            const auto sslMode = utf8ToWide(context->config->sslMode);
            for (int index = 0; index < 6; ++index) {
                wchar_t value[64]{};
                SendDlgItemMessageW(dialog, IDC_DB_SSLMODE, CB_GETLBTEXT, index,
                                    reinterpret_cast<LPARAM>(value));
                if (sslMode == value) {
                    SendDlgItemMessageW(dialog, IDC_DB_SSLMODE, CB_SETCURSEL, index,
                                        0);
                    break;
                }
            }
            if (!context->initialError.empty()) {
                setWideText(dialog, IDC_DB_STATUS,
                            L"Ошибка: " + utf8ToWide(context->initialError));
            }
            return TRUE;
        }
        case WM_COMMAND:
            if (LOWORD(wParam) == IDC_DB_TEST) {
                const auto config = readConnectionConfig(dialog);
                if (!config.isComplete()) {
                    setWideText(dialog, IDC_DB_STATUS,
                                L"Заполните хост, порт, базу и пользователя.");
                } else {
                    testConnection(dialog, config);
                }
                return TRUE;
            }
            if (LOWORD(wParam) == IDOK) {
                const auto config = readConnectionConfig(dialog);
                if (!config.isComplete()) {
                    setWideText(dialog, IDC_DB_STATUS,
                                L"Заполните хост, порт, базу и пользователя.");
                    return TRUE;
                }
                if (!testConnection(dialog, config)) {
                    return TRUE;
                }
                *context->config = config;
                EndDialog(dialog, IDOK);
                return TRUE;
            }
            if (LOWORD(wParam) == IDCANCEL) {
                EndDialog(dialog, IDCANCEL);
                return TRUE;
            }
            break;
        default:
            break;
    }
    return FALSE;
}

INT_PTR CALLBACK ProductDlgProc(HWND dialog, UINT message, WPARAM wParam,
                                LPARAM lParam) {
    auto* context = reinterpret_cast<ServiceContext*>(
        GetWindowLongPtrW(dialog, DWLP_USER));

    switch (message) {
        case WM_INITDIALOG:
            context = reinterpret_cast<ServiceContext*>(lParam);
            SetWindowLongPtrW(dialog, DWLP_USER,
                              reinterpret_cast<LONG_PTR>(context));
            if (context == nullptr || context->service == nullptr) {
                EndDialog(dialog, IDCANCEL);
                return TRUE;
            }
            try {
                fillCategories(dialog, *context->service);
            } catch (const std::exception& error) {
                showError(dialog, error);
            }
            setWideText(dialog, IDC_PROD_UNIT, L"шт");
            setWideText(dialog, IDC_PROD_TAX, L"20");
            setWideText(dialog, IDC_PROD_SHELF, L"30");
            setWideText(dialog, IDC_PROD_MIN, L"0");
            SetFocus(GetDlgItem(dialog, IDC_PROD_SKU));
            return FALSE;
        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK) {
                try {
                    ProductInput input;
                    input.sku = getUtf8Text(dialog, IDC_PROD_SKU);
                    input.name = getUtf8Text(dialog, IDC_PROD_NAME);
                    input.category = getUtf8Text(dialog, IDC_PROD_CATEGORY);
                    input.unit = getUtf8Text(dialog, IDC_PROD_UNIT);
                    const auto price = parseMoney(getWideText(dialog, IDC_PROD_PRICE));
                    if (!price) {
                        throw AppError("Укажите корректную цену продажи.");
                    }
                    input.salePriceCents = *price;
                    const auto tax = parsePercentToBasisPoints(
                        getWideText(dialog, IDC_PROD_TAX));
                    if (!tax) {
                        throw AppError("Укажите корректную ставку НДС.");
                    }
                    input.taxRateBp = *tax;
                    const auto shelf = parseInt(getWideText(dialog, IDC_PROD_SHELF));
                    if (!shelf) {
                        throw AppError("Укажите срок хранения в днях.");
                    }
                    input.shelfLifeDays = *shelf;
                    const auto minStock = parseInt(getWideText(dialog, IDC_PROD_MIN));
                    if (!minStock) {
                        throw AppError("Укажите минимальный остаток.");
                    }
                    input.minStock = *minStock;
                    context->service->createProduct(input);
                    EndDialog(dialog, IDOK);
                } catch (const std::exception& error) {
                    showError(dialog, error);
                }
                return TRUE;
            }
            if (LOWORD(wParam) == IDCANCEL) {
                EndDialog(dialog, IDCANCEL);
                return TRUE;
            }
            break;
        default:
            break;
    }
    return FALSE;
}

INT_PTR CALLBACK SupplyDlgProc(HWND dialog, UINT message, WPARAM wParam,
                               LPARAM lParam) {
    auto* context = reinterpret_cast<ServiceContext*>(
        GetWindowLongPtrW(dialog, DWLP_USER));

    switch (message) {
        case WM_INITDIALOG:
            context = reinterpret_cast<ServiceContext*>(lParam);
            SetWindowLongPtrW(dialog, DWLP_USER,
                              reinterpret_cast<LONG_PTR>(context));
            if (context == nullptr || context->service == nullptr) {
                EndDialog(dialog, IDCANCEL);
                return TRUE;
            }
            try {
                fillSuppliers(dialog, *context->service);
                fillProducts(dialog, IDC_SUP_PRODUCT, *context->service, false);
                SYSTEMTIME today{};
                GetLocalTime(&today);
                setDateControl(dialog, IDC_SUP_DATE, today);
                setDateControl(dialog, IDC_SUP_EXPIRY, addDays(today, 30));
                setWideText(dialog, IDC_SUP_INVOICE, autoInvoiceNumber());
                if (SendDlgItemMessageW(dialog, IDC_SUP_PRODUCT, CB_GETCOUNT, 0, 0) ==
                    0) {
                    setWideText(dialog, IDC_SUP_INFO,
                                L"Сначала добавьте хотя бы один товар.");
                    EnableWindow(GetDlgItem(dialog, IDOK), FALSE);
                } else {
                    updateSupplyInfo(dialog, *context->service);
                }
            } catch (const std::exception& error) {
                showError(dialog, error);
                EndDialog(dialog, IDCANCEL);
            }
            return FALSE;
        case WM_COMMAND:
            if (HIWORD(wParam) == CBN_SELCHANGE &&
                LOWORD(wParam) == IDC_SUP_PRODUCT) {
                updateSupplyInfo(dialog, *context->service);
                return TRUE;
            }
            if (LOWORD(wParam) == IDOK) {
                try {
                    const auto productId = selectedComboId(dialog, IDC_SUP_PRODUCT);
                    if (productId == 0) {
                        throw AppError("Выберите товар.");
                    }
                    SupplyInput input;
                    input.supplierName = getUtf8Text(dialog, IDC_SUP_SUPPLIER);
                    input.invoiceNo = getUtf8Text(dialog, IDC_SUP_INVOICE);
                    input.productId = productId;
                    const auto quantity = parseInt(getWideText(dialog, IDC_SUP_QTY));
                    if (!quantity) {
                        throw AppError("Укажите количество.");
                    }
                    input.quantity = *quantity;
                    const auto cost = parseMoney(getWideText(dialog, IDC_SUP_COST));
                    if (!cost) {
                        throw AppError("Укажите закупочную цену.");
                    }
                    input.unitCostCents = *cost;
                    input.expiryDate = getDateControl(dialog, IDC_SUP_EXPIRY);
                    input.suppliedAt = getDateControl(dialog, IDC_SUP_DATE);
                    input.note = getUtf8Text(dialog, IDC_SUP_NOTE);
                    context->service->receiveSupply(input);
                    EndDialog(dialog, IDOK);
                } catch (const std::exception& error) {
                    showError(dialog, error);
                }
                return TRUE;
            }
            if (LOWORD(wParam) == IDCANCEL) {
                EndDialog(dialog, IDCANCEL);
                return TRUE;
            }
            break;
        default:
            break;
    }
    return FALSE;
}

INT_PTR CALLBACK SaleDlgProc(HWND dialog, UINT message, WPARAM wParam,
                             LPARAM lParam) {
    auto* context = reinterpret_cast<ServiceContext*>(
        GetWindowLongPtrW(dialog, DWLP_USER));

    switch (message) {
        case WM_INITDIALOG:
            context = reinterpret_cast<ServiceContext*>(lParam);
            SetWindowLongPtrW(dialog, DWLP_USER,
                              reinterpret_cast<LONG_PTR>(context));
            if (context == nullptr || context->service == nullptr) {
                EndDialog(dialog, IDCANCEL);
                return TRUE;
            }
            try {
                fillProducts(dialog, IDC_SALE_PRODUCT, *context->service, true);
                if (SendDlgItemMessageW(dialog, IDC_SALE_PRODUCT, CB_GETCOUNT, 0, 0) ==
                    0) {
                    setWideText(dialog, IDC_SALE_INFO,
                                L"Нет товара в наличии. Сначала оформите поступление.");
                    EnableWindow(GetDlgItem(dialog, IDOK), FALSE);
                } else {
                    setWideText(dialog, IDC_SALE_QTY, L"1");
                    updateSaleInfo(dialog, *context->service);
                }
            } catch (const std::exception& error) {
                showError(dialog, error);
                EndDialog(dialog, IDCANCEL);
            }
            return FALSE;
        case WM_COMMAND:
            if (HIWORD(wParam) == CBN_SELCHANGE &&
                LOWORD(wParam) == IDC_SALE_PRODUCT) {
                updateSaleInfo(dialog, *context->service);
                return TRUE;
            }
            if (LOWORD(wParam) == IDOK) {
                try {
                    const auto productId = selectedComboId(dialog, IDC_SALE_PRODUCT);
                    if (productId == 0) {
                        throw AppError("Выберите товар.");
                    }
                    SaleInput input;
                    input.productId = productId;
                    const auto quantity = parseInt(getWideText(dialog, IDC_SALE_QTY));
                    if (!quantity) {
                        throw AppError("Укажите количество.");
                    }
                    input.quantity = *quantity;
                    const auto price = parseMoney(getWideText(dialog, IDC_SALE_PRICE));
                    if (!price) {
                        throw AppError("Укажите цену продажи.");
                    }
                    input.unitPriceCents = *price;
                    const auto result = context->service->sell(input);
                    showInfo(dialog,
                             L"Продажа оформлена. Прибыль: " +
                                 formatMoney(result.profitCents) + L".");
                    EndDialog(dialog, IDOK);
                } catch (const std::exception& error) {
                    showError(dialog, error);
                }
                return TRUE;
            }
            if (LOWORD(wParam) == IDCANCEL) {
                EndDialog(dialog, IDCANCEL);
                return TRUE;
            }
            break;
        default:
            break;
    }
    return FALSE;
}

INT_PTR CALLBACK WriteOffDlgProc(HWND dialog, UINT message, WPARAM wParam,
                                 LPARAM lParam) {
    auto* context = reinterpret_cast<ServiceContext*>(
        GetWindowLongPtrW(dialog, DWLP_USER));

    switch (message) {
        case WM_INITDIALOG:
            context = reinterpret_cast<ServiceContext*>(lParam);
            SetWindowLongPtrW(dialog, DWLP_USER,
                              reinterpret_cast<LONG_PTR>(context));
            if (context == nullptr || context->service == nullptr) {
                EndDialog(dialog, IDCANCEL);
                return TRUE;
            }
            try {
                fillProducts(dialog, IDC_WO_PRODUCT, *context->service, true);
                addComboText(dialog, IDC_WO_REASON, L"Истёк срок годности", 0);
                addComboText(dialog, IDC_WO_REASON, L"Брак / порча", 1);
                addComboText(dialog, IDC_WO_REASON, L"Прочее", 2);
                SendDlgItemMessageW(dialog, IDC_WO_REASON, CB_SETCURSEL, 0, 0);
                setWideText(dialog, IDC_WO_QTY, L"1");
                if (SendDlgItemMessageW(dialog, IDC_WO_PRODUCT, CB_GETCOUNT, 0, 0) ==
                    0) {
                    setWideText(dialog, IDC_WO_INFO,
                                L"Нет товара в наличии. Сначала оформите поступление.");
                    EnableWindow(GetDlgItem(dialog, IDOK), FALSE);
                } else {
                    updateWriteOffInfo(dialog, *context->service);
                }
            } catch (const std::exception& error) {
                showError(dialog, error);
                EndDialog(dialog, IDCANCEL);
            }
            return FALSE;
        case WM_COMMAND:
            if (HIWORD(wParam) == CBN_SELCHANGE &&
                LOWORD(wParam) == IDC_WO_PRODUCT) {
                updateWriteOffInfo(dialog, *context->service);
                return TRUE;
            }
            if (LOWORD(wParam) == IDOK) {
                try {
                    const auto productId = selectedComboId(dialog, IDC_WO_PRODUCT);
                    if (productId == 0) {
                        throw AppError("Выберите товар.");
                    }
                    WriteOffInput input;
                    input.productId = productId;
                    const auto quantity = parseInt(getWideText(dialog, IDC_WO_QTY));
                    if (!quantity) {
                        throw AppError("Укажите количество.");
                    }
                    input.quantity = *quantity;
                    const auto reasonIndex = static_cast<int>(
                        SendDlgItemMessageW(dialog, IDC_WO_REASON, CB_GETCURSEL, 0, 0));
                    input.reason = reasonIndex == 1
                                       ? WriteOffReason::Damaged
                                       : (reasonIndex == 2 ? WriteOffReason::Other
                                                           : WriteOffReason::Expired);
                    input.comment = getUtf8Text(dialog, IDC_WO_COMMENT);
                    const auto result = context->service->writeOff(input);
                    showInfo(dialog,
                             L"Списано " + formatInt(result.quantity) +
                                 L" ед. на сумму " + formatMoney(result.costCents) + L".");
                    EndDialog(dialog, IDOK);
                } catch (const std::exception& error) {
                    showError(dialog, error);
                }
                return TRUE;
            }
            if (LOWORD(wParam) == IDCANCEL) {
                EndDialog(dialog, IDCANCEL);
                return TRUE;
            }
            break;
        default:
            break;
    }
    return FALSE;
}

}  // namespace

bool showConnectionDialog(HWND parent, DbConfig& config,
                          const std::string& initialError) {
    ConnectionContext context{&config, initialError};
    return DialogBoxParamW(GetModuleHandleW(nullptr),
                           MAKEINTRESOURCEW(IDD_CONNECTION), parent,
                           ConnectionDlgProc,
                           reinterpret_cast<LPARAM>(&context)) == IDOK;
}

bool showProductDialog(HWND parent, WarehouseService& service) {
    ServiceContext context{&service};
    return DialogBoxParamW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDD_PRODUCT),
                           parent, ProductDlgProc,
                           reinterpret_cast<LPARAM>(&context)) == IDOK;
}

bool showSupplyDialog(HWND parent, WarehouseService& service) {
    ServiceContext context{&service};
    return DialogBoxParamW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDD_SUPPLY),
                           parent, SupplyDlgProc,
                           reinterpret_cast<LPARAM>(&context)) == IDOK;
}

bool showSaleDialog(HWND parent, WarehouseService& service) {
    ServiceContext context{&service};
    return DialogBoxParamW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDD_SALE),
                           parent, SaleDlgProc,
                           reinterpret_cast<LPARAM>(&context)) == IDOK;
}

bool showWriteOffDialog(HWND parent, WarehouseService& service) {
    ServiceContext context{&service};
    return DialogBoxParamW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDD_WRITEOFF),
                           parent, WriteOffDlgProc,
                           reinterpret_cast<LPARAM>(&context)) == IDOK;
}

void showError(HWND parent, const std::exception& error) {
    MessageBoxW(parent, errorToWide(error).c_str(), L"Ошибка",
                MB_OK | MB_ICONERROR);
}

void showInfo(HWND parent, const std::wstring& message) {
    MessageBoxW(parent, message.c_str(), L"Складской учёт",
                MB_OK | MB_ICONINFORMATION);
}

}  // namespace warehouse
