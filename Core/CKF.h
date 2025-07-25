/**
 * @file CKF.h
 * @brief 立方卡尔曼滤波器头文件
 * @details 定义了CKF类，实现立方卡尔曼滤波算法
 * @author xubb
 * @date 20250711
 */

#ifndef CKF_H
#define CKF_H

#include "IMotionModel.h"
#include <vector>

/**
 * @brief 立方卡尔曼滤波器类
 * @details 实现基于立方规则的非线性滤波算法，用于状态估计
 */
class CKF
{
public:
    /**
     * @brief 构造函数
     */
    CKF();

    /**
     * @brief 预测步骤
     * @param x 状态向量(输入/输出参数)
     * @param P 状态协方差矩阵(输入/输出参数)
     * @param model 运动模型
     * @param dt 时间步长(秒)
     * @details 根据运动模型将状态向前预测，更新状态向量和协方差矩阵
     */
    void predict(StateVector& x, Eigen::MatrixXd& P,
                 const IMotionModel& model, double dt);

    /**
     * @brief 更新步骤
     * @param x 状态向量(输入/输出参数)
     * @param P 状态协方差矩阵(输入/输出参数)
     * @param model 运动模型
     * @param z 观测向量
     * @param R 观测噪声协方差矩阵
     * @details 根据观测数据更新状态估计，修正状态向量和协方差矩阵
     */
    void update(StateVector& x, Eigen::MatrixXd& P,
                const IMotionModel& model,
                const MeasurementVector& z, const Eigen::MatrixXd& R);

private:
    /**
     * @brief 生成立方点
     * @param x 状态向量
     * @param P 状态协方差矩阵
     * @return 立方点集合
     * @details 根据当前状态和协方差生成用于滤波计算的立方点
     */
    std::vector<StateVector> generateCubaturePoints(const StateVector& x, const Eigen::MatrixXd& P);
};

#endif // CKF_H
