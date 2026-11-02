#pragma once

#include <windows.h>

#include <string>
#include <vector>

#include "db/Database.hpp"
#include "service/WarehouseService.hpp"
#include "util/Config.hpp"

namespace warehouse {

class MainWindow {
public:
    MainWindow(HINSTANCE instance, Database& database, WarehouseService& service,
               DbConfig& config);
    ~MainWindow();

    MainWindow(const MainWindow&) = delete;
    MainWindow& operator=(const MainWindow&) = delete;

    bool create(int showCommand);
    int run();

private:
    enum class Tab {
        Products = 0,
        Supplies = 1,
        Sales = 2,
        WriteOffs = 3,
        Reports = 4,
    };

    static LRESULT CALLBACK WndProc(HWND window, UINT message, WPARAM wParam,
                                    LPARAM lParam);
    LRESULT handleMessage(UINT message, WPARAM wParam, LPARAM lParam);

    void createControls();
    void createFonts();
    void layoutControls();
    void updateConnectionStatus();
    void updateTabControls();
    void updateReportVisibility();
    void applyTab();
    void reload();
    void clearRows();
    void setColumns(const std::vector<std::pair<std::wstring, int>>& columns);
    void addRow(const std::vector<std::wstring>& values, COLORREF color = 0);
    void setStatus(const std::wstring& text);

    void primaryAction();
    void generateReport();
    void openSettings();
    void showAbout();

    std::string reportFrom() const;
    std::string reportTo() const;
    int reportKind() const;

    HINSTANCE instance_ = nullptr;
    HWND window_ = nullptr;
    Database& database_;
    WarehouseService& service_;
    DbConfig& config_;

    HWND title_ = nullptr;
    HWND subtitle_ = nullptr;
    HWND tab_ = nullptr;
    HWND primaryButton_ = nullptr;
    HWND refreshButton_ = nullptr;
    HWND list_ = nullptr;
    HWND status_ = nullptr;

    HWND reportType_ = nullptr;
    HWND reportFromLabel_ = nullptr;
    HWND reportFrom_ = nullptr;
    HWND reportToLabel_ = nullptr;
    HWND reportTo_ = nullptr;
    HWND daysLabel_ = nullptr;
    HWND days_ = nullptr;
    HWND summary_ = nullptr;

    HFONT font_ = nullptr;
    HFONT fontBold_ = nullptr;
    HFONT fontTitle_ = nullptr;

    Tab activeTab_ = Tab::Products;
    std::vector<COLORREF> rowColors_;
};

}  // namespace warehouse
