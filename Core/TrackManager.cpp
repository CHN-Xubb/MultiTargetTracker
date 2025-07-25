/**
 * @file TrackManager.cpp
 * @brief 航迹管理器实现文件
 * @details 实现了TrackManager类的各种功能，包括数据关联、航迹创建和管理
 * @author xubb
 * @date 20250711
 */

#include "TrackManager.h"
#include "LogManager.h"
#include "ConstantVelocityModel.h"
#include "ConstantAccelerationModel.h"
#include <limits>
#include <set>
#include <QSettings>

// 定义统一的日志宏
#define LOG_DEBUG(msg) qDebug() << "[TrackManager::" << __FUNCTION__ << "] " << msg
#define LOG_INFO(msg) qInfo() << "[TrackManager::" << __FUNCTION__ << "] " << msg
#define LOG_WARN(msg) qWarning() << "[TrackManager::" << __FUNCTION__ << "] " << msg
#define LOG_ERROR(msg) qCritical() << "[TrackManager::" << __FUNCTION__ << "] " << msg

// 函数跟踪宏
#define LOG_FUNCTION_BEGIN() LOG_DEBUG("开始")
#define LOG_FUNCTION_END() LOG_DEBUG("结束")

/**
 * @brief 构造函数
 * @details 从配置文件初始化航迹管理器参数
 */
TrackManager::TrackManager()
    : m_nextTrackId(0),
      m_lastProcessTime(0.0),
      m_associationGateDistance(0.0),
      m_newTrackGateDistance(0.0)
{
    LOG_FUNCTION_BEGIN();

    // 从配置文件读取参数
    QSettings settings("Server.ini", QSettings::IniFormat);
    m_associationGateDistance = settings.value("KalmanFilter/associationGateDistance", 10.0).toDouble();
    m_newTrackGateDistance = settings.value("KalmanFilter/newTrackGateDistance", 5.0).toDouble();

    LOG_INFO("初始化完成，关联门限: " + QString::number(m_associationGateDistance) +
             "米，新航迹门限: " + QString::number(m_newTrackGateDistance) + "米");

    LOG_FUNCTION_END();
}

/**
 * @brief 析构函数
 */
TrackManager::~TrackManager()
{
    LOG_FUNCTION_BEGIN();
    LOG_INFO("航迹管理器销毁，当前航迹数: " + QString::number(m_tracks.size()));
    LOG_FUNCTION_END();
}

/**
 * @brief 处理观测数据
 * @param measurements 观测数据列表
 * @details 主处理函数，执行数据关联、航迹更新和管理
 */
void TrackManager::processMeasurements(const std::vector<Measurement>& measurements)
{
    QWriteLocker locker(&m_lock);

    if (measurements.empty()) {
        return;
    }

    LOG_DEBUG("开始处理 " + QString::number(measurements.size()) +
              " 条观测数据，当前航迹数: " + QString::number(m_tracks.size()));

    // 1. 数据关联
    std::vector<std::pair<int, int>> matches;
    std::vector<int> unmatchedTracks;
    std::vector<int> unmatchedMeasurements;
    dataAssociation(measurements, matches, unmatchedTracks, unmatchedMeasurements);

    // 2. 更新匹配的航迹
    LOG_DEBUG("开始更新 " + QString::number(matches.size()) + " 个匹配的航迹");
    updateMatchedTracks(matches, measurements);

    // 3. 为未匹配的观测创建新航迹
    LOG_DEBUG("处理 " + QString::number(unmatchedMeasurements.size()) + " 个未匹配的观测");
    createNewTracks(unmatchedMeasurements, measurements);

    // 4. 管理未匹配的航迹
    LOG_DEBUG("管理 " + QString::number(unmatchedTracks.size()) + " 个未匹配的航迹");
    manageUnmatchedTracks(unmatchedTracks);

    // 更新时间戳
    m_lastProcessTime = measurements.back().timestamp;

    LOG_DEBUG("处理完成。匹配数: " + QString::number(matches.size()) +
              "，未匹配航迹数: " + QString::number(unmatchedTracks.size()) +
              "，未匹配观测数: " + QString::number(unmatchedMeasurements.size()) +
              "，当前航迹总数: " + QString::number(m_tracks.size()));
}


/**
 * @brief 预测所有航迹状态到指定时间
 * @param timestamp 目标时间戳
 * @details 将所有航迹的状态向前预测到指定时间点
 */
void TrackManager::predictTo(double timestamp)
{
    QWriteLocker locker(&m_lock);

    // 首次调用时初始化时间戳
    if (m_lastProcessTime == 0.0) {
        m_lastProcessTime = timestamp;
        LOG_DEBUG("初始化时间戳: " + QString::number(timestamp));
        return;
    }

    double dt = timestamp - m_lastProcessTime;
    if (dt <= 0) {
        LOG_DEBUG("时间差为负或零，跳过预测: " + QString::number(dt));
        return;
    }

    LOG_DEBUG("预测 " + QString::number(m_tracks.size()) +
              " 条航迹到时间戳 " + QString::number(timestamp) +
              "，时间差: " + QString::number(dt) + " 秒");

    // 预测所有航迹
    for (const auto& pair : m_tracks) {
        TrackPtr track = pair.second;
        track->predict(dt);
    }
}

/**
 * @brief 获取当前所有航迹
 * @return 航迹指针的vector
 * @details 线程安全地获取当前所有活动航迹
 */
std::vector<TrackPtr> TrackManager::getTracks() const
{
    QWriteLocker locker(&m_lock);

    std::vector<TrackPtr> tracks;
    tracks.reserve(m_tracks.size());

    for (const auto& pair : m_tracks) {
        tracks.push_back(pair.second);
    }

    LOG_DEBUG("获取 " + QString::number(tracks.size()) + " 条航迹");
    return tracks;
}

/**
 * @brief 数据关联
 * @param measurements 观测数据列表
 * @param matches 成功匹配的航迹ID和观测索引对
 * @param unmatchedTracks 未匹配的航迹ID列表
 * @param unmatchedMeasurements 未匹配的观测索引列表
 * @details 将观测数据关联到已存在的航迹
 */
void TrackManager::dataAssociation(const std::vector<Measurement>& measurements,
                                   std::vector<std::pair<int, int>>& matches,
                                   std::vector<int>& unmatchedTracks,
                                   std::vector<int>& unmatchedMeasurements)
{
    LOG_FUNCTION_BEGIN();

    // 如果没有航迹，所有观测都是未匹配的
    if (m_tracks.empty()) {
        LOG_DEBUG("无现有航迹，所有 " + QString::number(measurements.size()) + " 条观测都标记为未匹配");
        for (size_t i = 0; i < measurements.size(); ++i) {
            unmatchedMeasurements.push_back(i);
        }
        LOG_FUNCTION_END();
        return;
    }

    std::vector<bool> meas_matched(measurements.size(), false);
    std::set<int> matched_track_ids;

    LOG_DEBUG("开始关联 " + QString::number(m_tracks.size()) + " 条航迹和 " +
              QString::number(measurements.size()) + " 个观测");

    // 对每个航迹，找到距离最近的观测点
    for (const auto& pair : m_tracks) {
        int trackId = pair.first;
        const TrackPtr& track = pair.second;

        double min_dist = std::numeric_limits<double>::max();
        int best_match_idx = -1;

        Vector3 predicted_pos = track->getState().head<3>();

        for (size_t j = 0; j < measurements.size(); ++j) {
            if (meas_matched[j]) continue;

            double dist = (predicted_pos - measurements[j].position).norm();
            LOG_DEBUG("检查航迹 " + QString::number(trackId) + " <-> 观测 " +
                      QString::number(j) + ". 距离: " + QString::number(dist, 'f', 2) + " 米");

            if (dist < min_dist) {
                min_dist = dist;
                best_match_idx = j;
            }
        }

        // 如果最近的观测点在门限内，则认为是成功匹配
        if (best_match_idx != -1 && min_dist < m_associationGateDistance) {
            matches.push_back({trackId, best_match_idx});
            meas_matched[best_match_idx] = true;
            matched_track_ids.insert(trackId);
            LOG_DEBUG("航迹 " + QString::number(trackId) + " 与观测 " +
                      QString::number(best_match_idx) + " 匹配成功，距离: " +
                      QString::number(min_dist, 'f', 2) + " 米");
        } else {
            LOG_DEBUG("航迹 " + QString::number(trackId) + " 无匹配，最近距离 " +
                      QString::number(min_dist, 'f', 2) + " 米超出门限 " +
                      QString::number(m_associationGateDistance, 'f', 2) + " 米");
        }
    }

    // 找出未匹配的航迹
    for (const auto& pair : m_tracks) {
        if (matched_track_ids.find(pair.first) == matched_track_ids.end()) {
            unmatchedTracks.push_back(pair.first);
        }
    }

    // 找出未匹配的观测
    for (size_t i = 0; i < measurements.size(); ++i) {
        if (!meas_matched[i]) {
            unmatchedMeasurements.push_back(i);
        }
    }

    LOG_DEBUG("关联完成，匹配数: " + QString::number(matches.size()) +
              "，未匹配航迹数: " + QString::number(unmatchedTracks.size()) +
              "，未匹配观测数: " + QString::number(unmatchedMeasurements.size()));

    LOG_FUNCTION_END();
}

/**
 * @brief 更新匹配的航迹
 * @param matches 成功匹配的航迹ID和观测索引对
 * @param measurements 观测数据列表
 * @details 使用匹配的观测数据更新相应的航迹
 */
void TrackManager::updateMatchedTracks(const std::vector<std::pair<int, int>>& matches,
                                       const std::vector<Measurement>& measurements)
{
    LOG_FUNCTION_BEGIN();

    for (const auto& match : matches) {
        int trackId = match.first;
        int measIdx = match.second;

        if(m_tracks.count(trackId)) {
            LOG_DEBUG("更新航迹 " + QString::number(trackId) + " 使用观测索引 " +
                      QString::number(measIdx));
            m_tracks[trackId]->update(measurements[measIdx]);
        } else {
            LOG_WARN("尝试更新不存在的航迹ID: " + QString::number(trackId));
        }
    }

    LOG_FUNCTION_END();
}

/**
 * @brief 创建新航迹
 * @param unmatchedMeasurements 未匹配的观测索引列表
 * @param measurements 观测数据列表
 * @details 为未匹配的观测创建新航迹，包含聚类处理
 */
void TrackManager::createNewTracks(const std::vector<int>& unmatchedMeasurements,
                                   const std::vector<Measurement>& measurements)
{
    LOG_FUNCTION_BEGIN();

    if (unmatchedMeasurements.empty()) {
        LOG_DEBUG("无未匹配观测，跳过创建");
        LOG_FUNCTION_END();
        return;
    }

    LOG_DEBUG("处理 " + QString::number(unmatchedMeasurements.size()) + " 个未匹配观测");
    std::vector<bool> meas_processed(measurements.size(), false);
    int newTracksCreated = 0;

    // 处理每个未匹配的观测
    for (int i = 0; i < unmatchedMeasurements.size(); ++i) {
        int idx1 = unmatchedMeasurements[i];
        if (meas_processed[idx1]) {
            LOG_DEBUG("观测 " + QString::number(idx1) + " 已处理，跳过");
            continue;
        }

        // 默认将当前观测点作为新航迹的核心
        std::vector<int> cluster;
        cluster.push_back(idx1);
        meas_processed[idx1] = true;

        // 检查其他未匹配的观测点是否与它足够近（聚类处理）
        for (int j = i + 1; j < unmatchedMeasurements.size(); ++j) {
            int idx2 = unmatchedMeasurements[j];
            if (meas_processed[idx2]) {
                continue;
            }

            double dist = (measurements[idx1].position - measurements[idx2].position).norm();
            if (dist < m_newTrackGateDistance) {
                // 如果距离很近，认为是同一目标的多次上报，归为一类
                meas_processed[idx2] = true;
                LOG_DEBUG("观测 " + QString::number(idx2) + " 与观测 " +
                          QString::number(idx1) + " 距离仅 " + QString::number(dist, 'f', 2) +
                          " 米，低于门限 " + QString::number(m_newTrackGateDistance, 'f', 2) +
                          " 米，已聚类并跳过单独创建");
            }
        }

        // 只为这个"聚类"的第一个观测点创建航迹
        auto model = std::make_unique<ConstantAccelerationModel>();
        TrackPtr newTrack = std::make_shared<Track>(measurements[idx1], m_nextTrackId++, std::move(model));

        m_tracks[newTrack->getId()] = newTrack;
        newTracksCreated++;

        LOG_INFO("创建新航迹，ID: " + QString::number(newTrack->getId()) +
                 "，位置: (" + QString::number(measurements[idx1].position.x(), 'f', 2) +
                 ", " + QString::number(measurements[idx1].position.y(), 'f', 2) +
                 ", " + QString::number(measurements[idx1].position.z(), 'f', 2) + ")");
    }

    LOG_DEBUG("共创建 " + QString::number(newTracksCreated) + " 条新航迹");
    LOG_FUNCTION_END();
}

/**
 * @brief 管理未匹配的航迹
 * @param unmatchedTracks 未匹配的航迹ID列表
 * @details 增加未匹配航迹的丢失计数，删除丢失过久的航迹
 */
void TrackManager::manageUnmatchedTracks(const std::vector<int>& unmatchedTracks)
{
    LOG_FUNCTION_BEGIN();

    int deletedCount = 0;

    for (int trackId : unmatchedTracks) {
        if(m_tracks.count(trackId)) {
            LOG_DEBUG("增加航迹 " + QString::number(trackId) + " 的丢失计数");
            m_tracks[trackId]->incrementMisses();

            if (m_tracks[trackId]->isLost()) {
                LOG_INFO("删除航迹 " + QString::number(trackId) + "，丢失次数: " +
                         QString::number(m_tracks[trackId]->getMisses()));
                m_tracks.erase(trackId);
                deletedCount++;
            }
        } else {
            LOG_WARN("尝试管理不存在的航迹ID: " + QString::number(trackId));
        }
    }

    LOG_DEBUG("共删除 " + QString::number(deletedCount) + " 条丢失航迹");
    LOG_FUNCTION_END();
}
