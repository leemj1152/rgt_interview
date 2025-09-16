#pragma once
#include <unordered_map>
#include <memory>
#include <fstream>
#include <string>
#include <vector>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <filesystem> // for create_directories

class LogFileManager {
public:
    LogFileManager() = default;

    // 파일 핸들 관리 특성상 복사는 금지, 이동은 허용
    LogFileManager(const LogFileManager&)            = delete;
    LogFileManager& operator=(const LogFileManager&) = delete;
    LogFileManager(LogFileManager&&) noexcept        = default;
    LogFileManager& operator=(LogFileManager&&) noexcept = default;

    ~LogFileManager() { closeAll(); }

    // logs/ 폴더에 파일을 연다 (없으면 폴더 생성)
    void openLogFile(const std::string& filename) {
        std::filesystem::create_directories("logs");
        const std::string fullpath = makePath(filename);

        if (writers_.find(fullpath) != writers_.end()) return;

        auto ofs = std::make_unique<std::ofstream>();
        ofs->open(fullpath, std::ios::out | std::ios::app);
        if (!ofs->is_open()) {
            throw std::runtime_error("Failed to open file: " + fullpath);
        }
        writers_.emplace(fullpath, std::move(ofs));
    }

    // 타임스탬프를 붙여 기록
    void writeLog(const std::string& filename, const std::string& message) {
        const std::string fullpath = makePath(filename);
        auto it = writers_.find(fullpath);
        if (it == writers_.end()) {
            throw std::runtime_error("File is not open: " + fullpath);
        }
        std::ofstream& out = *(it->second);
        if (!out.good()) {
            throw std::runtime_error("Bad output stream: " + fullpath);
        }
        out << "[" << nowTimestamp() << "] " << message << '\n';
        out.flush();
        if (!out.good()) {
            throw std::runtime_error("Write/flush failed: " + fullpath);
        }
    }

    // 파일 전체를 줄 단위로 읽어 반환
    std::vector<std::string> readLogs(const std::string& filename) const {
        const std::string fullpath = makePath(filename);
        std::ifstream in(fullpath);
        if (!in.is_open()) {
            throw std::runtime_error("Failed to open file for read: " + fullpath);
        }
        std::vector<std::string> lines;
        std::string line;
        while (std::getline(in, line)) {
            lines.push_back(line);
        }
        if (in.bad()) {
            throw std::runtime_error("Read failed: " + fullpath);
        }
        return lines;
    }

    // 개별 파일 닫기
    void closeLogFile(const std::string& filename) {
        const std::string fullpath = makePath(filename);
        auto it = writers_.find(fullpath);
        if (it != writers_.end()) {
            writers_.erase(it); // unique_ptr 파괴 → 파일 자동 close
        }
    }

private:
    std::unordered_map<std::string, std::unique_ptr<std::ofstream>> writers_;

    static std::string nowTimestamp() {
        using clock = std::chrono::system_clock;
        auto now = clock::now();
        std::time_t tt = clock::to_time_t(now);
        std::tm tm{};
        #if defined(_WIN32)
            localtime_s(&tm, &tt);
        #else
            tm = *std::localtime(&tt);
        #endif
        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
        return oss.str();
    }

    static std::string makePath(const std::string& filename) {
        return std::string("logs/") + filename;
    }

    void closeAll() {
        writers_.clear(); // 모든 ofstream 정리
    }
};
