/**
 * @file LogManager.h
 * @brief 日志管理器头文件
 * @details 定义了日志管理器类，提供统一的日志管理功能
 * @author xubb
 * @date 20250711
 */

#ifndef LOGMANAGER_H
#define LOGMANAGER_H

#include <QDebug>
#include <QObject>
#include <QFile>
#include <QMutex>
#include <QString>
#include <QMap>

/**
 * @brief 日志管理器类
 * @details 提供统一的日志管理功能，支持日志文件轮转、日志级别控制等
 *          使用单例模式确保全局只有一个日志管理器实例
 */
class LogManager {
public:
    /**
     * @brief 获取日志管理器单例实例
     * @return 日志管理器实例的引用
     */
    static LogManager& instance();

    /**
     * @brief 安装日志管理器
     * @details 接管Qt的日志输出
     */
    void install();

    /**
     * @brief 卸载日志管理器
     * @details 恢复Qt默认的日志处理
     */
    void uninstall();

    // ========== 配置接口 ==========

    /**
     * @brief 设置日志文件的最大大小
     * @param size 文件大小（字节）
     */
    void setMaxFileSize(qint64 size);

    /**
     * @brief 设置保留的日志文件最大数量
     * @param count 文件数量
     */
    void setMaxFileCount(int count);

    /**
     * @brief 设置日志文件存储目录
     * @param dir 目录路径
     */
    void setLogDirectory(const QString& dir);

    // ========== 日志级别控制接口 ==========

    /**
     * @brief 设置指定日志级别是否启用
     * @param type 日志级别
     * @param enabled true表示启用，false表示禁用
     */
    void setLogLevelEnabled(QtMsgType type, bool enabled);

    /**
     * @brief 查询指定日志级别是否启用
     * @param type 日志级别
     * @return true表示启用，false表示禁用
     */
    bool isLogLevelEnabled(QtMsgType type) const;

    /**
     * @brief 启用所有日志级别
     */
    void enableAllLogLevels();

    /**
     * @brief 禁用所有日志级别
     */
    void disableAllLogLevels();

    /**
     * @brief 设置是否将日志输出到控制台
     * @param enabled true表示输出到控制台，false表示不输出
     */
    void setConsoleOutputEnabled(bool enabled);

    /**
     * @brief 设置是否将日志输出到文件
     * @param enabled true表示输出到文件，false表示不输出
     */
    void setFileOutputEnabled(bool enabled);

private:
    /**
     * @brief 私有构造函数
     * @details 确保单例模式
     */
    LogManager();

    /**
     * @brief 私有析构函数
     */
    ~LogManager();

    /**
     * @brief 禁用拷贝构造函数
     */
    LogManager(const LogManager&) = delete;

    /**
     * @brief 禁用赋值操作符
     */
    LogManager& operator=(const LogManager&) = delete;

    /**
     * @brief Qt消息处理函数
     * @param type 消息类型
     * @param context 消息上下文
     * @param msg 消息内容
     */
    static void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg);

    /**
     * @brief 将消息写入日志文件
     * @param message 格式化后的消息
     */
    void writeToFile(const QString& message);

    /**
     * @brief 执行日志文件轮转
     * @details 当日志文件大小超过限制时，执行轮转操作
     */
    void rotateFiles();

private:
    /**
     * @brief 线程安全互斥锁
     */
    mutable QMutex m_mutex;

    /**
     * @brief 当前日志文件
     */
    QFile* m_logFile = nullptr;

    /**
     * @brief 最大文件大小
     * @details 单个日志文件的最大字节数
     */
    qint64 m_maxFileSize;

    /**
     * @brief 最大文件数量
     * @details 保留的历史日志文件最大数量
     */
    int m_maxFileCount;

    /**
     * @brief 日志目录
     * @details 日志文件存储的目录路径
     */
    QString m_logDirectory;

    /**
     * @brief 日志文件基础名称
     */
    QString m_logBaseName;

    /**
     * @brief 原始消息处理器
     */
    QtMessageHandler m_oldHandler = nullptr;

    /**
     * @brief 日志级别控制映射表
     * @details 存储各日志级别是否启用的状态
     */
    QMap<QtMsgType, bool> m_logLevelEnabled;

    /**
     * @brief 控制台输出控制
     * @details 是否将日志输出到控制台
     */
    bool m_consoleOutputEnabled = true;

    /**
     * @brief 文件输出控制
     * @details 是否将日志输出到文件
     */
    bool m_fileOutputEnabled = true;
};

#endif // LOGMANAGER_H
