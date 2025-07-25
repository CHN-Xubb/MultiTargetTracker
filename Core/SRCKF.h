#ifndef SRCKF_H
#define SRCKF_H

#include "IMotionModel.h" // 假设这是您已有的运动模型接口
#include <vector>

/**
 * @brief 平方根容积卡尔曼滤波器类
 * @details 实现基于 Cholesky 因子传递的 SR-CKF 算法，具有优异的数值稳定性。
 */
class SRCKF
{
public:
    SRCKF();

    /**
     * @brief 预测步骤
     * @param x 状态向量 (输入/输出)
     * @param S 状态协方差的 Cholesky 因子 (输入/输出)，P = S*S'
     * @param model 运动模型
     * @param dt 时间步长
     */
    void predict(StateVector& x, Eigen::MatrixXd& S,
                 const IMotionModel& model, double dt);

    /**
     * @brief 更新步骤
     * @param x 状态向量 (输入/输出)
     * @param S 状态协方差的 Cholesky 因子 (输入/输出)
     * @param model 运动模型
     * @param z 观测向量
     * @param R 观测噪声协方差矩阵
     */
    void update(StateVector& x, Eigen::MatrixXd& S,
                const IMotionModel& model,
                const MeasurementVector& z, const Eigen::MatrixXd& R);

private:
    /**
     * @brief Cholesky 因子更新的核心函数 (基于 QR 分解)
     * @param S_old 旧的 Cholesky 因子
     * @param M 要合并的矩阵
     * @return 新的 Cholesky 因子
     * @details 这是 SR-CKF 的核心，通过 QR 分解对 Cholesky 因子进行加性更新。
     */
    Eigen::MatrixXd qr_update(const Eigen::MatrixXd& S_old, const Eigen::MatrixXd& M);

    /**
     * @brief Cholesky 因子降维更新的核心函数
     * @param S_old 旧的 Cholesky 因子
     * @param U 要"减去"的矩阵，其外积 U*U' 将从 P 中减去
     * @return 新的 Cholesky 因子
     * @details 这是 SR-CKF 保证数值稳定性的关键，通过 QR 分解实现协方差的减法。
     */
    Eigen::MatrixXd chol_downdate(const Eigen::MatrixXd& S_old, const Eigen::MatrixXd& U);
};

#endif // SRCKF_H
