#include "Service.h"
#include "HealthCheckServer.h"
#include <QCoreApplication>
#include <QSettings>
#include <csignal>
#include <QDir>
#include "LogManager.h"


// 用于捕获系统信号的静态函数
void handleSignal(int signal)
{
    if (signal == SIGINT || signal == SIGTERM) {
        // 触发Qt应用的退出流程
        QCoreApplication::quit();
    }
}

Service::Service(QObject *parent) : QObject(parent), m_worker(nullptr)
{
    initLogging();
    initConfig();
    initWorkerThread();

    m_healthCheckServer = new HealthCheckServer(this, this);

    // 连接应用的退出信号到服务的stop槽，实现优雅退出
    connect(qApp, &QCoreApplication::aboutToQuit, this, &Service::stop);

    // 注册信号处理器
    signal(SIGINT, handleSignal);  // Ctrl+C
    signal(SIGTERM, handleSignal); // kill 命令
}

Service::~Service()
{
    // 确保线程在析构前已停止
    if (m_workerThread.isRunning()) {
        m_workerThread.quit();
        m_workerThread.wait(3000); // 等待最多3秒
    }
}

Worker* Service::getWorker() const
{
    return m_worker;
}

bool Service::isWorkerThreadRunning() const
{
    return m_workerThread.isRunning();
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
    // --- 安装和配置日志管理器 ---
    // 1. 安装日志管理器（必须在所有日志调用之前）
    LogManager::instance().install();

    // 2. (可选) 自定义配置
    // 为了方便测试，我们将文件大小限制为 5MB，文件数量为 3
    LogManager::instance().setMaxFileSize(5*1024*1024); // 5 MB
    LogManager::instance().setMaxFileCount(3);
    // LogManager::instance().setLogDirectory("C:/my_app_logs"); // 也可以指定一个绝对路径
    // qFatal("这是一个致命错误，应用程序将终止。"); // 取消注释会使程序中止

    qInfo()<<"================ 服务启动中 ================";
}

void Service::initWorkerThread()
{
    m_worker = new Worker();
    m_worker->moveToThread(&m_workerThread);

    // 当线程启动时，开始执行Worker的doWork
    connect(&m_workerThread, &QThread::started, m_worker, &Worker::doWork);

    // 允许Worker通过调用quit()来停止线程事件循环
    connect(m_worker, &QObject::destroyed, &m_workerThread, &QThread::quit);
}

void Service::start()
{
    qInfo()<<"正在启动服务...";
    m_workerThread.start();

    QSettings settings("Server.ini", QSettings::IniFormat);
    quint16 port = settings.value("HealthCheck/port", 8899).toUInt();
    if (!m_healthCheckServer->startListen(port)) {
        qCritical() << "健康检查服务器启动失败，端口号" << port;
    } else {
        qInfo() << "健康检查服务器正在监听端口" << port;
    }

    qInfo()<<"服务启动成功。";
}

void Service::stop()
{
    qInfo()<<"正在停止服务...";

    m_healthCheckServer->stopListen();

    // 如果线程正在运行，则请求停止
    if (m_workerThread.isRunning()) {
        // 调用Worker的stopWork槽来安全停止
        QMetaObject::invokeMethod(m_worker, "stopWork", Qt::QueuedConnection);
        // 等待线程处理完所有事件并退出
        m_workerThread.wait(5000); // 等待最多5秒
    }
    qInfo()<<"================ 服务已停止 ================";
}
