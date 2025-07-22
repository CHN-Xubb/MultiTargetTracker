#include "Service.h"
#include "HealthCheckServer.h"
#include <QCoreApplication>
#include <QSettings>
#include <csignal>
#include <QDir>
#include "LogManager.h"

Service::Service(int argc, char **argv)
    : QtService<QCoreApplication>(argc, argv, "MultiTargetTrackerService"),
      m_worker(nullptr),
      m_isServiceRunning(false)
{
    // 设置服务的描述信息
    setServiceDescription("MultiTargetTrackerService");
    // 设置服务的启动类型
    // QtServiceController::AutoStartup: 系统启动时自动运行 (最常用)
    // QtServiceController::ManualStartup: 手动启动
    setStartupType(QtServiceController::AutoStartup);

}

Service::~Service()
{
    // 确保线程在析构前已停止
    if (m_workerThread.isRunning()) {
        m_workerThread.quit();
        m_workerThread.wait(3000); // 等待最多3秒
    }
}

QDateTime Service::getLastWorkerHeartbeat() const
{
    QMutexLocker locker(&m_heartbeatMutex);
    return m_lastWorkerHeartbeat;
}

bool Service::isWorkerThreadRunning() const
{
    return m_isServiceRunning && m_workerThread.isRunning();
}

void Service::onWorkerHeartbeat(const QDateTime &lastHeartbeat)
{
    QMutexLocker locker(&m_heartbeatMutex);
    m_lastWorkerHeartbeat = lastHeartbeat;
}

void Service::initConfig()
{
    // 构建配置文件的绝对路径
    QString configPath = QDir(QCoreApplication::applicationDirPath()).filePath("Server.ini");
    qInfo() << "从以下路径加载配置:" << configPath;

    QSettings settings(configPath, QSettings::IniFormat); // 使用绝对路径
    if (!QFile::exists(configPath)) {
        qInfo() << "未找到 Server.ini。正在创建默认配置文件。";

        // 通用配置
        settings.setValue("General/workerInterval", 100);

        // 健康检查配置
        settings.setValue("HealthCheck/port", 8899);

        // 卡尔曼滤波器与航迹管理配置
        settings.beginGroup("KalmanFilter");
        settings.setValue("processNoiseStd", 0.1);
        settings.setValue("processNoiseStd_CA", 1.0); // CA模型的过程噪声（jerk）
        settings.setValue("measurementNoiseStd", 2.0);
        settings.setValue("initialPositionUncertainty", 2.0);
        settings.setValue("initialVelocityUncertainty", 1.0);
        settings.setValue("initialAccelerationUncertainty", 10.0); // CA模型的初始加速度不确定性
        settings.setValue("associationGateDistance", 10.0);
        settings.setValue("newTrackGateDistance", 5.0);
        settings.setValue("confirmationHits", 3);
        settings.setValue("maxMissesToDelete", 5);

        settings.endGroup();
    }
}

void Service::initLogging()
{
    LogManager::instance().install();
    LogManager::instance().setMaxFileSize(5*1024*1024); // 5 MB
    LogManager::instance().setMaxFileCount(3);
}

void Service::initWorkerThread()
{
    m_worker = new Worker();
    m_worker->moveToThread(&m_workerThread);

    //心跳检查
    QObject::connect(m_worker, &Worker::heartbeat, this, &Service::onWorkerHeartbeat);

    // 当线程启动时，开始执行Worker的doWork
    connect(&m_workerThread, &QThread::started, m_worker, &Worker::doWork);

    // 允许Worker通过调用quit()来停止线程事件循环
    connect(m_worker, &QObject::destroyed, &m_workerThread, &QThread::quit);
}

void Service::start()
{
    QCoreApplication *app = application();
    app->setApplicationVersion("V1.0");

    initLogging();

    if (QDir::setCurrent(QCoreApplication::applicationDirPath()))
    {
        qInfo() << "已将工作目录设置为:" << QCoreApplication::applicationDirPath();
    }
    else
    {
        qCritical() << "错误：无法将工作目录设置为应用程序目录！";
        return;
    }

    initConfig();

    qInfo() << "================ 服务启动中 ================";

    try {

        // 1. 初始化工作线程
        initWorkerThread();

        // 2. 初始化并启动健康检查服务器
        m_healthCheckServer = new HealthCheckServer(this, this);
        QString configPath = QCoreApplication::applicationDirPath() + "/Server.ini";
        QSettings settings(configPath, QSettings::IniFormat);

        quint16 port = settings.value("HealthCheck/port", 8899).toUInt();

        if (!m_healthCheckServer->startListen(port))
        {
            qCritical() << "健康检查服务器启动失败，端口号" << port;
            return;
        }

        qInfo() << "健康检查服务器正在监听端口" << port;

        // 3. 启动工作线程
        m_workerThread.start();

        m_isServiceRunning = true;

        qInfo() << "================ 服务启动成功 ================";

        return;
    }
    catch (const std::exception& e)
    {
        qCritical() << "服务启动时发生严重错误: " << e.what();
        return ;
    }
    catch (...)
    {
        qCritical() << "服务启动时发生未知的严重错误。";
        return;
    }
}

void Service::stop()
{
    qInfo() << "================ 正在停止服务 ================";

    // 停止健康检查服务器
    if (m_healthCheckServer)
    {
        m_healthCheckServer->stopListen();
    }

    // 停止工作线程
    if (m_workerThread.isRunning())
    {
        // 使用 QueuedConnection 调用 stopWork 来确保在正确的线程中执行
        QMetaObject::invokeMethod(m_worker, "stopWork", Qt::QueuedConnection);

        // 等待线程优雅地退出
        if (!m_workerThread.wait(5000))
        { // 等待最多5秒
            qWarning() << "工作线程在5秒内没有正常退出，将尝试强制终止。";
            m_workerThread.terminate();
            m_workerThread.wait(); // 等待终止完成
        }
    }

    m_isServiceRunning = false;

    qInfo() << "================ 服务已停止 ================";

    return ;
}
