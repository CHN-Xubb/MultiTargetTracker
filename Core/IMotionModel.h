/**
 * @file IMotionModel.h
 * @brief 运动模型接口头文件
 * @details 定义了运动模型的抽象接口，用于状态预测和观测映射
 * @author xubb
 * @date 20250711
 */

#ifndef IMOTIONMODEL_H
#define IMOTIONMODEL_H

#include <Eigen/Dense>
#include <functional>

/**
 * @brief 状态向量类型别名
 * @details 使用Eigen的动态向量表示目标状态
 */
using StateVector = Eigen::VectorXd;

/**
 * @brief 观测向量类型别名
 * @details 使用Eigen的3D向量表示观测值，通常为位置
 */
using MeasurementVector = Eigen::Vector3d;

/**
 * @brief 运动模型接口类
 * @details 定义了所有运动模型必须实现的方法，用于目标状态预测和观测映射
 */
class IMotionModel
{
public:
    /**
     * @brief 虚析构函数
     * @details 确保派生类能够正确释放资源
     */
    virtual ~IMotionModel() = default;

    /**
     * @brief 获取状态向量的维度
     * @return 状态向量的维度
     * @details 返回模型使用的状态向量维度，如匀速模型为6，匀加速模型为9
     */
    virtual int stateDim() const = 0;

    /**
     * @brief 获取观测向量的维度
     * @return 观测向量的维度
     * @details 返回模型使用的观测向量维度，通常为3（x,y,z位置）
     */
    virtual int measurementDim() const = 0;

    /**
     * @brief 状态预测函数
     * @param x 当前状态向量
     * @param dt 时间步长(秒)
     * @return 预测后的状态向量
     * @details 根据当前状态和时间差，预测下一时刻的状态
     */
    virtual StateVector predict(const StateVector& x, double dt) const = 0;

    /**
     * @brief 观测映射函数
     * @param x 当前状态向量
     * @return 对应的观测向量
     * @details 从状态向量中提取可观测部分，通常为位置信息
     */
    virtual MeasurementVector observe(const StateVector& x) const = 0;

    /**
     * @brief 获取过程噪声协方差矩阵
     * @param dt 时间步长(秒)
     * @return 过程噪声协方差矩阵Q
     * @details 计算与时间步长相关的过程噪声协方差矩阵
     */
    virtual Eigen::MatrixXd getProcessNoiseMatrix(double dt) const = 0;

    /**
     * @brief 获取初始协方差矩阵
     * @return 初始状态协方差矩阵P0
     * @details 返回新创建航迹的初始不确定性矩阵
     */
    virtual Eigen::MatrixXd getInitialCovariance() const = 0;
};

#endif // IMOTIONMODEL_H
