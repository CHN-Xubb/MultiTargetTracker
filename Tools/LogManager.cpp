/**
 * @file LogManager.cpp
 * @brief 日志管理器实现文件
 * @details 实现了LogManager类的各种功能，包括日志文件管理、消息处理等
 * @author xubb
 * @date 20250711
 */

#include "LogManager.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QTextStream>
#include <iostream>

/**
 * @brief 默认最大文件大小常量
 * @details 默认单个日志文件的最大大小为10MB
 */
const qint64 DEFAULT_MAX_FILE_SIZE = 10 * 1024 * 1024; // 10 MB

/**
 * @brief 默认最大文件数量常量
 * @details 默认最多保留5个历史日志文件
 */
const int DEFAULT_MAX_FILE_COUNT = 5;

/**
 * @brief 获取日志管理器单例实例
 * @return 日志管理器实例的引用
 */
LogManager& LogManager::instance() {
    static LogManager instance;
    return instance;
}

/**
 * @brief 构造函数
 * @details 初始化默认配置
 */
LogManager::LogManager() {
    // 初始化文件配置
    m_maxFileSize = DEFAULT_MAX_FILE_SIZE;
    m_maxFileCount = DEFAULT_MAX_FILE_COUNT;

    // 设置默认日志目录
    m_logDirectory = QCoreApplication::applicationDirPath() + "/logs";

    // 设置默认日志文件名
    m_logBaseName = QCoreApplication::applicationName() + ".log";
    if (m_logBaseName.isEmpty()) {
        m_logBaseName = "application.log";
    }

    // 默认启用所有日志级别
    m_logLevelEnabled[QtDebugMsg] = true;
    m_logLevelEnabled[QtInfoMsg] = true;
    m_logLevelEnabled[QtWarningMsg] = true;
    m_logLevelEnabled[QtCriticalMsg] = true;
    m_logLevelEnabled[QtFatalMsg] = true;
}

/**
 * @brief 析构函数
 * @details 清理资源
 */
LogManager::~LogManager() {
    if (m_logFile && m_logFile->isOpen()) {
        m_logFile->close();
        delete m_logFile;
    }
}

/**
 * @brief 安装日志管理器
 * @details 确保日志目录存在，并接管Qt的日志输出
 */
void LogManager::install() {
    // 确保日志目录存在
    QDir dir(m_logDirectory);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    // 保存原始消息处理器并安装自定义处理器
    m_oldHandler = qInstallMessageHandler(LogManager::messageHandler);
}

/**
 * @brief 卸载日志管理器
 * @details 恢复Qt默认的日志处理
 */
void LogManager::uninstall() {
    qInstallMessageHandler(m_oldHandler);
    m_oldHandler = nullptr;
}

/**
 * @brief 设置最大文件大小
 * @param size 文件大小（字节）
 */
void LogManager::setMaxFileSize(qint64 size) {
    QMutexLocker locker(&m_mutex);
    m_maxFileSize = size;
}

/**
 * @brief 设置最大文件数量
 * @param count 文件数量
 */
void LogManager::setMaxFileCount(int count) {
    QMutexLocker locker(&m_mutex);
    m_maxFileCount = count;
}

/**
 * @brief 设置日志目录
 * @param dir 目录路径
 */
void LogManager::setLogDirectory(const QString& dir) {
    QMutexLocker locker(&m_mutex);
    m_logDirectory = dir;
}

/**
 * @brief 设置日志级别是否启用
 * @param type 日志级别
 * @param enabled true表示启用，false表示禁用
 */
void LogManager::setLogLevelEnabled(QtMsgType type, bool enabled) {
    QMutexLocker locker(&m_mutex);
    m_logLevelEnabled[type] = enabled;
}

/**
 * @brief 查询日志级别是否启用
 * @param type 日志级别
 * @return true表示启用，false表示禁用
 */
bool LogManager::isLogLevelEnabled(QtMsgType type) const {
    QMutexLocker locker(&m_mutex);
    return m_logLevelEnabled.value(type, true);
}

/**
 * @brief 启用所有日志级别
 */
void LogManager::enableAllLogLevels() {
    QMutexLocker locker(&m_mutex);
    for (auto it = m_logLevelEnabled.begin(); it != m_logLevelEnabled.end(); ++it) {
        it.value() = true;
    }
}

/**
 * @brief 禁用所有日志级别
 */
void LogManager::disableAllLogLevels() {
    QMutexLocker locker(&m_mutex);
    for (auto it = m_logLevelEnabled.begin(); it != m_logLevelEnabled.end(); ++it) {
        it.value() = false;
    }
}

/**
 * @brief 设置是否输出到控制台
 * @param enabled true表示输出到控制台，false表示不输出
 */
void LogManager::setConsoleOutputEnabled(bool enabled) {
    QMutexLocker locker(&m_mutex);
    m_consoleOutputEnabled = enabled;
}

/**
 * @brief 设置是否输出到文件
 * @param enabled true表示输出到文件，false表示不输出
 */
void LogManager::setFileOutputEnabled(bool enabled) {
    QMutexLocker locker(&m_mutex);
    m_fileOutputEnabled = enabled;
}

/**
 * @brief 消息处理函数
 * @param type 消息类型
 * @param context 消息上下文
 * @param msg 消息内容
 * @details 根据消息类型和设置，将日志消息格式化并输出到控制台和/或文件
 */
void LogManager::messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg) {
    // 获取单例实例
    LogManager& self = LogManager::instance();

    // 检查该日志级别是否被禁用
    if (!self.isLogLevelEnabled(type)) {
        return;
    }

    // 格式化日志级别字符串
    QString level;
    switch (type) {
        case QtDebugMsg:    level = "DEBUG"; break;
        case QtInfoMsg:     level = "INFO";  break;
        case QtWarningMsg:  level = "WARN";  break;
        case QtCriticalMsg: level = "CRIT";  break;
        case QtFatalMsg:    level = "FATAL"; break;
    }

    // 格式化日志消息
    QString formattedMessage = QString("[%1] [%2] %3\n")
                               .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz"))
                               .arg(level)
                               .arg(msg);

    // 输出到控制台（如果启用）
    if (self.m_consoleOutputEnabled) {
        fprintf(stderr, "%s", formattedMessage.toLocal8Bit().constData());
        fflush(stderr);
    }

    // 输出到文件（如果启用）
    if (self.m_fileOutputEnabled) {
        self.writeToFile(formattedMessage);
    }

    // 处理致命错误
    if (type == QtFatalMsg) {
        abort();
    }
}

/**
 * @brief 将消息写入文件
 * @param message 格式化后的消息
 * @details 将消息写入日志文件，并在必要时执行文件轮转
 */
void LogManager::writeToFile(const QString& message) {
    // 使用互斥锁保证线程安全
    QMutexLocker locker(&m_mutex);

    // 如果文件未打开则尝试打开
    if (m_logFile == nullptr) {
        m_logFile = new QFile(m_logDirectory + "/" + m_logBaseName);
        if (!m_logFile->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            // 打开失败，输出错误信息
            std::cerr << "Failed to open log file: " << m_logFile->fileName().toStdString() << std::endl;
            delete m_logFile;
            m_logFile = nullptr;
            return;
        }
    }

    // 检查文件大小是否超限
    if (m_logFile->size() > m_maxFileSize) {
        rotateFiles();
        // 轮转后重新打开文件
        if (m_logFile == nullptr) {
            m_logFile = new QFile(m_logDirectory + "/" + m_logBaseName);
            if (!m_logFile->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
                std::cerr << "Failed to open new log file after rotation." << std::endl;
                delete m_logFile;
                m_logFile = nullptr;
                return;
            }
        }
    }

    // 写入日志消息
    QTextStream out(m_logFile);
    out << message;
    out.flush(); // 立即刷新到磁盘
}

/**
 * @brief 执行日志文件轮转
 * @details 关闭当前日志文件，将旧文件重命名，然后创建新的日志文件
 */
void LogManager::rotateFiles() {
    // 关闭当前日志文件
    if (m_logFile) {
        m_logFile->close();
        delete m_logFile;
        m_logFile = nullptr;
    }

    QString baseLogPath = m_logDirectory + "/" + m_logBaseName;

    // 删除最旧的日志文件
    QString oldestLogFile = baseLogPath + "." + QString::number(m_maxFileCount - 1);
    if (QFile::exists(oldestLogFile)) {
        QFile::remove(oldestLogFile);
    }

    // 重命名日志文件（从后往前）
    // 例如：log.3 -> log.4, log.2 -> log.3, log.1 -> log.2, log -> log.1
    for (int i = m_maxFileCount - 2; i >= 0; --i) {
        QString currentFile = baseLogPath;
        if (i > 0) {
            currentFile += "." + QString::number(i);
        }

        if (QFile::exists(currentFile)) {
            QString nextFile = baseLogPath + "." + QString::number(i + 1);
            QFile::rename(currentFile, nextFile);
        }
    }
}
