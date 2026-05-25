#pragma once

#include <QString>
#include <QDir>
#include <chrono>
#include <fstream>
#include <mutex>
#include <string>
#include <vector>

namespace bgtc {

enum class LogLevel { Debug, Info, Warning, Error };

struct LogEntry {
    std::int64_t timestampMs = 0;
    LogLevel level = LogLevel::Info;
    std::string category;
    std::string playerId;
    std::string roomId;
    std::string battleId;
    std::string message;
};

class Logger {
public:
    static Logger& instance();

    void init(const QString& logDir = {}, const QString& playerId = {});
    void shutdown();

    void log(LogLevel level, const QString& category, const QString& message);
    void debug(const QString& category, const QString& message);
    void info(const QString& category, const QString& message);
    void warn(const QString& category, const QString& message);
    void error(const QString& category, const QString& message);

    void setPlayerId(const QString& id);
    void setRoomId(const QString& id);
    void setBattleId(const QString& id);

    [[nodiscard]] QString exportLog() const;
    bool exportToFile(const QString& path) const;
    [[nodiscard]] QString currentLogPath() const;

private:
    Logger() = default;
    ~Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    std::string formatEntry(const LogEntry& entry) const;
    void writeLine(const std::string& line);

    mutable std::mutex mutex_;
    std::ofstream file_;
    QString logDir_;
    QString logPath_;
    QString currentPlayerId_;
    QString currentRoomId_;
    QString currentBattleId_;
    std::vector<LogEntry> buffer_;
    static constexpr size_t kMaxBuffer = 1000;
};

}  // namespace bgtc
