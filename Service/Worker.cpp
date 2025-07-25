#include "Worker.h"
#include <QTime>
#include <QThread>
#include <QSettings>
#include "LogManager.h"
#include "nlohmann/json.hpp"
#include "MessageRelayManager.h"
#include <algorithm>

using json = nlohmann::json;

Worker::Worker(QObject *parent)
    : QObject(parent), m_timer(nullptr), m_running(false)
{

    qRegisterMetaType<std::string>("std::string");

    QSettings settings("Server.ini", QSettings::IniFormat);
    m_interval = settings.value("General/workerInterval", 100).toInt();

    m_trackManager = std::make_unique<TrackManager>();

    m_lastHeartbeat = QDateTime::currentDateTimeUtc();

    connect(&g_MessageManager, &MessageRelayManager::messageReceived, this, &Worker::onMessageReceived);
}

Worker::~Worker() {}


void Worker::doWork()
{
    qInfo() << "工作线程已在线程中启动: " << (quintptr)QThread::currentThreadId() << ", 间隔: " << m_interval << "毫秒.";
    m_running = true;

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &Worker::onTimeout);
    m_timer->start(m_interval);
}

void Worker::stopWork()
{
    qInfo() << "正在停止工作线程...";
    if (m_timer && m_timer->isActive()) {
        m_timer->stop();
    }
    m_running = false;
    qInfo() << "工作线程已停止。";
    QThread::currentThread()->quit();
}

void Worker::onMessageReceived(const std::string& message)
{
    if (!m_running) return;

    try {
        // 1. 解析JSON字符串
        json data = json::parse(message);

        if(!data.contains("ObserverId"))
        {
            return;
        }

        // 2. 访问数据
        // 使用 .at() 方法访问，如果键不存在会抛出异常，更安全
        int observerId = data.at("ObserverId");
        double timestamp = data.at("Timestamp");

        // 访问嵌套对象
        const json& position = data.at("Position");
        double x = position.at("x");
        double y = position.at("y");
        double z = position.at("z");

        Measurement m{Vector3(x,y,z),timestamp,observerId};

        QMutexLocker locker(&m_bufferMutex);
        m_measurementBuffer.push_back(m);


    } catch (json::exception& e) {
        qCritical() << "JSON 处理错误: " << e.what();
    }
}


void Worker::onTimeout()
{
    if (!m_running) return;

    // 1. 从缓冲区取出本周期的所有观测数据
    std::vector<Measurement> currentMeasurements;
    {
        QMutexLocker locker(&m_bufferMutex);
        if (!m_measurementBuffer.empty()) {
            currentMeasurements.swap(m_measurementBuffer);
        }
    }

    // 如果有数据，则进行处理
    if (!currentMeasurements.empty()) {
        // 2. 对本批次的观测数据按时间戳排序，确保时间顺序正确
        std::sort(currentMeasurements.begin(), currentMeasurements.end(),
                  [](const Measurement& a, const Measurement& b) {
            return a.timestamp < b.timestamp;
        });

        // ========================[核心修改部分开始]========================

        // 3. (新) 一次性将所有航迹预测到本批次最新的时间戳
        // 这是至关重要的第一步。它将所有航迹的状态统一更新到一个共同的时间点，
        // 为后续的批量数据关联做好了准备。
        double latestTimestamp = currentMeasurements.back().timestamp;
        m_trackManager->predictTo(latestTimestamp);

        // 4. (新) 用本周期的所有观测数据，一次性更新所有航迹
        // 将整个观测数据批次传递给TrackManager。TrackManager内部的数据关联、
        // 更新、创建和删除逻辑将一次性完成，避免了在Worker层进行高开销的循环。
        m_trackManager->processMeasurements(currentMeasurements);

        // ========================[核心修改部分结束]========================
    }

    // 5. 定时输出跟踪和预测结果，并将确认航迹打包成JSON发送
    auto tracks = m_trackManager->getTracks();

    json outputJson;
    outputJson["timestamp"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate).toStdString();
    outputJson["tracks"] = json::array();

    for (const auto& track : tracks) {
        if (track->isConfirmed()) {
            StateVector state = track->getState();
            Vector3 pos = state.head<3>();
            Vector3 vel = state.segment<3>(3); // 注意：匀加速模型中，速度在中间3个维度

            json trackJson;
            trackJson["id"] = track->getId();
            trackJson["hits"] = track->getHits();
            trackJson["position"] = { {"x", pos.x()}, {"y", pos.y()}, {"z", pos.z()} };
            trackJson["velocity"] = { {"x", vel.x()}, {"y", vel.y()}, {"z", vel.z()} };

            std::vector<Vector3> future = track->predictFutureTrajectory(2.0, 0.5);
            json futurePathJson = json::array();
            for(const auto& p : future) {
                futurePathJson.push_back({ {"x", p.x()}, {"y", p.y()}, {"z", p.z()} });
            }
            trackJson["future_trajectory"] = futurePathJson;

            outputJson["tracks"].push_back(trackJson);
        }
    }

    if (!outputJson["tracks"].empty()) {
        try {
            std::string jsonData = outputJson.dump();
            g_MessageManager.sendMessage(jsonData);
            qInfo()<<"outputJson " <<QString::fromStdString(jsonData);
        } catch (const json::exception& e) {
            qCritical() << "序列化要发送的航迹JSON失败: " << e.what();
        }
    }

    m_lastHeartbeat = QDateTime::currentDateTimeUtc();
    emit heartbeat(m_lastHeartbeat);
}
