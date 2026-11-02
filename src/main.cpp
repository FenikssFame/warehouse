#include <windows.h>
#include <commctrl.h>

#include <string>

#include "db/Database.hpp"
#include "service/WarehouseService.hpp"
#include "ui/Dialogs.hpp"
#include "ui/MainWindow.hpp"
#include "util/Config.hpp"
#include "util/Text.hpp"

#pragma comment(linker, \
    "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' " \
    "version='6.0.0.0' processorArchitecture='*' " \
    "publicKeyToken='6595b64144ccf1df' language='*'\"")

namespace {

void showFatal(const std::wstring& message) {
    MessageBoxW(nullptr, message.c_str(), L"Складской учёт",
                MB_OK | MB_ICONERROR);
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    INITCOMMONCONTROLSEX controls{};
    controls.dwSize = sizeof(controls);
    controls.dwICC = ICC_WIN95_CLASSES | ICC_DATE_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&controls);

    warehouse::DbConfig config = warehouse::ConfigStore::load();
    warehouse::Database database;
    std::string connectionError;
    bool connected = false;

    if (config.isComplete()) {
        try {
            database.connect(config);
            connected = true;
        } catch (const std::exception& error) {
            connectionError = error.what();
        }
    }

    while (!connected) {
        if (!warehouse::showConnectionDialog(nullptr, config, connectionError)) {
            return 0;
        }
        try {
            database.connect(config);
            connected = true;
        } catch (const std::exception& error) {
            connectionError = error.what();
        }
    }

    try {
        warehouse::ConfigStore::save(config);
    } catch (const std::exception& error) {
        MessageBoxW(nullptr,
                    (L"Не удалось сохранить настройки: " +
                     warehouse::errorToWide(error))
                        .c_str(),
                    L"Предупреждение", MB_OK | MB_ICONWARNING);
    }

    warehouse::WarehouseService service(database);
    warehouse::MainWindow window(instance, database, service, config);
    if (!window.create(showCommand)) {
        showFatal(L"Не удалось создать главное окно приложения.");
        return 1;
    }
    return window.run();
}
