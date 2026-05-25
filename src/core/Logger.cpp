#include "core/Logger.h"

#include <QDateTime>
#include <QStandardPaths>
#include <QDir>

namespace bgtc {

Logger& Logger::instance() {
    static Logger logger;
    return logger;
}

Logger::~Logger() {
    shutdown();
}

void Logger::init(const QString& logDir, const QString& playerId) {
    std::lock_guard<std::mutex> lock(mutex_);
    currentPlayerId_ = playerId;

    if (logDir.isEmpty()) {
        logDir_ = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/logs";
    } else {
        logDir_ = logDir;
    }

    QDir().mkpath(logDir_);

    const auto date = QDateTime::currentDateTime().toString("yyyy-MM-dd");
    logPath_ = logDir_ + "/tank-client-" + date + ".log";

    file_.open(logPath_.toStdString(), std::ios::out | std::ios::app);
    if (file_.is_open()) {
        info("core", "Logger initialized: " + logPath_);
    }
}

void Logger::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (file_.is_open()) {
        info("core", "Logger shutting down");
        file_.close();
    }
}

void Logger::log(LogLevel level, const QString& category, const QString& message) {
    LogEntry entry;
    entry.timestampMs = QDateTime::currentMSecsSinceEpoch();
    entry.level = level;
    entry.category = category.toStdString();
    entry.playerId = currentPlayerId_.toStdString();
    entry.roomId = currentRoomId_.toStdString();
    entry.battleId = currentBattleId_.toStdString();
    entry.message = message.toStdString();

    const auto line = formatEntry(entry);

    {
        std::lock_guard<std::mutex> lock(mutex_);
        buffer_.push_back(entry);
        if (buffer_.size() > kMaxBuffer) {
            buffer_.erase(buffer_.begin());
        }
        if (file_.is_open()) {
            file_ << line << std::endl;
            file_.flush();
        }
    }
}

void Logger::debug(const QString& category, const QString& message) {
    log(LogLevel::Debug, category, message);
}

void Logger::info(const QString& category, const QString& message) {
    log(LogLevel::Info, category, message);
}

void Logger::warn(const QString& category, const QString& message) {
    log(LogLevel::Warning, category, message);
}

void Logger::error(const QString& category, const QString& message) {
    log(LogLevel::Error, category, message);
}

void Logger::setPlayerId(const QString& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    currentPlayerId_ = id;
}

void Logger::setRoomId(const QString& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    currentRoomId_ = id;
}

void Logger::setBattleId(const QString& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    currentBattleId_ = id;
}

QString Logger::exportLog() const {
    std::lock_guard<std::mutex> lock(mutex_);
    QString result;
    for (const auto& entry : buffer_) {
        result += QString::fromStdString(formatEntry(entry)) + "\n";
    }
    return result;
}

bool Logger::exportToFile(const QString& path) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ofstream out(path.toStdString(), std::ios::out | std::ios::trunc);
    if (!out.is_open()) {
        return false;
    }
    for (const auto& entry : buffer_) {
        out << formatEntry(entry) << "\n";
    }
    return true;
}

QString Logger::currentLogPath() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return logPath_;
}

std::string Logger::formatEntry(const LogEntry& entry) const {
    const auto dt = QDateTime::fromMSecsSinceEpoch(entry.timestampMs);
    const auto timestamp = dt.toString("yyyy-MM-dd hh:mm:ss.zzz").toStdString();

    const char* levelStr = "INFO";
    switch (entry.level) {
        case LogLevel::Debug:   levelStr = "DEBUG"; break;
        case LogLevel::Info:    levelStr = "INFO";  break;
        case LogLevel::Warning: levelStr = "WARN";  break;
        case LogLevel::Error:   levelStr = "ERROR"; break;
    }

    std::string result = "[" + timestamp + "] [" + levelStr + "] [" + entry.category + "]";
    if (!entry.playerId.empty())  result += " [player:" + entry.playerId + "]";
    if (!entry.roomId.empty())    result += " [room:" + entry.roomId + "]";
    if (!entry.battleId.empty())  result += " [battle:" + entry.battleId + "]";
    result += " " + entry.message;
    return result;
}

void Logger::writeLine(const std::string& line) {
    if (file_.is_open()) {
        file_ << line << std::endl;
    }
}

}  // namespace bgtc
