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
#include <vector> // 确保包含<vector>

// 定义统一的日志宏
#define LOG_DEBUG(msg) qDebug() << "[TrackManager::" << __FUNCTION__ << "] " << msg
#define LOG_INFO(msg) qInfo() << "[TrackManager::" << __FUNCTION__ << "] " << msg
#define LOG_WARN(msg) qWarning() << "[TrackManager::" << __FUNCTION__ << "] " << msg
#define LOG_ERROR(msg) qCritical() << "[TrackManager::" << __FUNCTION__ << "] " << msg

// 函数跟踪宏
#define LOG_FUNCTION_BEGIN() LOG_DEBUG("开始")
#define LOG_FUNCTION_END() LOG_DEBUG("结束")


TrackManager::TrackManager()
    : m_nextTrackId(0),
      m_lastProcessTime(0.0),
      m_associationGateDistance(0.0),
      m_newTrackGateDistance(0.0)
{
    LOG_FUNCTION_BEGIN();

    QSettings settings("Server.ini", QSettings::IniFormat);
    m_associationGateDistance = settings.value("KalmanFilter/associationGateDistance", 10.0).toDouble();
    m_newTrackGateDistance = settings.value("KalmanFilter/newTrackGateDistance", 5.0).toDouble();


    LOG_INFO("初始化完成，关联门限: " + QString::number(m_associationGateDistance) +
             "米，新航迹门限: " + QString::number(m_newTrackGateDistance) + "米");

    LOG_FUNCTION_END();
}


TrackManager::~TrackManager()
{
    LOG_FUNCTION_BEGIN();
    LOG_INFO("航迹管理器销毁，当前航迹数: " + QString::number(m_tracks.size()));
    LOG_FUNCTION_END();
}


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
    // ========================[核心修改点 1: 获取已匹配航迹ID]========================
    // dataAssociation现在返回成功匹配的航迹ID集合，供后续使用
    std::set<int> matched_track_ids = dataAssociation(measurements, matches, unmatchedTracks, unmatchedMeasurements);

    // 2. 更新匹配的航迹
    LOG_DEBUG("开始更新 " + QString::number(matches.size()) + " 个匹配的航迹");
    updateMatchedTracks(matches, measurements);

    // 3. 为未匹配的观测创建新航迹
    LOG_DEBUG("处理 " + QString::number(unmatchedMeasurements.size()) + " 个未匹配的观测");
    // ========================[核心修改点 2: 传递已匹配航迹ID]========================
    // 将已匹配的航迹ID列表传递给createNewTracks，以防止创建重复航迹
    createNewTracks(unmatchedMeasurements, measurements, matched_track_ids);


    // 4. 管理未匹配的航迹
    LOG_DEBUG("管理 " + QString::number(unmatchedTracks.size()) + " 个未匹配的航迹");
    manageUnmatchedTracks(unmatchedTracks);

    // 只有在处理完一批数据后才更新时间戳
    if (!measurements.empty()) {
        m_lastProcessTime = measurements.back().timestamp;
    }


    LOG_DEBUG("处理完成。匹配数: " + QString::number(matches.size()) +
              "，未匹配航迹数: " + QString::number(unmatchedTracks.size()) +
              "，未匹配观测数: " + QString::number(unmatchedMeasurements.size()) +
              "，当前航迹总数: " + QString::number(m_tracks.size()));
}



void TrackManager::predictTo(double timestamp)
{
    QWriteLocker locker(&m_lock);

    if (m_lastProcessTime == 0.0) {
        m_lastProcessTime = timestamp;
        LOG_DEBUG("初始化时间戳: " + QString::number(timestamp));
        return;
    }

    double dt = timestamp - m_lastProcessTime;
    if (dt <= 0) {
        //LOG_DEBUG("时间差为负或零，跳过预测: " + QString::number(dt));
        return;
    }

    LOG_DEBUG("预测 " + QString::number(m_tracks.size()) +
              " 条航迹到时间戳 " + QString::number(timestamp) +
              "，时间差: " + QString::number(dt) + " 秒");

    for (const auto& pair : m_tracks) {
        TrackPtr track = pair.second;
        track->predict(dt);
    }
}


std::vector<TrackPtr> TrackManager::getTracks() const
{
    QReadLocker locker(&m_lock);

    std::vector<TrackPtr> tracks;
    tracks.reserve(m_tracks.size());

    for (const auto& pair : m_tracks) {
        tracks.push_back(pair.second);
    }

    LOG_DEBUG("获取 " + QString::number(tracks.size()) + " 条航迹");
    return tracks;
}


// ========================[核心修改点 3: 修改dataAssociation返回值]========================
std::set<int> TrackManager::dataAssociation(const std::vector<Measurement>& measurements,
                                            std::vector<std::pair<int, int>>& matches,
                                            std::vector<int>& unmatchedTracks,
                                            std::vector<int>& unmatchedMeasurements)
{
    LOG_FUNCTION_BEGIN();
    std::set<int> matched_track_ids;

    if (m_tracks.empty()) {
        LOG_DEBUG("无现有航迹，所有 " + QString::number(measurements.size()) + " 条观测都标记为未匹配");
        for (size_t i = 0; i < measurements.size(); ++i) {
            unmatchedMeasurements.push_back(i);
        }
        LOG_FUNCTION_END();
        return matched_track_ids;
    }

    std::vector<bool> meas_matched(measurements.size(), false);


    LOG_DEBUG("开始关联 " + QString::number(m_tracks.size()) + " 条航迹和 " +
              QString::number(measurements.size()) + " 个观测");

    for (const auto& pair : m_tracks) {
        int trackId = pair.first;
        const TrackPtr& track = pair.second;

        double min_dist = std::numeric_limits<double>::max();
        int best_match_idx = -1;

        Vector3 predicted_pos = track->getState().head<3>();

        for (size_t j = 0; j < measurements.size(); ++j) {
            if (meas_matched[j]) continue;

            double dist = (predicted_pos - measurements[j].position).norm();

            if (dist < min_dist) {
                min_dist = dist;
                best_match_idx = j;
            }
        }

        if (best_match_idx != -1 && min_dist < m_associationGateDistance) {
            // 防止一个观测被多个航迹匹配，需要再次检查
            if (!meas_matched[best_match_idx]) {
                matches.push_back({trackId, best_match_idx});
                meas_matched[best_match_idx] = true;
                matched_track_ids.insert(trackId);
                LOG_DEBUG("航迹 " + QString::number(trackId) + " 与观测 " +
                          QString::number(best_match_idx) + " 匹配成功，距离: " +
                          QString::number(min_dist, 'f', 2) + " 米");
            }
        } else {
            // LOG_DEBUG("航迹 " + QString::number(trackId) + " 无匹配");
        }
    }

    for (const auto& pair : m_tracks) {
        if (matched_track_ids.find(pair.first) == matched_track_ids.end()) {
            unmatchedTracks.push_back(pair.first);
        }
    }

    for (size_t i = 0; i < measurements.size(); ++i) {
        if (!meas_matched[i]) {
            unmatchedMeasurements.push_back(i);
        }
    }

    LOG_DEBUG("关联完成，匹配数: " + QString::number(matches.size()) +
              "，未匹配航迹数: " + QString::number(unmatchedTracks.size()) +
              "，未匹配观测数: " + QString::number(unmatchedMeasurements.size()));

    LOG_FUNCTION_END();
    return matched_track_ids;
}


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




// ========================[核心修改点 4: 重构createNewTracks逻辑]========================
void TrackManager::createNewTracks(const std::vector<int>& unmatchedMeasurements,
                                   const std::vector<Measurement>& measurements,
                                   const std::set<int>& matchedTrackIds)
{
    LOG_FUNCTION_BEGIN();

    if (unmatchedMeasurements.empty()) {
        LOG_DEBUG("无未匹配观测，跳过创建");
        LOG_FUNCTION_END();
        return;
    }

    std::vector<int> trulyUnmatchedMeasurements;

    for (int measIdx : unmatchedMeasurements) {
        const auto& measurement = measurements[measIdx];
        bool isCloseToExistingTrack = false;

        // 检查这个“未匹配”的观测点是否离任何一个“已匹配”的航迹很近
        for (int trackId : matchedTrackIds) {
            if (m_tracks.count(trackId)) {
                double dist = (m_tracks[trackId]->getState().head<3>() - measurement.position).norm();
                if (dist < m_newTrackGateDistance) {
                    isCloseToExistingTrack = true;
                    LOG_DEBUG("未匹配观测 " + QString::number(measIdx) + " 因距离已更新的航迹 " +
                              QString::number(trackId) + " 过近 (" + QString::number(dist, 'f', 2) + "米)，被忽略");
                    break;
                }
            }
        }

        // 如果它不靠近任何已存在的航迹，才认为它可能是一个新目标
        if (!isCloseToExistingTrack) {
            trulyUnmatchedMeasurements.push_back(measIdx);
        }
    }

    if (trulyUnmatchedMeasurements.empty()) {
        LOG_DEBUG("所有未匹配观测都因靠近现有航迹而被忽略，无新航迹创建");
        LOG_FUNCTION_END();
        return;
    }

    LOG_DEBUG("处理 " + QString::number(trulyUnmatchedMeasurements.size()) + " 个真正未匹配的观测");
    std::vector<bool> meas_processed(measurements.size(), false);
    int newTracksCreated = 0;

    for (int idx1 : trulyUnmatchedMeasurements) {
        if (meas_processed[idx1]) {
            continue;
        }

        // 为这个真正无归属的观测点创建新航迹
        auto model = std::make_unique<ConstantAccelerationModel>();
        TrackPtr newTrack = std::make_shared<Track>(measurements[idx1], m_nextTrackId++, std::move(model));

        m_tracks[newTrack->getId()] = newTrack;
        newTracksCreated++;

        LOG_INFO("创建新航迹，ID: " + QString::number(newTrack->getId()) +
                 "，位置: (" + QString::number(measurements[idx1].position.x(), 'f', 2) +
                 ", " + QString::number(measurements[idx1].position.y(), 'f', 2) +
                 ", " + QString::number(measurements[idx1].position.z(), 'f', 2) + ")");

        // (可选) 仍然可以保留内部聚类，以处理来自同一新目标的密集点云
        for (int idx2 : trulyUnmatchedMeasurements) {
            if (idx1 == idx2 || meas_processed[idx2]) continue;
            double dist = (measurements[idx1].position - measurements[idx2].position).norm();
            if (dist < m_newTrackGateDistance) {
                meas_processed[idx2] = true;
                LOG_DEBUG("观测 " + QString::number(idx2) + " 与新航迹 " +
                          QString::number(newTrack->getId()) + " 的初始点 " + QString::number(idx1) +
                          " 聚类，不再单独创建航迹");
            }
        }
    }

    LOG_DEBUG("共创建 " + QString::number(newTracksCreated) + " 条新航迹");
    LOG_FUNCTION_END();
}


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



