/**
 * @file Track.h
 * @brief 航迹跟踪类的头文件
 * @details 定义了Track类，负责单个目标的状态估计和轨迹预测
 * @author xubb
 * @date 20250711
 */

#ifndef TRACK_H
#define TRACK_H

#include "DataStructures.h"
#include "IMotionModel.h"
#include "SRCKF.h"
#include "CKF.h"
#include <memory>

/**
 * @brief 航迹跟踪类
 * @details 负责管理单个目标的状态估计、更新和预测
 */
class Track
{
public:
    /**
     * @brief 构造函数
     * @param initialMeasurement 初始观测数据
     * @param trackId 航迹ID
     * @param model 运动模型
     * @details 基于初始观测和运动模型创建航迹
     */
    Track(const Measurement& initialMeasurement, int trackId, std::unique_ptr<IMotionModel> model);

    /**
     * @brief 析构函数
     */
    ~Track();

    /**
     * @brief 预测航迹状态
     * @param dt 时间步长(秒)
     * @details 根据运动模型将航迹状态向前预测指定时间
     */
    void predict(double dt);

    /**
     * @brief 更新航迹状态
     * @param measurement 观测数据
     * @details 使用观测数据更新航迹状态估计
     */
    void update(const Measurement& measurement);

    /**
     * @brief 预测未来轨迹
     * @param timeHorizon 预测时间范围(秒)
     * @param timeStep 预测时间步长(秒)
     * @return 未来位置点的向量
     * @details 基于当前状态和运动模型预测未来轨迹点
     */
    std::vector<Vector3> predictFutureTrajectory(double timeHorizon, double timeStep) const;

    /**
     * @brief 获取航迹ID
     * @return 航迹ID
     */
    int getId() const;

    /**
     * @brief 检查航迹是否已确认
     * @return 如果航迹已确认则返回true
     * @details 当连续命中次数达到阈值时航迹被确认
     */
    bool isConfirmed() const;

    /**
     * @brief 检查航迹是否已丢失
     * @return 如果航迹已丢失则返回true
     * @details 当连续丢失次数超过阈值时航迹被标记为丢失
     */
    bool isLost() const;

    /**
     * @brief 增加航迹丢失计数
     * @details 当航迹与任何观测都不匹配时调用
     */
    void incrementMisses();

    /**
     * @brief 获取当前状态向量
     * @return 状态向量的常引用
     */
    const StateVector& getState() const;

    /**
     * @brief 获取最后更新时间
     * @return 最后一次更新的时间戳
     */
    double getLastUpdateTime() const;

    /**
     * @brief 获取命中次数
     * @return 命中次数
     */
    int getHits() const;

    /**
     * @brief 获取丢失次数
     * @return 连续丢失次数
     */
    int getMisses() const;

private:
    /**
     * @brief 卡尔曼滤波器
     * @details 用于状态估计的核心算法组件
     */
    CKF m_filter;

    /**
     * @brief 运动模型
     * @details 描述目标运动特性的模型
     */
    std::unique_ptr<IMotionModel> m_model;

    /**
     * @brief 状态向量
     * @details 当前估计的目标状态
     */
    StateVector m_x;

    /**
     * @brief 状态协方差矩阵
     * @details 表示状态估计的不确定性
     */
    Eigen::MatrixXd m_P;

    /**
     * @brief 观测噪声矩阵
     */
    Eigen::MatrixXd m_R;

    /**
     * @brief 航迹ID
     */
    int m_id;

    /**
     * @brief 航迹年龄
     * @details 航迹存在的周期数
     */
    int m_age;

    /**
     * @brief 命中次数
     * @details 航迹被观测更新的总次数
     */
    int m_hits;

    /**
     * @brief 丢失次数
     * @details 航迹连续未匹配的次数
     */
    int m_misses;

    /**
     * @brief 最后更新时间
     */
    double m_lastUpdateTime;

    /**
     * @brief 确认所需命中次数
     * @details 航迹被确认所需的最小命中次数
     */
    int m_confirmationHits;

    /**
     * @brief 删除所需丢失次数
     * @details 航迹被删除所需的连续丢失次数
     */
    int maxMissesToDelete;
};

/**
 * @brief 航迹智能指针类型
 */
using TrackPtr = std::shared_ptr<Track>;

#endif // TRACK_H
