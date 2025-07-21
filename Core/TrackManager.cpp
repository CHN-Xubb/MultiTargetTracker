#include "TrackManager.h"
#include "LogManager.h"
#include "ConstantVelocityModel.h" // 需要包含具体的模型以创建实例
#include "ConstantAccelerationModel.h"
#include <limits> // 用于 std::numeric_limits
#include <set>    // 用于 std::set，优化关联逻辑
#include <QSettings>


TrackManager::TrackManager() : m_nextTrackId(0), m_lastProcessTime(0.0),
    m_associationGateDistance(0.0),m_newTrackGateDistance(0.0)
{
    QSettings settings("Server.ini", QSettings::IniFormat);
    m_associationGateDistance = settings.value("KalmanFilter/associationGateDistance", 10.0).toDouble();
    m_newTrackGateDistance = settings.value("KalmanFilter/newTrackGateDistance", 5.0).toDouble();
}

TrackManager::~TrackManager() {}

void TrackManager::processMeasurements(const std::vector<Measurement>& measurements) {
    QMutexLocker locker(&m_mutex);

    if (measurements.empty()) {
        return;
    }

    //qDebug() << "[关联] 开始数据关联，现有" << m_tracks.size() << "条航迹和" << measurements.size() << "个量测。";

    // 1. 数据关联
    std::vector<std::pair<int, int>> matches;
    std::vector<int> unmatchedTracks;
    std::vector<int> unmatchedMeasurements;
    dataAssociation(measurements, matches, unmatchedTracks, unmatchedMeasurements);

    // 2. 更新匹配的航迹
    updateMatchedTracks(matches, measurements);

    // 3. 为未匹配的观测创建新航迹 (核心修改点)
    createNewTracks(unmatchedMeasurements, measurements);

    // 4. 管理未匹配的航迹
    manageUnmatchedTracks(unmatchedTracks);

    // 更新时间戳
    m_lastProcessTime = measurements.back().timestamp;

//    qDebug() << "[关联] 关联完成。匹配数:" << matches.size()
//             << "未匹配航迹数:" << unmatchedTracks.size()
//             << "未匹配量测数:" << unmatchedMeasurements.size();
}

void TrackManager::predictTo(double timestamp) {
    QMutexLocker locker(&m_mutex);
    if (m_lastProcessTime == 0.0) {
        m_lastProcessTime = timestamp;
        return;
    }

    double dt = timestamp - m_lastProcessTime;
    if (dt <= 0) return;

    // C++11 兼容的循环方式
    for (const auto& pair : m_tracks) {
        TrackPtr track = pair.second;
        track->predict(dt);
    }
}

std::vector<TrackPtr> TrackManager::getTracks() const {
    QMutexLocker locker(&m_mutex);
    std::vector<TrackPtr> tracks;
    tracks.reserve(m_tracks.size());
    // C++11 兼容的循环方式
    for (const auto& pair : m_tracks) {
        tracks.push_back(pair.second);
    }
    return tracks;
}


// --- 私有方法实现 ---

void TrackManager::dataAssociation(const std::vector<Measurement>& measurements,
                                   std::vector<std::pair<int, int>>& matches,
                                   std::vector<int>& unmatchedTracks,
                                   std::vector<int>& unmatchedMeasurements)
{
    // C++11 兼容的关联逻辑
    if (m_tracks.empty()) {
        for (size_t i = 0; i < measurements.size(); ++i) {
            unmatchedMeasurements.push_back(i);
        }
        return;
    }

    std::vector<bool> meas_matched(measurements.size(), false);
    std::set<int> matched_track_ids; // 使用 set 提高查找效率

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
            //qDebug().nospace() << "    [关联] 检查航迹 " << trackId << " <-> 量测 " << j << ". 距离: " << QString::number(dist, 'f', 2);
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
            //qDebug() << "        -> [关联] 找到航迹" << trackId << "与量测" << best_match_idx << "的匹配. 最小距离:" << min_dist;
        } else {
            //qDebug() << "        -> [关联] 航迹" << trackId << "无匹配. 最近距离" << min_dist << "在门限" << m_associationGateDistance << "之外";
        }
    }

    // 遍历所有航迹，不在 matched_track_ids 中的就是未匹配的航迹
    for (const auto& pair : m_tracks) {
        if (matched_track_ids.find(pair.first) == matched_track_ids.end()) {
            unmatchedTracks.push_back(pair.first);
        }
    }

    // 遍历所有观测，不在 meas_matched 中的就是未匹配的观测
    for (size_t i = 0; i < measurements.size(); ++i) {
        if (!meas_matched[i]) {
            unmatchedMeasurements.push_back(i);
        }
    }
}


void TrackManager::updateMatchedTracks(const std::vector<std::pair<int, int>>& matches,
                                       const std::vector<Measurement>& measurements)
{
    for (const auto& match : matches) {
        int trackId = match.first;
        int measIdx = match.second;
        if(m_tracks.count(trackId)){
            m_tracks[trackId]->update(measurements[measIdx]);
        }
    }
}

// ======================= 核心修改点 =======================
void TrackManager::createNewTracks(const std::vector<int>& unmatchedMeasurements,
                                   const std::vector<Measurement>& measurements)
{
    if (unmatchedMeasurements.empty()) {
        return;
    }

    std::vector<bool> meas_processed(measurements.size(), false);

    for (int i = 0; i < unmatchedMeasurements.size(); ++i) {
        int idx1 = unmatchedMeasurements[i];
        if (meas_processed[idx1]) {
            continue;
        }

        // 默认将当前观测点作为新航迹的核心
        std::vector<int> cluster;
        cluster.push_back(idx1);
        meas_processed[idx1] = true;

        // 检查其他未匹配的观测点是否与它足够近
        for (int j = i + 1; j < unmatchedMeasurements.size(); ++j) {
            int idx2 = unmatchedMeasurements[j];
            if (meas_processed[idx2]) {
                continue;
            }

            double dist = (measurements[idx1].position - measurements[idx2].position).norm();
            if (dist < m_newTrackGateDistance) {
                // 如果距离很近，认为是同一目标的多次上报，归为一类，不再单独创建航迹
                meas_processed[idx2] = true;
                //qWarning() << "[创建] 索引" << idx2 << "处的量测与" << idx1 << "距离过近(距离=" << dist << ")。正在聚类并跳过新航迹创建。";
            }
        }

        // 只为这个“聚类”的第一个观测点创建航迹
        auto model = std::make_unique<ConstantAccelerationModel>();
        TrackPtr newTrack = std::make_shared<Track>(measurements[idx1], m_nextTrackId++, std::move(model));

        m_tracks[newTrack->getId()] = newTrack;
    }
}


void TrackManager::manageUnmatchedTracks(const std::vector<int>& unmatchedTracks)
{
    for (int trackId : unmatchedTracks) {
        if(m_tracks.count(trackId)){
            // 日志已移动到 Track::incrementMisses() 内部
            m_tracks[trackId]->incrementMisses();
            if (m_tracks[trackId]->isLost()) {
                //qWarning() << "[管理] 因丢失次数过多 (" << m_tracks[trackId]->getMisses() << ")，删除航迹" << trackId;
                m_tracks.erase(trackId);
            }
        }
    }
}
