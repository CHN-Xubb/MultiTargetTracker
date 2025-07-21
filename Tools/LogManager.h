#ifndef LOGMANAGER_H
#define LOGMANAGER_H

#include <QDebug>
#include <QObject>
#include <QFile>
#include <QMutex>
#include <QString>

// 定义日志文件轮转的默认配置
const qint64 DEFAULT_MAX_FILE_SIZE = 10 * 1024 * 1024; // 10 MB
const int DEFAULT_MAX_FILE_COUNT = 5;                  // 最多 5 个日志文件

class LogManager {
public:
    // 获取单例实例
    static LogManager& instance();

    // 安装日志管理器
    void install();

    // 卸载日志管理器（恢复默认行为）
    void uninstall();

    // --- 配置方法 ---
    // 设置日志文件的最大大小（单位：字节）
    void setMaxFileSize(qint64 size);

    // 设置日志文件的最大数量
    void setMaxFileCount(int count);

    // 设置日志存储目录
    void setLogDirectory(const QString& dir);

private:
    // 私有化构造函数和析构函数，以实现单例模式
    LogManager();
    ~LogManager();

    // 删除拷贝构造和赋值操作
    LogManager(const LogManager&) = delete;
    LogManager& operator=(const LogManager&) = delete;

    // 自定义的 Qt 消息处理函数
    static void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg);

    // 将消息写入文件的实际操作
    void writeToFile(const QString& message);

    // 执行文件轮转
    void rotateFiles();

private:
    // 用于保护文件写入操作的互斥锁
    QMutex m_mutex;

    // 当前打开的日志文件指针
    QFile* m_logFile = nullptr;

    // 日志配置
    qint64 m_maxFileSize;
    int m_maxFileCount;
    QString m_logDirectory;
    QString m_logBaseName; // 日志文件的基础名称，如 "app.log"

    // 保存原始的 Qt 消息处理器
    QtMessageHandler m_oldHandler = nullptr;
};

#endif // LOGMANAGER_H
