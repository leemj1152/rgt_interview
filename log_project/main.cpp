#include "LogFileManager.hpp"
#include <iostream>
#include <windows.h>

int main() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    try {
        LogFileManager manager;
        manager.openLogFile("error.log");
        manager.openLogFile("debug.log");
        manager.openLogFile("info.log");

        manager.writeLog("error.log", "Database connection failed");
        manager.writeLog("debug.log", "User login attempt");
        manager.writeLog("info.log", "Server started successfully");

        auto errorLogs = manager.readLogs("error.log");

        std::cout << u8"// error.log 파일 내용\n";
        for (auto& line : manager.readLogs("error.log")) std::cout << line << "\n";

        std::cout << u8"\n// debug.log 파일 내용\n";
        for (auto& line : manager.readLogs("debug.log")) std::cout << line << "\n";

        std::cout << u8"\n// info.log 파일 내용\n";
        for (auto& line : manager.readLogs("info.log")) std::cout << line << "\n";

        std::cout << u8"\n// readLogs 반환값\n";
        std::cout << "errorLogs[0] = '" << errorLogs[0] << "'\n";
    } catch (const std::exception& e) {
        std::cerr << u8"[예외] " << e.what() << "\n";
        return 1;
    }
}
