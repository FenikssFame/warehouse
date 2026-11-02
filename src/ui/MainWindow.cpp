#include "ui/MainWindow.hpp"

#include <commctrl.h>
#include <uxtheme.h>

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

#include "resource.h"
#include "ui/Dialogs.hpp"
#include "util/Format.hpp"
#include "util/Text.hpp"

namespace warehouse {
namespace {

constexpr int kMargin = 12;

std::wstring wide(const std::string& value) {
    return utf8ToWide(value);
}

std::string narrow(const std::wstring& value) {
    return wideToUtf8(value);
}

std::wstring statusRu(const std::string& status) {
    if (status == "OUT") {
        return L"Нет в наличии";
    }
    if (status == "EXPIRED") {
        return L"Просрочен";
    }
    if (status == "SOON") {
        return L"Скоро истекает";
    }
    if (status == "LOW") {
        return L"Мало";
    }
    return L"В наличии";
}

COLORREF statusColor(const std::string& status) {
    if (status == "EXPIRED") {
        return RGB(176, 42, 42);
    }
    if (status == "SOON") {
        return RGB(190, 105, 0);
    }
    if (status == "LOW") {
        return RGB(143, 110, 0);
    }
    return 0;
}

std::wstring reasonRu(const std::string& reason) {
    if (reason == "EXPIRED") {
        return L"Истёк срок";
    }
    if (reason == "DAMAGED") {
        return L"Брак / порча";
    }
    return L"Прочее";
}

SYSTEMTIME systemTimeFromIso(const std::string& iso) {
    SYSTEMTIME value{};
    int year = 0;
    int month = 0;
    int day = 0;
    if (iso.size() == 10 &&
        sscanf_s(iso.c_str(), "%4d-%2d-%2d", &year, &month, &day) == 3) {
        value.wYear = static_cast<WORD>(year);
        value.wMonth = static_cast<WORD>(month);
        value.wDay = static_cast<WORD>(day);
    }
    return value;
}

std::string isoFromSystemTime(const SYSTEMTIME& value) {
    char buffer[16]{};
    std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d", value.wYear, value.wMonth,
                  value.wDay);
    return buffer;
}

std::string dateControlIso(HWND dialog, int controlId) {
    SYSTEMTIME value{};
    if (SendDlgItemMessageW(dialog, controlId, DTM_GETSYSTEMTIME, 0,
                            reinterpret_cast<LPARAM>(&value)) != GDT_VALID) {
        return "";
    }
    return isoFromSystemTime(value);
}

void setDateControl(HWND dialog, int controlId, const SYSTEMTIME& value) {
    SendDlgItemMessageW(dialog, controlId, DTM_SETSYSTEMTIME, GDT_VALID,
                        reinterpret_cast<LPARAM>(&value));
}

int scale(HWND window, int value) {
    const int dpi = window != nullptr ? GetDpiForWindow(window) : GetDpiForSystem();
    return MulDiv(value, dpi == 0 ? 96 : dpi, 96);
}

void setFont(HWND control, HFONT font) {
    if (control != nullptr) {
        SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    }
}

}  // namespace

MainWindow::MainWindow(HINSTANCE instance, Database& database,
                       WarehouseService& service, DbConfig& config)
    : instance_(instance), database_(database), service_(service), config_(config) {}

MainWindow::~MainWindow() {
    if (font_ != nullptr) {
        DeleteObject(font_);
    }
    if (fontBold_ != nullptr) {
        DeleteObject(fontBold_);
    }
    if (fontTitle_ != nullptr) {
        DeleteObject(fontTitle_);
    }
}

bool MainWindow::create(int showCommand) {
    if (showCommand == SW_HIDE) {
        showCommand = SW_SHOWNORMAL;
    }

    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = &MainWindow::WndProc;
    windowClass.hInstance = instance_;
    windowClass.hIcon = LoadIconW(instance_, MAKEINTRESOURCEW(IDI_APP));
    windowClass.hIconSm = LoadIconW(instance_, MAKEINTRESOURCEW(IDI_APP));
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    windowClass.lpszMenuName = MAKEINTRESOURCEW(IDR_MAIN_MENU);
    windowClass.lpszClassName = L"WarehouseMainWindowClass";

    if (RegisterClassExW(&windowClass) == 0 &&
        GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    RECT workArea{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
    const int screenWidth = workArea.right - workArea.left;
    const int screenHeight = workArea.bottom - workArea.top;
    const int windowWidth = std::min(scale(nullptr, 1200), screenWidth - 24);
    const int windowHeight = std::min(scale(nullptr, 720), screenHeight - 24);
    const int windowX = workArea.left + (screenWidth - windowWidth) / 2;
    const int windowY = workArea.top + (screenHeight - windowHeight) / 2;

    window_ = CreateWindowExW(
        0, windowClass.lpszClassName, L"Складской учёт - PostgreSQL",
        WS_OVERLAPPEDWINDOW, windowX, windowY, windowWidth, windowHeight, nullptr,
        nullptr, instance_, this);
    if (window_ == nullptr) {
        return false;
    }
    SetWindowTextW(window_, L"Складской учёт - PostgreSQL");

    ShowWindow(window_, showCommand);
    UpdateWindow(window_);
    return true;
}

int MainWindow::run() {
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}

LRESULT CALLBACK MainWindow::WndProc(HWND window, UINT message, WPARAM wParam,
                                     LPARAM lParam) {
    if (message == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        auto* self = static_cast<MainWindow*>(create->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->window_ = window;
        return TRUE;
    }

    auto* self =
        reinterpret_cast<MainWindow*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (self == nullptr) {
        return DefWindowProcW(window, message, wParam, lParam);
    }
    return self->handleMessage(message, wParam, lParam);
}

LRESULT MainWindow::handleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_CREATE:
            createFonts();
            createControls();
            updateConnectionStatus();
            applyTab();
            return 0;

        case WM_SIZE:
            layoutControls();
            return 0;

        case WM_GETMINMAXINFO: {
            auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
            info->ptMinTrackSize.x = scale(window_, 980);
            info->ptMinTrackSize.y = scale(window_, 660);
            return 0;
        }

        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDC_PRIMARY:
                    primaryAction();
                    return 0;
                case IDC_REFRESH:
                    reload();
                    return 0;
                case IDC_REPORT_TYPE:
                    if (HIWORD(wParam) == CBN_SELCHANGE) {
                        updateReportVisibility();
                    }
                    return 0;
                case ID_FILE_SETTINGS:
                    openSettings();
                    return 0;
                case ID_FILE_EXIT:
                    PostMessageW(window_, WM_CLOSE, 0, 0);
                    return 0;
                case ID_HELP_ABOUT:
                    showAbout();
                    return 0;
                default:
                    break;
            }
            break;

        case WM_NOTIFY: {
            const auto* header = reinterpret_cast<NMHDR*>(lParam);
            if (header->idFrom == IDC_TAB && header->code == TCN_SELCHANGE) {
                const int selected = TabCtrl_GetCurSel(tab_);
                if (selected >= 0) {
                    activeTab_ = static_cast<Tab>(selected);
                    applyTab();
                }
                return 0;
            }
            if (header->idFrom == IDC_LIST && header->code == NM_CUSTOMDRAW) {
                auto* draw = reinterpret_cast<NMLVCUSTOMDRAW*>(lParam);
                if (draw->nmcd.dwDrawStage == CDDS_PREPAINT) {
                    return CDRF_NOTIFYITEMDRAW;
                }
                if (draw->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) {
                    const auto row = static_cast<std::size_t>(draw->nmcd.dwItemSpec);
                    if (row < rowColors_.size() && rowColors_[row] != 0) {
                        draw->clrText = rowColors_[row];
                    }
                    return CDRF_DODEFAULT;
                }
            }
            break;
        }

        case WM_CTLCOLORSTATIC: {
            HDC deviceContext = reinterpret_cast<HDC>(wParam);
            SetBkMode(deviceContext, TRANSPARENT);
            if (reinterpret_cast<HWND>(lParam) == title_) {
                SetTextColor(deviceContext, RGB(31, 78, 121));
            } else if (reinterpret_cast<HWND>(lParam) == subtitle_) {
                SetTextColor(deviceContext, RGB(90, 100, 112));
            }
            return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_WINDOW));
        }

        case WM_CLOSE:
            DestroyWindow(window_);
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        default:
            break;
    }
    return DefWindowProcW(window_, message, wParam, lParam);
}

void MainWindow::createFonts() {
    HDC deviceContext = GetDC(window_);
    const int dpi = deviceContext != nullptr
                        ? GetDeviceCaps(deviceContext, LOGPIXELSY)
                        : 96;
    if (deviceContext != nullptr) {
        ReleaseDC(window_, deviceContext);
    }

    font_ = CreateFontW(-MulDiv(9, dpi, 72), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    fontBold_ =
        CreateFontW(-MulDiv(9, dpi, 72), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                    CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    fontTitle_ =
        CreateFontW(-MulDiv(20, dpi, 72), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                    CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
}

void MainWindow::createControls() {
    title_ = CreateWindowExW(0, L"STATIC", L"Складской учёт",
                             WS_CHILD | WS_VISIBLE | SS_LEFT, 0, 0, 0, 0, window_,
                             nullptr, instance_, nullptr);
    subtitle_ = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_LEFT,
                                0, 0, 0, 0, window_, nullptr, instance_, nullptr);

    tab_ = CreateWindowExW(0, WC_TABCONTROLW, L"",
                           WS_CHILD | WS_VISIBLE | TCS_TABS | WS_TABSTOP, 0, 0, 0,
                           0, window_, reinterpret_cast<HMENU>(IDC_TAB), instance_,
                           nullptr);
    const wchar_t* tabs[] = {L"Товары", L"Поступления", L"Продажи", L"Списания",
                             L"Отчёты"};
    for (int index = 0; index < 5; ++index) {
        TCITEMW item{};
        item.mask = TCIF_TEXT;
        item.pszText = const_cast<wchar_t*>(tabs[index]);
        TabCtrl_InsertItem(tab_, index, &item);
    }
    TabCtrl_SetCurSel(tab_, 0);

    primaryButton_ = CreateWindowExW(
        0, L"BUTTON", L"Добавить товар",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON, 0, 0, 0, 0, window_,
        reinterpret_cast<HMENU>(IDC_PRIMARY), instance_, nullptr);
    refreshButton_ = CreateWindowExW(
        0, L"BUTTON", L"Обновить", WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 0, 0, 0,
        window_, reinterpret_cast<HMENU>(IDC_REFRESH), instance_, nullptr);

    list_ = CreateWindowExW(
        WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS |
            WS_TABSTOP,
        0, 0, 0, 0, window_, reinterpret_cast<HMENU>(IDC_LIST), instance_, nullptr);
    ListView_SetExtendedListViewStyle(
        list_, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_GRIDLINES);
    SetWindowTheme(list_, L"Explorer", nullptr);

    status_ = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_LEFT |
                                                      SS_SUNKEN,
                              0, 0, 0, 0, window_, reinterpret_cast<HMENU>(IDC_STATUS),
                              instance_, nullptr);

    reportType_ = CreateWindowExW(
        0, L"COMBOBOX", L"",
        WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 0, 0, 0, 0, window_,
        reinterpret_cast<HMENU>(IDC_REPORT_TYPE), instance_, nullptr);
    const wchar_t* reportNames[] = {L"Прибыль по товарам", L"Просроченные списания",
                                    L"Скоро испортятся"};
    for (int index = 0; index < 3; ++index) {
        SendMessageW(reportType_, CB_ADDSTRING, 0,
                     reinterpret_cast<LPARAM>(reportNames[index]));
    }
    SendMessageW(reportType_, CB_SETCURSEL, 0, 0);

    reportFromLabel_ = CreateWindowExW(0, L"STATIC", L"Период с:",
                                       WS_CHILD | SS_LEFT, 0, 0, 0, 0, window_,
                                       nullptr, instance_, nullptr);
    reportFrom_ = CreateWindowExW(
        0, DATETIMEPICK_CLASSW, L"",
        WS_CHILD | DTS_SHORTDATEFORMAT | WS_TABSTOP, 0, 0, 0, 0, window_,
        reinterpret_cast<HMENU>(IDC_REPORT_FROM), instance_, nullptr);
    reportToLabel_ = CreateWindowExW(0, L"STATIC", L"по:", WS_CHILD | SS_LEFT, 0, 0,
                                     0, 0, window_, nullptr, instance_, nullptr);
    reportTo_ = CreateWindowExW(
        0, DATETIMEPICK_CLASSW, L"",
        WS_CHILD | DTS_SHORTDATEFORMAT | WS_TABSTOP, 0, 0, 0, 0, window_,
        reinterpret_cast<HMENU>(IDC_REPORT_TO), instance_, nullptr);
    daysLabel_ = CreateWindowExW(0, L"STATIC", L"Дней:", WS_CHILD | SS_LEFT, 0, 0, 0,
                                 0, window_, nullptr, instance_, nullptr);
    days_ = CreateWindowExW(0, L"EDIT", L"7",
                            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_NUMBER |
                                WS_TABSTOP,
                            0, 0, 0, 0, window_,
                            reinterpret_cast<HMENU>(IDC_REPORT_DAYS), instance_,
                            nullptr);
    summary_ = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | SS_LEFT | SS_ENDELLIPSIS,
                               0, 0, 0, 0, window_, reinterpret_cast<HMENU>(IDC_SUMMARY),
                               instance_, nullptr);

    for (HWND control : {title_, subtitle_, tab_, primaryButton_, refreshButton_,
                         list_, status_, reportType_, reportFromLabel_, reportFrom_,
                         reportToLabel_, reportTo_, daysLabel_, days_, summary_}) {
        setFont(control, font_);
    }
    setFont(title_, fontTitle_);
    setFont(primaryButton_, fontBold_);

    SYSTEMTIME today{};
    GetLocalTime(&today);
    SYSTEMTIME firstDay = today;
    firstDay.wDay = 1;
    setDateControl(window_, IDC_REPORT_FROM, firstDay);
    setDateControl(window_, IDC_REPORT_TO, today);

    updateReportVisibility();
    layoutControls();
}

void MainWindow::layoutControls() {
    if (window_ == nullptr || list_ == nullptr) {
        return;
    }
    RECT client{};
    GetClientRect(window_, &client);
    const int margin = scale(window_, kMargin);

    MoveWindow(title_, margin, scale(window_, 10), client.right - 2 * margin,
               scale(window_, 30), TRUE);
    MoveWindow(subtitle_, margin, scale(window_, 40), client.right - 2 * margin,
               scale(window_, 18), TRUE);
    MoveWindow(tab_, margin, scale(window_, 64), client.right - 2 * margin,
               scale(window_, 30), TRUE);

    const int toolbarY = scale(window_, 102);
    MoveWindow(primaryButton_, margin, toolbarY, scale(window_, 220),
               scale(window_, 28), TRUE);
    MoveWindow(refreshButton_, margin + scale(window_, 230), toolbarY,
               scale(window_, 100), scale(window_, 28), TRUE);

    const bool reports = activeTab_ == Tab::Reports;
    int listY = scale(window_, 142);
    if (reports) {
        const int reportY = scale(window_, 140);
        int x = margin;
        MoveWindow(reportType_, x, reportY, scale(window_, 220), scale(window_, 200),
                   TRUE);
        x += scale(window_, 232);
        MoveWindow(reportFromLabel_, x, reportY + scale(window_, 5),
                   scale(window_, 60), scale(window_, 18), TRUE);
        x += scale(window_, 62);
        MoveWindow(reportFrom_, x, reportY, scale(window_, 105), scale(window_, 24),
                   TRUE);
        x += scale(window_, 112);
        MoveWindow(reportToLabel_, x, reportY + scale(window_, 5), scale(window_, 28),
                   scale(window_, 18), TRUE);
        x += scale(window_, 30);
        MoveWindow(reportTo_, x, reportY, scale(window_, 105), scale(window_, 24),
                   TRUE);
        x += scale(window_, 115);
        MoveWindow(daysLabel_, x, reportY + scale(window_, 5), scale(window_, 50),
                   scale(window_, 18), TRUE);
        x += scale(window_, 50);
        MoveWindow(days_, x, reportY, scale(window_, 55), scale(window_, 24), TRUE);

        MoveWindow(summary_, margin, scale(window_, 168),
                   client.right - 2 * margin, scale(window_, 20), TRUE);
        listY = scale(window_, 194);
    }

    const int statusHeight = scale(window_, 22);
    MoveWindow(status_, margin, client.bottom - statusHeight - scale(window_, 8),
               client.right - 2 * margin, statusHeight, TRUE);
    const int listBottom = client.bottom - statusHeight - scale(window_, 18);
    const int listHeight = std::max(scale(window_, 100), listBottom - listY);
    MoveWindow(list_, margin, listY, client.right - 2 * margin, listHeight, TRUE);
}

void MainWindow::updateConnectionStatus() {
    if (subtitle_ == nullptr) {
        return;
    }
    std::wstring text = L"PostgreSQL";
    if (database_.isConnected()) {
        text += L" · " + utf8ToWide(config_.database) + L"@";
        text += utf8ToWide(config_.host) + L":" +
                std::to_wstring(config_.port);
    } else {
        text += L" · нет подключения";
    }
    SetWindowTextW(subtitle_, text.c_str());
}

void MainWindow::updateTabControls() {
    if (primaryButton_ == nullptr) {
        return;
    }
    switch (activeTab_) {
        case Tab::Products:
            SetWindowTextW(primaryButton_, L"Добавить товар");
            break;
        case Tab::Supplies:
            SetWindowTextW(primaryButton_, L"Оформить поступление");
            break;
        case Tab::Sales:
            SetWindowTextW(primaryButton_, L"Продать");
            break;
        case Tab::WriteOffs:
            SetWindowTextW(primaryButton_, L"Списать");
            break;
        case Tab::Reports:
            SetWindowTextW(primaryButton_, L"Сформировать");
            break;
    }
    updateReportVisibility();
    layoutControls();
}

void MainWindow::updateReportVisibility() {
    const bool reports = activeTab_ == Tab::Reports;
    const bool days = reports && reportKind() == 2;
    const bool period = reports && !days;

    ShowWindow(reportType_, reports ? SW_SHOW : SW_HIDE);
    ShowWindow(reportFromLabel_, period ? SW_SHOW : SW_HIDE);
    ShowWindow(reportFrom_, period ? SW_SHOW : SW_HIDE);
    ShowWindow(reportToLabel_, period ? SW_SHOW : SW_HIDE);
    ShowWindow(reportTo_, period ? SW_SHOW : SW_HIDE);
    ShowWindow(daysLabel_, days ? SW_SHOW : SW_HIDE);
    ShowWindow(days_, days ? SW_SHOW : SW_HIDE);
    ShowWindow(summary_, reports ? SW_SHOW : SW_HIDE);
}

void MainWindow::applyTab() {
    updateTabControls();
    reload();
}

void MainWindow::reload() {
    if (!database_.isConnected()) {
        setStatus(L"Нет подключения к PostgreSQL.");
        return;
    }

    setStatus(L"Загрузка...");
    SendMessageW(list_, WM_SETREDRAW, FALSE, 0);
    rowColors_.clear();
    ListView_DeleteAllItems(list_);
    while (Header_GetItemCount(ListView_GetHeader(list_)) > 0) {
        ListView_DeleteColumn(list_, 0);
    }

    try {
        switch (activeTab_) {
            case Tab::Products: {
                setColumns({{L"Артикул", 100},
                            {L"Название", 230},
                            {L"Категория", 150},
                            {L"Цена", 110},
                            {L"НДС", 75},
                            {L"Остаток", 95},
                            {L"Ближайший срок", 120},
                            {L"Статус", 135}});
                const auto products = service_.listProducts();
                for (const auto& product : products) {
                    const std::wstring stock =
                        formatInt(product.stockQuantity) + L" " +
                        utf8ToWide(product.unit);
                    addRow({utf8ToWide(product.sku), utf8ToWide(product.name),
                            utf8ToWide(product.category),
                            formatMoney(product.salePriceCents),
                            formatPercent(product.taxRateBp), stock,
                            product.nearestExpiry.empty()
                                ? L"—"
                                : utf8ToWide(formatDateRu(product.nearestExpiry)),
                            statusRu(product.status)},
                           statusColor(product.status));
                }
                setStatus(L"Товаров: " + std::to_wstring(products.size()) +
                          L" · " + (database_.isConnected() ? L"подключено" : L"нет связи"));
                break;
            }
            case Tab::Supplies: {
                setColumns({{L"Дата", 135},       {L"Поставщик", 180},
                            {L"Накладная", 105},  {L"Товар", 210},
                            {L"Кол-во", 70},      {L"Остаток", 75},
                            {L"Закупка", 100},    {L"Сумма", 105},
                            {L"Срок", 100}});
                const auto rows = service_.listSupplies();
                for (const auto& row : rows) {
                    addRow({utf8ToWide(row.suppliedAt), utf8ToWide(row.supplier),
                            utf8ToWide(row.invoiceNo), utf8ToWide(row.product),
                            formatInt(row.quantity), formatInt(row.remaining),
                            formatMoney(row.unitCostCents),
                            formatMoney(row.totalCostCents),
                            utf8ToWide(row.expiryDate)});
                }
                setStatus(L"Поступлений: " + std::to_wstring(rows.size()));
                break;
            }
            case Tab::Sales: {
                setColumns({{L"Дата", 135},      {L"Товар", 220},
                            {L"Кол-во", 70},     {L"Цена", 100},
                            {L"Выручка", 105},   {L"НДС", 90},
                            {L"Себестоимость", 120},
                            {L"Прибыль", 105}});
                const auto rows = service_.listSales();
                for (const auto& row : rows) {
                    addRow({utf8ToWide(row.soldAt), utf8ToWide(row.product),
                            formatInt(row.quantity),
                            formatMoney(row.unitPriceCents),
                            formatMoney(row.revenueCents), formatMoney(row.taxCents),
                            formatMoney(row.costCents),
                            formatMoney(row.profitCents)},
                           row.profitCents < 0 ? RGB(176, 42, 42) : 0);
                }
                setStatus(L"Продаж: " + std::to_wstring(rows.size()));
                break;
            }
            case Tab::WriteOffs: {
                setColumns({{L"Дата", 135},       {L"Товар", 220},
                            {L"Кол-во", 70},      {L"Причина", 125},
                            {L"Себестоимость", 115},
                            {L"Срок партии", 105}, {L"Комментарий", 220}});
                const auto rows = service_.listWriteOffs();
                for (const auto& row : rows) {
                    addRow({utf8ToWide(row.writtenOffAt), utf8ToWide(row.product),
                            formatInt(row.quantity), reasonRu(row.reason),
                            formatMoney(row.costCents),
                            utf8ToWide(row.batchExpiry), utf8ToWide(row.comment)},
                           row.reason == "EXPIRED" ? RGB(176, 42, 42) : 0);
                }
                setStatus(L"Списаний: " + std::to_wstring(rows.size()));
                break;
            }
            case Tab::Reports:
                generateReport();
                break;
        }
    } catch (const std::exception& error) {
        showError(window_, error);
        setStatus(L"Ошибка загрузки данных.");
    }

    SendMessageW(list_, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(list_, nullptr, TRUE);
}

void MainWindow::generateReport() {
    const int kind = reportKind();
    if (kind == 2) {
        const auto daysText = [this]() {
            wchar_t buffer[32]{};
            GetWindowTextW(days_, buffer, 32);
            return std::wstring(buffer);
        }();
        const auto days = parseInt(daysText);
        if (!days) {
            showInfo(window_, L"Укажите количество дней.");
            return;
        }
        const auto rows = service_.expiringSoon(*days);
        setColumns({{L"Товар", 220},      {L"Поставщик", 180},
                    {L"Партия", 105},      {L"Кол-во", 70},
                    {L"Срок годности", 115},
                    {L"Дней осталось", 105},
                    {L"Закупка", 100}});
        int total = 0;
        for (const auto& row : rows) {
            total += row.quantity;
            addRow({utf8ToWide(row.product), utf8ToWide(row.supplier),
                    utf8ToWide(row.batchReceivedAt), formatInt(row.quantity),
                    utf8ToWide(formatDateRu(row.expiryDate)),
                    formatInt(row.daysLeft), formatMoney(row.unitCostCents)},
                   row.daysLeft <= 2 ? RGB(190, 105, 0) : 0);
        }
        SetWindowTextW(summary_,
                       (L"Позиций: " + std::to_wstring(rows.size()) +
                        L" · единиц: " + formatInt(total))
                           .c_str());
        setStatus(L"Товары, которые скоро испортятся: " +
                  std::to_wstring(rows.size()));
        return;
    }

    const std::string from = reportFrom();
    const std::string to = reportTo();
    if (kind == 0) {
        const auto report = service_.profitReport(from, to);
        setColumns({{L"Товар", 220},          {L"Кол-во", 70},
                    {L"Выручка", 115},        {L"НДС", 100},
                    {L"Себестоимость", 125},  {L"Прибыль", 115},
                    {L"Прибыль/ед.", 115}});
        for (const auto& row : report.rows) {
            addRow({utf8ToWide(row.product), formatInt(row.quantity),
                    formatMoney(row.revenueCents), formatMoney(row.taxCents),
                    formatMoney(row.costCents), formatMoney(row.profitCents),
                    formatMoney(row.profitPerUnitCents)},
                   row.profitCents < 0 ? RGB(176, 42, 42) : 0);
        }
        SetWindowTextW(
            summary_,
            (L"Итого: продано " + formatInt(report.totals.quantity) + L" ед. · "
             L"выручка " + formatMoney(report.totals.revenueCents) + L" · НДС " +
             formatMoney(report.totals.taxCents) + L" · себестоимость " +
             formatMoney(report.totals.costCents) + L" · прибыль " +
             formatMoney(report.totals.profitCents))
                .c_str());
        setStatus(L"Период: " + utf8ToWide(formatDateRu(from)) + L" — " +
                  utf8ToWide(formatDateRu(to)) + L" · товаров: " +
                  std::to_wstring(report.rows.size()));
        return;
    }

    const auto rows = service_.expiredWriteOffs(from, to);
    setColumns({{L"Дата", 135},        {L"Товар", 220},
                {L"Кол-во", 70},       {L"Себестоимость", 125},
                {L"Срок партии", 110}, {L"Комментарий", 230}});
    Cents totalCost = 0;
    for (const auto& row : rows) {
        totalCost += row.costCents;
        addRow({utf8ToWide(row.writtenOffAt), utf8ToWide(row.product),
                formatInt(row.quantity), formatMoney(row.costCents),
                utf8ToWide(row.batchExpiry), utf8ToWide(row.comment)},
               RGB(176, 42, 42));
    }
    SetWindowTextW(summary_,
                   (L"Списано по истечении срока: " + formatInt(
                        std::accumulate(rows.begin(), rows.end(), 0,
                                        [](int sum, const ExpiredWriteOffRow& row) {
                                            return sum + row.quantity;
                                        })) +
                    L" ед. · себестоимость " + formatMoney(totalCost))
                       .c_str());
    setStatus(L"Просроченные списания за период: " + std::to_wstring(rows.size()));
}

void MainWindow::primaryAction() {
    bool changed = false;
    try {
        switch (activeTab_) {
            case Tab::Products:
                changed = showProductDialog(window_, service_);
                break;
            case Tab::Supplies:
                changed = showSupplyDialog(window_, service_);
                break;
            case Tab::Sales:
                changed = showSaleDialog(window_, service_);
                break;
            case Tab::WriteOffs:
                changed = showWriteOffDialog(window_, service_);
                break;
            case Tab::Reports:
                generateReport();
                return;
        }
    } catch (const std::exception& error) {
        showError(window_, error);
        return;
    }
    if (changed) {
        reload();
    }
}

void MainWindow::openSettings() {
    DbConfig updated = config_;
    if (!showConnectionDialog(window_, updated, "")) {
        return;
    }

    try {
        database_.connect(updated);
        config_ = updated;
        ConfigStore::save(config_);
        updateConnectionStatus();
        applyTab();
    } catch (const std::exception& error) {
        showError(window_, error);
    }
}

void MainWindow::showAbout() {
    MessageBoxW(
        window_,
        L"Складской учёт\n"
        L"Вариант 3: учёт поступлений, продаж и списаний товара.\n\n"
        L"C++20 · Win32 API · PostgreSQL (libpq)\n\n"
        L"Прибыль рассчитывается как выручка минус НДС и себестоимость.\n"
        L"Себестоимость определяется по партиям (FEFO).",
        L"О программе", MB_OK | MB_ICONINFORMATION);
}

std::string MainWindow::reportFrom() const {
    return dateControlIso(window_, IDC_REPORT_FROM);
}

std::string MainWindow::reportTo() const {
    return dateControlIso(window_, IDC_REPORT_TO);
}

int MainWindow::reportKind() const {
    const int selected =
        static_cast<int>(SendMessageW(reportType_, CB_GETCURSEL, 0, 0));
    return selected < 0 ? 0 : selected;
}

void MainWindow::setColumns(
    const std::vector<std::pair<std::wstring, int>>& columns) {
    ListView_DeleteAllItems(list_);
    while (Header_GetItemCount(ListView_GetHeader(list_)) > 0) {
        ListView_DeleteColumn(list_, 0);
    }
    for (std::size_t index = 0; index < columns.size(); ++index) {
        LVCOLUMNW column{};
        column.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
        column.fmt = LVCFMT_LEFT;
        column.cx = columns[index].second;
        column.pszText =
            const_cast<wchar_t*>(columns[index].first.c_str());
        ListView_InsertColumn(list_, static_cast<int>(index), &column);
    }
}

void MainWindow::addRow(const std::vector<std::wstring>& values, COLORREF color) {
    if (values.empty()) {
        return;
    }
    const int index = ListView_GetItemCount(list_);
    LVITEMW item{};
    item.mask = LVIF_TEXT;
    item.iItem = index;
    item.pszText = const_cast<wchar_t*>(values[0].c_str());
    ListView_InsertItem(list_, &item);
    for (std::size_t column = 1; column < values.size(); ++column) {
        ListView_SetItemText(list_, index, static_cast<int>(column),
                             const_cast<wchar_t*>(values[column].c_str()));
    }
    rowColors_.push_back(color);
}

void MainWindow::setStatus(const std::wstring& text) {
    if (status_ != nullptr) {
        SetWindowTextW(status_, text.c_str());
    }
}

}  // namespace warehouse
