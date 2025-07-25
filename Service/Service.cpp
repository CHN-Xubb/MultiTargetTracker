/**
 * @file Service.cpp
 * @brief 服务类的实现文件
 * @details 实现了Service类的各种功能，包括服务的启动、停止和工作线程管理
 * @author xubb
 * @date 20250711
 */

#include "Service.h"
#include "HealthCheckServer.h"
#include <QCoreApplication>
#include <QSettings>
#include <csignal>
#include <QDir>
#include "LogManager.h"

// 定义统一的日志宏，与现有LogManager配合使用
#define LOG_DEBUG(msg) qDebug() << "[Service::" << __FUNCTION__ << "] " << msg
#define LOG_INFO(msg) qInfo() << "[Service::" << __FUNCTION__ << "] " << msg
#define LOG_WARN(msg) qWarning() << "[Service::" << __FUNCTION__ << "] " << msg
#define LOG_ERROR(msg) qCritical() << "[Service::" << __FUNCTION__ << "] " << msg

#define LOG_FUNCTION_BEGIN() LOG_DEBUG("开始")
#define LOG_FUNCTION_END() LOG_DEBUG("结束")


/**
 * @brief 构造函数
 * @param argc 命令行参数数量
 * @param argv 命令行参数数组
 * @details 初始化服务，设置服务描述和启动类型
 */
Service::Service(int argc, char **argv)
    : QtService<QCoreApplication>(argc, argv, "MultiTargetTrackerService"),
      m_worker(nullptr),
      m_isServiceRunning(false)
{
    // 设置服务的描述信息
    setServiceDescription("MultiTargetTrackerService");
    // 设置服务的启动类型
    setStartupType(QtServiceController::AutoStartup);
}

/**
 * @brief 析构函数
 * @details 确保在对象销毁前停止工作线程
 */
Service::~Service()
{
    // 确保线程在析构前已停止
    if (m_workerThread.isRunning()) {
        LOG_INFO("正在停止工作线程");
        m_workerThread.quit();
        if (m_workerThread.wait(3000)) { // 等待最多3秒
            LOG_INFO("工作线程已正常停止");
        } else {
            LOG_WARN("工作线程无法在3秒内停止");
        }
    }
    LOG_INFO("服务析构完成");
}

/**
 * @brief 获取工作线程的最后心跳时间
 * @return 最后一次心跳的时间
 * @details 线程安全地获取工作线程的最后心跳时间
 */
QDateTime Service::getLastWorkerHeartbeat() const
{
    QMutexLocker locker(&m_heartbeatMutex);
    return m_lastWorkerHeartbeat;
}

/**
 * @brief 检查工作线程是否正在运行
 * @return 如果工作线程正在运行则返回true，否则返回false
 * @details 检查服务运行状态和工作线程运行状态
 */
bool Service::isWorkerThreadRunning() const
{
    return m_isServiceRunning && m_workerThread.isRunning();
}

/**
 * @brief 工作线程心跳响应槽函数
 * @param lastHeartbeat 心跳时间
 * @details 更新工作线程的最后心跳时间
 */
void Service::onWorkerHeartbeat(const QDateTime &lastHeartbeat)
{
    QMutexLocker locker(&m_heartbeatMutex);
    m_lastWorkerHeartbeat = lastHeartbeat;
    LOG_DEBUG("收到工作线程心跳: " + lastHeartbeat.toString("yyyy-MM-dd hh:mm:ss.zzz"));
}

/**
 * @brief 初始化配置
 * @details 读取或创建应用程序的配置文件
 */
void Service::initConfig()
{
    LOG_FUNCTION_BEGIN();

    // 构建配置文件的绝对路径
    QString configPath = QDir(QCoreApplication::applicationDirPath()).filePath("Server.ini");
    qWarning()<<"配置文件路径: " + configPath;

    QSettings settings(configPath, QSettings::IniFormat); // 使用绝对路径
    if (!QFile::exists(configPath)) {
        qWarning("未找到配置文件，创建默认配置");

        // 通用配置
        settings.setValue("General/workerInterval", 100);
        LOG_DEBUG("设置 General/workerInterval = 100");

        // 健康检查配置
        settings.setValue("HealthCheck/port", 8899);
        LOG_DEBUG("设置 HealthCheck/port = 8899");

        // 卡尔曼滤波器与航迹管理配置
        settings.beginGroup("KalmanFilter");
        settings.setValue("processNoiseStd", 0.1);
        settings.setValue("measurementNoiseStd", 0.1);
        settings.setValue("initialPositionUncertainty", 2.0);
        settings.setValue("initialVelocityUncertainty", 1.0);
        settings.setValue("initialAccelerationUncertainty", 10.0);
        settings.setValue("associationGateDistance", 10.0);
        settings.setValue("newTrackGateDistance", 5.0);
        settings.setValue("confirmationHits", 3);
        settings.setValue("maxMissesToDelete", 5);
        LOG_DEBUG("完成卡尔曼滤波器默认配置设置");
        settings.endGroup();

        LOG_INFO("默认配置文件创建完成");
    } else {
        LOG_INFO("成功加载已有配置文件");
    }

    LOG_FUNCTION_END();
}

/**
 * @brief 初始化日志系统
 * @details 配置日志管理器的各项参数
 */
void Service::initLogging()
{
    LogManager::instance().install();
    LogManager::instance().setMaxFileSize(5*1024*1024); // 5 MB


    LogManager::instance().setMaxFileCount(3); // 文件数量限制: 3

    LogManager::instance().setLogLevelEnabled(QtDebugMsg, false);
    LogManager::instance().setLogLevelEnabled(QtInfoMsg, false);

//    LogManager::instance().setConsoleOutputEnabled(false);
//    LogManager::instance().setFileOutputEnabled(false);

}

/**
 * @brief 初始化工作线程
 * @details 创建工作对象，设置信号槽连接，准备工作线程
 */
void Service::initWorkerThread()
{
    LOG_FUNCTION_BEGIN();

    m_worker = new Worker();
    LOG_DEBUG("工作对象已创建");

    m_worker->moveToThread(&m_workerThread);
    LOG_DEBUG("工作对象已移动到工作线程");

    // 心跳检查
    QObject::connect(m_worker, &Worker::heartbeat, this, &Service::onWorkerHeartbeat);
    LOG_DEBUG("心跳信号已连接");

    // 当线程启动时，开始执行Worker的doWork
    connect(&m_workerThread, &QThread::started, m_worker, &Worker::doWork);
    LOG_DEBUG("线程启动信号已连接");

    // 当线程结束时，清理m_worker
    connect(&m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);
    LOG_DEBUG("线程结束信号已连接");

    LOG_INFO("工作线程初始化完成");
    LOG_FUNCTION_END();
}

/**
 * @brief 启动服务
 * @details 初始化配置，日志系统，工作线程和健康检查服务器
 */
void Service::start()
{
    initLogging();

    QCoreApplication *app = application();
    app->setApplicationVersion("V1.0");

    LOG_FUNCTION_BEGIN();
    LOG_INFO("================== 服务启动 ==================");
    LOG_INFO("应用版本: V1.0");


    if (QDir::setCurrent(QCoreApplication::applicationDirPath()))
    {
        LOG_INFO("工作目录: " + QCoreApplication::applicationDirPath());
    }
    else
    {
        LOG_ERROR("无法设置工作目录！");
        return;
    }

    initConfig();

    try {
        // 1. 初始化工作线程
        LOG_INFO("【阶段1】初始化工作线程");
        initWorkerThread();

        // 2. 初始化并启动健康检查服务器
        LOG_INFO("【阶段2】初始化健康检查服务器");
        m_healthCheckServer = new HealthCheckServer(this, this);
        QString configPath = QCoreApplication::applicationDirPath() + "/Server.ini";
        QSettings settings(configPath, QSettings::IniFormat);

        quint16 port = settings.value("HealthCheck/port", 8899).toUInt();
        LOG_DEBUG("健康检查服务器端口: " + QString::number(port));

        if (!m_healthCheckServer->startListen(port))
        {
            LOG_ERROR("健康检查服务器启动失败，端口: " + QString::number(port));
            return;
        }

        LOG_INFO("健康检查服务器已启动，端口: " + QString::number(port));

        // 3. 启动工作线程
        LOG_INFO("【阶段3】启动工作线程");
        m_workerThread.start();
        LOG_INFO("工作线程已启动");

        m_isServiceRunning = true;

        LOG_INFO("================== 服务启动成功 ==================");
        LOG_FUNCTION_END();
        return;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("启动异常: " + QString(e.what()));
        return;
    }
    catch (...)
    {
        LOG_ERROR("启动发生未知异常");
        return;
    }
}

/**
 * @brief 停止服务
 * @details 停止健康检查服务器和工作线程
 */
void Service::stop()
{
    LOG_FUNCTION_BEGIN();
    LOG_INFO("================== 服务停止 ==================");

    if (m_healthCheckServer)
    {
        LOG_INFO("【阶段1】停止健康检查服务器");
        m_healthCheckServer->stopListen();
        LOG_INFO("健康检查服务器已停止");
    }

    // 工作线程应自行处理关闭。我们只需请求退出并等待。
    if (m_workerThread.isRunning())
    {
        LOG_INFO("【阶段2】停止工作线程");
        // Worker的stopWork()方法应该调用其所在线程事件循环的quit()。
        // 使用invokeMethod确保该调用在工作线程中被正确地排队和执行。
        QMetaObject::invokeMethod(m_worker, "stopWork", Qt::QueuedConnection);
        LOG_DEBUG("已请求工作线程停止");

        // 等待线程正常结束
        if (!m_workerThread.wait(10000)) // 将超时增加到10秒以提高稳定性
        {
            LOG_WARN("工作线程在10秒内没有正常退出");
        } else {
            LOG_INFO("工作线程已正常退出");
        }
    }

    m_isServiceRunning = false;
    LOG_INFO("================== 服务停止完成 ==================");
    LOG_FUNCTION_END();
}
