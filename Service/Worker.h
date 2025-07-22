#ifndef WORKER_H
#define WORKER_H

#include <QObject>
#include <QTimer>
#include <QDateTime>
#include "TrackManager.h"
#include <memory>
#include <vector>
#include "DataStructures.h"

class Worker : public QObject
{
    Q_OBJECT
public:
    explicit Worker(QObject *parent = nullptr);
    ~Worker();

signals:
    void heartbeat(const QDateTime& lastHeartbeat);

public slots:
    void doWork();
    void stopWork();

private slots:
    void onTimeout();
    void onMessageReceived(const std::string& message);

private:
    QTimer *m_timer;
    bool m_running;
    int m_interval;

    // 使用智能指针管理 TrackManager
    std::unique_ptr<TrackManager> m_trackManager;

    // 观测数据缓冲区
    std::vector<Measurement> m_measurementBuffer;
    QMutex m_bufferMutex;
    QDateTime m_lastHeartbeat;
};

#endif // WORKER_H
