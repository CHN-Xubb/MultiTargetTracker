#ifndef SERVICE_H
#define SERVICE_H

#include "qtservice.h"
#include <QObject>
#include <QThread>
#include "Worker.h"

class HealthCheckServer;

class Service : public QtService<QCoreApplication>, public QObject
{
public:
    explicit Service(int argc, char **argv);
    ~Service() override;

    QDateTime getLastWorkerHeartbeat() const;

    bool isWorkerThreadRunning() const;

public slots:
    void onWorkerHeartbeat(const QDateTime& lastHeartbeat);


protected:
    void start()override;
    void stop()override;
    void pause()override{}
    void resume()override{}

private:
    void initConfig();
    void initLogging();
    void initWorkerThread();

    QThread m_workerThread;

    Worker* m_worker;

    HealthCheckServer* m_healthCheckServer;
    QDateTime m_lastWorkerHeartbeat;
    mutable QMutex m_heartbeatMutex;

    bool m_isServiceRunning; // 用于跟踪服务状态
};

#endif // SERVICE_H
