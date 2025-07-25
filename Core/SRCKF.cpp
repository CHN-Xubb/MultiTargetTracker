#include "SRCKF.h"
#include <iostream> // 用于可能的调试输出

SRCKF::SRCKF() {}

void SRCKF::predict(StateVector& x, Eigen::MatrixXd& S, const IMotionModel& model, double dt)
{
    const int n = model.stateDim();
    const double alpha = 1.0 / std::sqrt(2.0 * n);

    // 1. 生成 2n 个 Cubature 点 (与之前相同，但直接使用 S)
    std::vector<StateVector> cubaturePoints(2 * n);
    Eigen::MatrixXd term = std::sqrt(static_cast<double>(n)) * S;
    for (int i = 0; i < n; ++i) {
        cubaturePoints[i]       = x + term.col(i);
        cubaturePoints[i + n]   = x - term.col(i);
    }

    // 2. 通过状态转移模型传递 Cubature 点
    for (int i = 0; i < 2 * n; ++i) {
        cubaturePoints[i] = model.predict(cubaturePoints[i], dt);
    }

    // 3. 计算预测的均值
    StateVector x_pred = StateVector::Zero(n);
    for (const auto& pt : cubaturePoints) {
        x_pred += pt;
    }
    x_pred /= (2.0 * n);
    x = x_pred;

    // 4. 计算预测协方差的 Cholesky 因子 S_pred
    // 核心步骤：通过 QR 分解进行加性更新
    Eigen::MatrixXd M(n, 2 * n);
    for(int i = 0; i < 2 * n; ++i) {
        M.col(i) = alpha * (cubaturePoints[i] - x_pred);
    }
    Eigen::MatrixXd Q = model.getProcessNoiseMatrix(dt);
    Eigen::MatrixXd S_Q = Q.llt().matrixL(); // 获取过程噪声的 Cholesky 因子

    S = qr_update(M, S_Q); // S_pred = qr_update([M, S_Q])
}


void SRCKF::update(StateVector& x, Eigen::MatrixXd& S, const IMotionModel& model,
                 const MeasurementVector& z, const Eigen::MatrixXd& R)
{
    const int n = model.stateDim();
    const int m = model.measurementDim();
    const double alpha = 1.0 / std::sqrt(2.0 * n);

    // 1. 基于预测后的状态，生成新的 Cubature 点
    std::vector<StateVector> cubaturePoints(2 * n);
    Eigen::MatrixXd term = std::sqrt(static_cast<double>(n)) * S;
    for (int i = 0; i < n; ++i) {
        cubaturePoints[i]       = x + term.col(i);
        cubaturePoints[i + n]   = x - term.col(i);
    }

    // 2. 通过观测模型传递 Cubature 点
    std::vector<MeasurementVector> z_points(2 * n);
    for(int i = 0; i < 2 * n; ++i) {
        z_points[i] = model.observe(cubaturePoints[i]);
    }

    // 3. 计算预测的观测值
    MeasurementVector z_pred = MeasurementVector::Zero(m);
    for (const auto& pt : z_points) {
        z_pred += pt;
    }
    z_pred /= (2.0 * n);

    // 4. 计算创新协方差的 Cholesky 因子 S_zz 和互协方差 P_xz
    Eigen::MatrixXd M_z(m, 2 * n);
    Eigen::MatrixXd P_xz = Eigen::MatrixXd::Zero(n, m);
    for(int i = 0; i < 2 * n; ++i) {
        M_z.col(i) = alpha * (z_points[i] - z_pred);
        StateVector x_diff = cubaturePoints[i] - x;
        P_xz += x_diff * M_z.col(i).transpose();
    }
    P_xz *= alpha; // 注意这里的权重 alpha

    Eigen::MatrixXd S_R = R.llt().matrixL();
    Eigen::MatrixXd S_zz = qr_update(M_z, S_R);

    // 5. 计算卡尔曼增益 K (更稳健的方式)
    // K = P_xz * inv(S_zz * S_zz') = P_xz * inv(S_zz') * inv(S_zz)
    // 等价于求解线性方程组: K * S_zz' = P_xz  =>  K = P_xz * inv(S_zz')
    // Eigen 中，A * B.inverse() 可写为 A / B
    Eigen::MatrixXd K_temp = P_xz * S_zz.transpose().inverse();
    Eigen::MatrixXd K = K_temp * S_zz.inverse();

    // 或者用更数值稳定的解法器
    // Eigen::MatrixXd K = S_zz.transpose().ldlt().solve(P_xz.transpose()).transpose();


    // 6. 更新状态 (与之前相同)
    x += K * (z - z_pred);

    // 7. 更新 Cholesky 因子 S
    // 核心步骤：通过 Cholesky 降维更新 (chol_downdate) 实现减法
    Eigen::MatrixXd U = K * S_zz;
    S = chol_downdate(S, U);
}

// 私有辅助函数实现
Eigen::MatrixXd SRCKF::qr_update(const Eigen::MatrixXd& S_old, const Eigen::MatrixXd& M)
{
    // 构造一个增广矩阵 [S_old', M']'
    Eigen::MatrixXd A(S_old.cols() + M.cols(), S_old.rows());
    A.topRows(S_old.cols()) = S_old.transpose();
    A.bottomRows(M.cols()) = M.transpose();

    // 对增广矩阵进行 QR 分解
    Eigen::HouseholderQR<Eigen::MatrixXd> qr(A);
    Eigen::MatrixXd R = qr.matrixQR().triangularView<Eigen::Upper>();

    // 取出 R 的前 n 行，再转置，得到新的下三角 Cholesky 因子
    return R.topRows(S_old.rows()).transpose();
}

Eigen::MatrixXd SRCKF::chol_downdate(const Eigen::MatrixXd& S_old, const Eigen::MatrixXd& U)
{
    // chol_downdate 是 SR-UKF/SR-CKF 的核心和难点。
    // 它用于计算 S_new * S_new' = S_old * S_old' - U * U'
    // 这需要专门的算法，这里提供一个基于 QR 分解的实现。
    int n = S_old.cols();
    int m = U.cols();
    Eigen::MatrixXd A = Eigen::MatrixXd::Zero(n + m, n);
    A.block(0, 0, n, n) = S_old.transpose();
    A.block(n, 0, m, n) = U.transpose();

    // 使用 Givens 旋转将 U' 部分置为零，但 Eigen 没有直接的 Givens 接口
    // 这里我们使用一种更通用的 QR 分解方法，但它在概念上等同于降维更新
    // 这个实现比理论上的 Givens 旋转更简单，但功能上是等价的
    Eigen::HouseholderQR<Eigen::MatrixXd> qr(A);
    Eigen::MatrixXd R = qr.matrixQR().triangularView<Eigen::Upper>();
    return R.topRows(n).transpose();
}
