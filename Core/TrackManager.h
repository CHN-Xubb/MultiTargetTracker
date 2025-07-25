/**
 * @file TrackManager.h
 * @brief 航迹管理器头文件
 * @details 定义了TrackManager类，负责航迹的创建、更新和管理
 * @author xubb
 * @date 20250711
 */

#ifndef TRACKMANAGER_H
#define TRACKMANAGER_H

#include "DataStructures.h"
#include "Track.h"
#include <vector>
#include <unordered_map>
#include <memory>
#include <QMutex>
#include <QReadWriteLock>

/**
 * @brief 航迹管理器类
 * @details 负责管理多个航迹，包括数据关联、航迹创建、更新和删除
 */
class TrackManager
{
public:
    /**
     * @brief 构造函数
     * @details 初始化航迹管理器并从配置文件读取参数
     */
    TrackManager();

    /**
     * @brief 析构函数
     */
    ~TrackManager();

    /**
     * @brief 处理观测数据
     * @param measurements 观测数据列表
     * @details 主处理函数，接收所有观测数据并进行关联和更新
     */
    void processMeasurements(const std::vector<Measurement>& measurements);

    /**
     * @brief 预测所有航迹状态到指定时间
     * @param timestamp 目标时间戳
     * @details 将所有航迹的状态向前预测到指定时间点
     */
    void predictTo(double timestamp);

    /**
     * @brief 获取当前所有航迹
     * @return 航迹指针的vector
     * @details 线程安全地获取当前所有活动航迹
     */
    std::vector<TrackPtr> getTracks() const;

private:
    /**
     * @brief 数据关联
     * @param measurements 观测数据列表
     * @param matches 成功匹配的航迹ID和观测索引对
     * @param unmatchedTracks 未匹配的航迹ID列表
     * @param unmatchedMeasurements 未匹配的观测索引列表
     * @details 将观测数据关联到已存在的航迹
     */
    void dataAssociation(const std::vector<Measurement>& measurements,
                         std::vector<std::pair<int, int>>& matches,
                         std::vector<int>& unmatchedTracks,
                         std::vector<int>& unmatchedMeasurements);

    /**
     * @brief 更新匹配的航迹
     * @param matches 成功匹配的航迹ID和观测索引对
     * @param measurements 观测数据列表
     * @details 使用匹配的观测数据更新相应的航迹
     */
    void updateMatchedTracks(const std::vector<std::pair<int, int>>& matches,
                             const std::vector<Measurement>& measurements);

    /**
     * @brief 创建新航迹
     * @param unmatchedMeasurements 未匹配的观测索引列表
     * @param measurements 观测数据列表
     * @details 为未匹配的观测创建新航迹，包含聚类处理
     */
    void createNewTracks(const std::vector<int>& unmatchedMeasurements,
                         const std::vector<Measurement>& measurements);

    /**
     * @brief 管理未匹配的航迹
     * @param unmatchedTracks 未匹配的航迹ID列表
     * @details 增加未匹配航迹的丢失计数，删除丢失过久的航迹
     */
    void manageUnmatchedTracks(const std::vector<int>& unmatchedTracks);

private:
    /**
     * @brief 航迹集合
     * @details 保存所有活动航迹，键为航迹ID，值为航迹指针
     */
    std::unordered_map<int, TrackPtr> m_tracks;

    /**
     * @brief 下一个可用的航迹ID
     */
    int m_nextTrackId;

    /**
     * @brief 上一次处理的时间戳
     */
    double m_lastProcessTime;

    /**
     * @brief 关联门限距离(米)
     * @details 航迹与观测数据关联的最大允许距离
     */
    double m_associationGateDistance;

    /**
     * @brief 新航迹创建门限距离(米)
     * @details 防止重复创建航迹的距离阈值
     */
    double m_newTrackGateDistance;


    mutable QReadWriteLock m_lock;
};

#endif // TRACKMANAGER_H
