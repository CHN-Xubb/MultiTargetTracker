#ifndef TRACK_H
#define TRACK_H

#include "DataStructures.h"
#include "IMotionModel.h"
#include "CKF.h"
#include <memory>

class Track
{
public:
    Track(const Measurement& initialMeasurement, int trackId, std::unique_ptr<IMotionModel> model);
    ~Track();

    void predict(double dt);
    void update(const Measurement& measurement);

    // --- 轨迹预测接口 ---
    std::vector<Vector3> predictFutureTrajectory(double timeHorizon, double timeStep) const;

    // --- 航迹管理接口 ---
    int getId() const;
    bool isConfirmed() const;
    bool isLost() const;

    void incrementMisses();
    const StateVector& getState() const;
    double getLastUpdateTime() const;
    int getHits() const;
    int getMisses() const;

private:
    // --- 核心组件 ---
    CKF m_filter; // <--- 类型改为 CKF
    std::unique_ptr<IMotionModel> m_model;

    // --- 航迹状态 ---
    StateVector m_x;     // 当前状态向量
    Eigen::MatrixXd m_P; // <--- 存储完整的协方差矩阵 P，而不是 S
    Eigen::MatrixXd m_R; // 观测噪声

    // --- 航迹元数据 ---
    int m_id;
    int m_age;
    int m_hits;
    int m_misses;
    double m_lastUpdateTime;
    int m_confirmationHits;
    int maxMissesToDelete;
};

using TrackPtr = std::shared_ptr<Track>;

#endif // TRACK_H
