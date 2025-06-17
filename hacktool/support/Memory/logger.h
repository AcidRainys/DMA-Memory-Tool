#pragma once
#include <iostream>
#include <fstream>
#include <sstream>
#include <mutex>
#include <memory>
#include <chrono>
#include <iomanip>
#include <filesystem>
#include <cstdarg> 
#pragma warning( disable : 4996 )

enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERRORR
};

class Logger {
public:
    // ��ȡ����ʵ��
    static Logger& getInstance() {
        static Logger instance;
        return instance;
    }

    // ��ʼ����־ϵͳ
    void init(LogLevel level = LogLevel::INFO,
        const std::string& filename = "") {
        std::lock_guard<std::mutex> lock(mutex_);
        logLevel_ = level;
        if (!filename.empty()) {
            fileStream_.open(filename, std::ios::out | std::ios::app);
            if (!fileStream_.is_open()) {
                throw std::runtime_error("Failed to open log file: " + filename);
            }
        }
    }

    // ������־����
    void setLogLevel(LogLevel level) {
        std::lock_guard<std::mutex> lock(mutex_);
        logLevel_ = level;
    }

    void enableColor(bool enable) {
        std::lock_guard<std::mutex> lock(mutex_);
        colorEnabled_ = enable;
    }

    // ������־�ӿ� - �ַ����汾
    void log(LogLevel level, const std::string& message,
        const std::string& file = "", int line = -1) {
        if (level < logLevel_) return;
        logImpl(level, message, file, line);
    }

    // ������־�ӿ� - C�ַ����汾
    void log(LogLevel level, const char* message,
        const char* file = "", int line = -1) {
        if (level < logLevel_) return;
        logImpl(level, std::string(message), std::string(file), line);
    }

    // printf����ʽ���汾
    template<typename... Args>
    void log(LogLevel level, const char* format, Args... args) {
        if (level < logLevel_) return;

        char buffer[1024];
        snprintf(buffer, sizeof(buffer), format, args...);
        logImpl(level, std::string(buffer), "", -1);
    }

    // printf�����ļ������к�
    template<typename... Args>
    void log(LogLevel level, const char* format, const char* file, int line, Args... args) {
        if (level < logLevel_) return;

        char buffer[1024];
        snprintf(buffer, sizeof(buffer), format, args...);
        logImpl(level, std::string(buffer), std::string(file), line);
    }
    void clearConsole() {
        std::lock_guard<std::mutex> lock(mutex_);

#if defined(_WIN32) || defined(_WIN64)
        system("cls");  // Windowsϵͳ
#else
        system("clear");  // Linux/Macϵͳ
#endif
    }
private:
    LogLevel logLevel_;
    std::ofstream fileStream_;
    std::mutex mutex_;
    bool colorEnabled_ = true;

    Logger() : logLevel_(LogLevel::INFO) {}  // ˽�й��캯��
    ~Logger() {
        if (fileStream_.is_open()) {
            fileStream_.close();
        }
    }

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    // ʵ�ʵ���־ʵ��
    void logImpl(LogLevel level,
        const std::string& message,
        const std::string& file,
        int line) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);

        // ����������־��Ϣ�������ļ������
        std::stringstream fileSS;
        fileSS << "[" << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");

        // ��Ӻ���
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        fileSS << "." << std::setfill('0') << std::setw(3) << ms.count() << "] ";

        // �����־����
        fileSS << "[" << levelToString(level) << "] ";

        // ���Դ����λ��
        if (!file.empty() && line != -1) {
            fileSS << "[" << std::filesystem::path(file).filename().string()
                << ":" << line << "] ";
        }

        // �����־��Ϣ
        fileSS << message << std::endl;

        // ������ļ�
        if (fileStream_.is_open()) {
            fileStream_ << fileSS.str();
            fileStream_.flush();
        }

        // ������ɫ��־��Ϣ�����ڿ���̨�����
        if (colorEnabled_) {
            std::stringstream consoleSS;

            // ʱ������֣���ɫ��
            consoleSS << "\033[90m["
                << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S")
                << "." << std::setfill('0') << std::setw(3) << ms.count()
                << "]\033[0m ";

            // ��־���𲿷֣���ͬ��ɫ��
            consoleSS << getColorCode(level) << "[" << levelToString(level) << "]\033[0m ";

            // Դ����λ�ò��֣���ɫ��
            if (!file.empty() && line != -1) {
                consoleSS << "\033[36m["
                    << std::filesystem::path(file).filename().string()
                    << ":" << line << "]\033[0m ";
            }

            // ��Ϣ���ݣ����ݼ�����ɫ��
            consoleSS << getColorCode(level) << message << "\033[0m" << std::endl;

            std::cout << consoleSS.str();
        }
        else {
            std::cout << fileSS.str();
        }
    }

    std::string levelToString(LogLevel level) {
        switch (level) {
        case LogLevel::DEBUG:   return "DEBUG";
        case LogLevel::INFO:    return "INFO";
        case LogLevel::WARNING:  return "WARNING";
        case LogLevel::ERRORR:    return "ERROR";
        default:                return "UNKNOWN";
        }
    }

    std::string getColorCode(LogLevel level) {
        switch (level) {
        case LogLevel::DEBUG:   return "\033[32m"; // ��ɫ
        case LogLevel::INFO:    return "\033[34m"; // ��ɫ
        case LogLevel::WARNING: return "\033[33m"; // ��ɫ
        case LogLevel::ERRORR:   return "\033[31m"; // ��ɫ
        default:                return "\033[0m";
        }
    }
};

// �޸ĺ궨��
#define LOG_DEBUG(message, ...)   Logger::getInstance().log(LogLevel::DEBUG, message, ##__VA_ARGS__)
#define LOG_INFO(message, ...)    Logger::getInstance().log(LogLevel::INFO, message, ##__VA_ARGS__)
#define LOG_WARN(message, ...)    Logger::getInstance().log(LogLevel::WARNING, message, ##__VA_ARGS__)
#define LOG_ERROR(message, ...)   Logger::getInstance().log(LogLevel::ERRORR, message, ##__VA_ARGS__)
#define LOG_CLEAR() Logger::getInstance().clearConsole()