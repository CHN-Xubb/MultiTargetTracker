#ifndef SERVICE_H
#define SERVICE_H

#include <QObject>
#include <QThread>
#include "Worker.h"


class HealthCheckServer;

class Service : public QObject
{
    Q_OBJECT
public:
    explicit Service(QObject *parent = nullptr);
    ~Service();

    Worker* getWorker() const;
    bool isWorkerThreadRunning() const;

public slots:
    // 启动服务
    void start();
    // 停止服务（优雅退出）
    void stop();

private:
    void initConfig();
    void initLogging();
    void initWorkerThread();

    QThread m_workerThread;
    Worker* m_worker;
    HealthCheckServer* m_healthCheckServer;
};

#endif // SERVICE_H
