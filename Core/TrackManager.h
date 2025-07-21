#ifndef TRACKMANAGER_H
#define TRACKMANAGER_H

#include "DataStructures.h"
#include "Track.h"
#include <vector>
#include <unordered_map>
#include <memory>
#include <QMutex>

class TrackManager
{
public:
    TrackManager();
    ~TrackManager();

    // 主处理函数，接收所有观测数据
    void processMeasurements(const std::vector<Measurement>& measurements);

    // 预测所有航迹到指定时间
    void predictTo(double timestamp);

    // 获取当前所有航迹
    std::vector<TrackPtr> getTracks() const;

private:
    // 将观测数据关联到已存在的航迹
    void dataAssociation(const std::vector<Measurement>& measurements,
                         std::vector<std::pair<int, int>>& matches,
                         std::vector<int>& unmatchedTracks,
                         std::vector<int>& unmatchedMeasurements);

    // 更新匹配上的航迹
    void updateMatchedTracks(const std::vector<std::pair<int, int>>& matches,
                             const std::vector<Measurement>& measurements);

    // 为未匹配的观测创建新航迹
    void createNewTracks(const std::vector<int>& unmatchedMeasurements,
                         const std::vector<Measurement>& measurements);

    // 管理未匹配的航迹（增加丢失计数，删除丢失过久的航迹）
    void manageUnmatchedTracks(const std::vector<int>& unmatchedTracks);


    std::unordered_map<int, TrackPtr> m_tracks; // 键: 航迹ID, 值: 航迹跟踪器
    int m_nextTrackId;
    double m_lastProcessTime;
    double m_associationGateDistance;// 关联门限(米)
    double m_newTrackGateDistance;// 新航迹创建门限(米)，防止重复

    mutable QMutex m_mutex; // 保护 m_tracks 的访问
};

#endif // TRACKMANAGER_H
