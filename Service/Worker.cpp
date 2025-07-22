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
        // 2. 对本批次的观测数据按时间戳排序
        std::sort(currentMeasurements.begin(), currentMeasurements.end(),
                  [](const Measurement& a, const Measurement& b) {
            return a.timestamp < b.timestamp;
        });

        // 3. 逐个处理排好序的观测数据
        for (const auto& meas : currentMeasurements) {
            // 首先，将所有航迹预测到当前观测数据的时间戳
            m_trackManager->predictTo(meas.timestamp);

            // 然后，用这个单独的观测数据去更新航迹
            m_trackManager->processMeasurements({meas});
        }
    }

    // 4. 定时输出跟踪和预测结果，并将确认航迹打包成JSON发送
    auto tracks = m_trackManager->getTracks();

    // --- 新增: 准备用于发送的根JSON对象 ---
    json outputJson;
    outputJson["timestamp"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate).toStdString();
    outputJson["tracks"] = json::array(); // 创建一个空的JSON数组来存放航迹

    //qInfo() << "================ 周期报告 ================";
    //qInfo() << "当前活动航迹数:" << tracks.size();

    for (const auto& track : tracks) {
        if (track->isConfirmed()) {
            StateVector state = track->getState();
            Vector3 pos = state.head<3>();
            Vector3 vel = state.tail<3>();

            // --- 日志打印功能 (保持不变) ---
            //qDebug().nospace() << "  -> [航迹 ID: " << track->getId() << "] "
            //                               << "位置:(" << QString::number(pos.x(), 'f', 1) << "," << QString::number(pos.y(), 'f', 1) << "," << QString::number(pos.z(), 'f', 1) << ") "
            //                               << "速度:(" << QString::number(vel.x(), 'f', 1) << "," << QString::number(vel.y(), 'f', 1) << "," << QString::number(vel.z(), 'f', 1) << ") "
            //                               << "命中次数:" << track->getHits();

            std::vector<Vector3> future = track->predictFutureTrajectory(2.0, 0.5);
            QString futurePathStr = "未来路径 (5秒): ";
            for(const auto& p : future) {
                futurePathStr += QString("->(%1,%2,%3)").arg(p.x(), 0, 'f', 0).arg(p.y(), 0, 'f', 0).arg(p.z(), 0, 'f', 0);
            }
            //qDebug() << "     " << futurePathStr;
            // --- 日志结束 ---

            // --- 新增: 构建单个航迹的JSON对象 ---
            json trackJson;
            trackJson["id"] = track->getId();
            trackJson["hits"] = track->getHits();
            trackJson["position"] = { {"x", pos.x()}, {"y", pos.y()}, {"z", pos.z()} };
            trackJson["velocity"] = { {"x", vel.x()}, {"y", vel.y()}, {"z", vel.z()} };

            json futurePathJson = json::array();
            for(const auto& p : future) {
                futurePathJson.push_back({ {"x", p.x()}, {"y", p.y()}, {"z", p.z()} });
            }
            trackJson["future_trajectory"] = futurePathJson;

            // 将当前航迹的JSON对象添加到根对象的数组中
            outputJson["tracks"].push_back(trackJson);
        }
    }
    //qInfo() << "=============================================";

    // --- 新增: 检查并发送JSON数据 ---
    if (!outputJson["tracks"].empty()) {
        try {
            // 将JSON对象序列化为字符串
            std::string jsonData = outputJson.dump();

            // 通过全局管理器发送数据
            g_MessageManager.sendMessage(jsonData);

            qInfo()<<"outputJson " <<QString::fromStdString(jsonData);

            //qInfo() << "通过 MessageManager 发送了" << outputJson["tracks"].size() << "条已确认航迹。";
        } catch (const json::exception& e) {
            qCritical() << "序列化要发送的航迹JSON失败: " << e.what();
        }
    }

    m_lastHeartbeat = QDateTime::currentDateTimeUtc();
    emit heartbeat(m_lastHeartbeat);
}
