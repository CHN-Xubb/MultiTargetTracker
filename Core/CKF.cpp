#include "CKF.h"

CKF::CKF() {}

// 预测步骤 (使用完整协方差矩阵 P)
void CKF::predict(StateVector& x, Eigen::MatrixXd& P, const IMotionModel& model, double dt)
{
    const int n = model.stateDim();

    // 1. 生成 2n 个 Cubature 点
    auto cubaturePoints = generateCubaturePoints(x, P);

    // 2. 通过状态转移模型传递 Cubature 点
    for (int i = 0; i < 2 * n; ++i) {
        cubaturePoints[i] = model.predict(cubaturePoints[i], dt);
    }

    // 3. 计算预测的均值
    StateVector x_pred = StateVector::Zero(n);
    for (int i = 0; i < 2 * n; ++i) {
        x_pred += cubaturePoints[i];
    }
    x_pred /= (2.0 * n);
    x = x_pred; // 更新状态

    // 4. 计算预测的协方差矩阵
    Eigen::MatrixXd P_pred = Eigen::MatrixXd::Zero(n, n);
    for (int i = 0; i < 2 * n; ++i) {
        StateVector diff = cubaturePoints[i] - x_pred;
        P_pred += diff * diff.transpose();
    }
    P_pred /= (2.0 * n);

    // --- 修改点: 调用新的 getProcessNoiseMatrix 方法并传入 dt ---
    P_pred += model.getProcessNoiseMatrix(dt); // 加上过程噪声

    P = P_pred; // 更新协方差
}

// ... (update 和 generateCubaturePoints 方法保持不变) ...

// 更新步骤 (使用完整协方差矩阵 P)
void CKF::update(StateVector& x, Eigen::MatrixXd& P, const IMotionModel& model,
                 const MeasurementVector& z, const Eigen::MatrixXd& R)
{
    const int n = model.stateDim();
    const int m = model.measurementDim();

    // 1. 基于预测后的状态，生成新的 Cubature 点
    auto cubaturePoints = generateCubaturePoints(x, P);

    // 2. 通过观测模型传递 Cubature 点
    std::vector<MeasurementVector> z_points(2 * n);
    for(int i = 0; i < 2 * n; ++i) {
        z_points[i] = model.observe(cubaturePoints[i]);
    }

    // 3. 计算预测的观测值
    MeasurementVector z_pred = MeasurementVector::Zero();
    for (int i = 0; i < 2 * n; ++i) {
        z_pred += z_points[i];
    }
    z_pred /= (2.0 * n);

    // 4. 计算创新协方差 Pzz 和互协方差 Pxz
    Eigen::MatrixXd P_zz = Eigen::MatrixXd::Zero(m, m);
    Eigen::MatrixXd P_xz = Eigen::MatrixXd::Zero(n, m);
    for(int i = 0; i < 2 * n; ++i) {
        MeasurementVector z_diff = z_points[i] - z_pred;
        StateVector x_diff = cubaturePoints[i] - x;
        P_zz += z_diff * z_diff.transpose();
        P_xz += x_diff * z_diff.transpose();
    }
    P_zz /= (2.0 * n);
    P_xz /= (2.0 * n);
    P_zz += R; // 加上观测噪声

    // 5. 计算卡尔曼增益 K
    Eigen::MatrixXd K = P_xz * P_zz.inverse();

    // 6. 更新状态和协方差
    x += K * (z - z_pred);
    P -= K * P_zz * K.transpose();
}


std::vector<StateVector> CKF::generateCubaturePoints(const StateVector& x, const Eigen::MatrixXd& P)
{
    const int n = x.rows();
    std::vector<StateVector> points(2 * n);

    // 使用 Cholesky分解计算协方差的平方根
    Eigen::MatrixXd S = P.llt().matrixL();
    Eigen::MatrixXd term = std::sqrt(static_cast<double>(n)) * S;

    for (int i = 0; i < n; ++i) {
        points[i]       = x + term.col(i);
        points[i + n]   = x - term.col(i);
    }
    return points;
}
