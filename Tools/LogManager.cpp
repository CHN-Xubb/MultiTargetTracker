#include "LogManager.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QTextStream>
#include <iostream> // 用于在 Debug 模式下输出到 stderr

LogManager& LogManager::instance() {
    static LogManager instance;
    return instance;
}

LogManager::LogManager() {
    // 初始化默认配置
    m_maxFileSize = DEFAULT_MAX_FILE_SIZE;
    m_maxFileCount = DEFAULT_MAX_FILE_COUNT;

    // 默认日志目录为应用程序可执行文件路径下的 "logs" 文件夹
    m_logDirectory = QCoreApplication::applicationDirPath() + "/logs";

    // 默认日志文件名为应用程序名加上 ".log" 后缀
    m_logBaseName = QCoreApplication::applicationName() + ".log";
    if (m_logBaseName.isEmpty()) {
        m_logBaseName = "application.log";
    }
}

LogManager::~LogManager() {
    if (m_logFile && m_logFile->isOpen()) {
        m_logFile->close();
        delete m_logFile;
    }
}

void LogManager::install() {
    // 确保日志目录存在
    QDir dir(m_logDirectory);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    // 保存旧的消息处理器并安装我们自己的
    m_oldHandler = qInstallMessageHandler(LogManager::messageHandler);
}

void LogManager::uninstall() {
    qInstallMessageHandler(m_oldHandler);
    m_oldHandler = nullptr;
}

void LogManager::setMaxFileSize(qint64 size) {
    QMutexLocker locker(&m_mutex);
    m_maxFileSize = size;
}

void LogManager::setMaxFileCount(int count) {
    QMutexLocker locker(&m_mutex);
    m_maxFileCount = count;
}

void LogManager::setLogDirectory(const QString& dir) {
    QMutexLocker locker(&m_mutex);
    m_logDirectory = dir;
}

void LogManager::messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg) {
    // 获取单例实例
    LogManager& self = LogManager::instance();

    // 格式化日志消息
    QString level;
    switch (type) {
        case QtDebugMsg:    level = "DEBUG"; break;
        case QtInfoMsg:     level = "INFO";  break;
        case QtWarningMsg:  level = "WARN";  break;
        case QtCriticalMsg: level = "CRIT";  break;
        case QtFatalMsg:    level = "FATAL"; break;
    }

    QString formattedMessage = QString("[%1] [%2] %3\n")
                               .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz"))
                               .arg(level)
                               .arg(msg);

//#if defined(QT_DEBUG)
    // 在 Debug 模式下，同时输出到控制台
    // 使用 fprintf 是因为在某些情况下（如在 Qt Creator 的控制台中），qDebug 重定向后 std::cout/cerr 可能不起作用
    fprintf(stderr, "%s", formattedMessage.toLocal8Bit().constData());
    fflush(stderr);
//#endif

    // 将格式化后的消息写入文件
    self.writeToFile(formattedMessage);

    // 如果是 Fatal 错误，写入日志后终止程序
    if (type == QtFatalMsg) {
        abort();
    }
}

void LogManager::writeToFile(const QString& message) {
    // 使用 QMutexLocker 保证线程安全
    QMutexLocker locker(&m_mutex);

    // 如果文件未打开，则尝试打开
    if (m_logFile == nullptr) {
        m_logFile = new QFile(m_logDirectory + "/" + m_logBaseName);
        if (!m_logFile->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            // 如果打开失败，打印错误并返回
            std::cerr << "Failed to open log file: " << m_logFile->fileName().toStdString() << std::endl;
            delete m_logFile;
            m_logFile = nullptr;
            return;
        }
    }

    // 在写入前检查文件大小是否超限
    if (m_logFile->size() > m_maxFileSize) {
        rotateFiles();
        // 轮转后，m_logFile 会被重置，需要重新打开
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

    // 写入日志
    QTextStream out(m_logFile);
    out << message;
    out.flush(); // 确保立即写入磁盘
}

void LogManager::rotateFiles() {
    // 1. 关闭并删除当前文件句柄
    if (m_logFile) {
        m_logFile->close();
        delete m_logFile;
        m_logFile = nullptr;
    }

    QString baseLogPath = m_logDirectory + "/" + m_logBaseName;

    // 2. 删除最旧的日志文件（如果存在）
    QString oldestLogFile = baseLogPath + "." + QString::number(m_maxFileCount - 1);
    if (QFile::exists(oldestLogFile)) {
        QFile::remove(oldestLogFile);
    }

    // 3. 重命名日志文件（从后往前）
    // 例如：log.3 -> log.4, log.2 -> log.3, ...
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
