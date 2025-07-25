/**
 * @file ConstantAccelerationModel.h
 * @brief 匀加速运动模型头文件
 * @details 定义了ConstantAccelerationModel类，实现匀加速运动模型的状态预测和观测映射
 * @author xubb
 * @date 20250711
 */

#ifndef CONSTANTACCELERATIONMODEL_H
#define CONSTANTACCELERATIONMODEL_H

#include "IMotionModel.h"

/**
 * @brief 匀加速运动模型类
 * @details 实现9维状态空间（位置、速度、加速度）的匀加速运动模型
 *          包含状态预测、观测映射和过程噪声计算
 */
class ConstantAccelerationModel : public IMotionModel
{
public:
    /**
     * @brief 构造函数
     * @details 初始化匀加速运动模型并从配置文件读取参数
     */
    ConstantAccelerationModel();

    /**
     * @brief 获取状态向量的维度
     * @return 状态向量的维度（9：三维位置、速度和加速度）
     */
    int stateDim() const override;

    /**
     * @brief 获取观测向量的维度
     * @return 观测向量的维度（3：三维位置）
     */
    int measurementDim() const override;

    /**
     * @brief 状态预测函数
     * @param x 当前状态向量
     * @param dt 时间步长(秒)
     * @return 预测后的状态向量
     * @details 基于匀加速运动模型预测下一时刻的状态
     */
    StateVector predict(const StateVector& x, double dt) const override;

    /**
     * @brief 观测映射函数
     * @param x 状态向量
     * @return 观测向量
     * @details 从状态向量中提取位置分量作为观测
     */
    MeasurementVector observe(const StateVector& x) const override;

    /**
     * @brief 获取过程噪声协方差矩阵
     * @param dt 时间步长(秒)
     * @return 过程噪声协方差矩阵
     * @details 基于加加速度(jerk)噪声模型计算过程噪声协方差
     */
    Eigen::MatrixXd getProcessNoiseMatrix(double dt) const override;

    /**
     * @brief 获取初始协方差矩阵
     * @return 初始状态协方差矩阵
     * @details 从配置文件读取参数，构建初始状态不确定性矩阵
     */
    Eigen::MatrixXd getInitialCovariance() const override;

private:
    /**
     * @brief 状态向量维度
     */
    int m_stateDim;

    /**
     * @brief 观测向量维度
     */
    int m_measurementDim;

    /**
     * @brief 加加速度(jerk)过程噪声标准差
     * @details 描述加速度随时间变化的不确定性
     */
    double m_process_noise_std;
};

#endif // CONSTANTACCELERATIONMODEL_H
