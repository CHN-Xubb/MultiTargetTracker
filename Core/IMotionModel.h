#ifndef IMOTIONMODEL_H
#define IMOTIONMODEL_H

#include <Eigen/Dense>
#include <functional>

// 定义状态向量和观测向量的类型别名
using StateVector = Eigen::VectorXd;
using MeasurementVector = Eigen::Vector3d;

class IMotionModel
{
public:
    virtual ~IMotionModel() = default;

    // 获取状态向量的维度
    virtual int stateDim() const = 0;

    // 获取观测向量的维度
    virtual int measurementDim() const = 0;

    // 状态转移函数：根据当前状态和时间差，预测下一状态
    virtual StateVector predict(const StateVector& x, double dt) const = 0;

    // 观测函数：从状态向量中提取观测值
    virtual MeasurementVector observe(const StateVector& x) const = 0;

    // 获取过程噪声协方差矩阵 Q
    // --- 修改点: 增加 dt 参数 ---
    virtual Eigen::MatrixXd getProcessNoiseMatrix(double dt) const = 0;

    // 获取一个初始的协方差矩阵 P0
    virtual Eigen::MatrixXd getInitialCovariance() const = 0;
};

#endif // IMOTIONMODEL_H
