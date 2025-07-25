#include "ConstantVelocityModel.h"
#include <QSettings>
#include <cmath>

ConstantVelocityModel::ConstantVelocityModel()
    : m_stateDim(6), m_measurementDim(3)
{
    // 只从配置中加载噪声标准差，并保存为成员变量
    QSettings settings("Server.ini", QSettings::IniFormat);
    m_process_noise_std = settings.value("KalmanFilter/processNoiseStd", 5.0).toDouble();
}

int ConstantVelocityModel::stateDim() const { return m_stateDim; }
int ConstantVelocityModel::measurementDim() const { return m_measurementDim; }

StateVector ConstantVelocityModel::predict(const StateVector& x, double dt) const
{
    StateVector new_x = x;
    new_x.head<3>() += x.tail<3>() * dt;
    return new_x;
}

MeasurementVector ConstantVelocityModel::observe(const StateVector& x) const
{
    // 观测的是位置
    return x.head<3>();
}


// --- 修改点: 实现新的、依赖于 dt 的 Q 矩阵计算 ---
Eigen::MatrixXd ConstantVelocityModel::getProcessNoiseMatrix(double dt) const
{
    // 基于离散白噪声加速度模型，计算依赖于 dt 的 Q 矩阵
    // Q = G * G' * q, 其中 q 是加速度噪声的方差

    double q = std::pow(m_process_noise_std, 2);

    Eigen::MatrixXd G(6, 3);
    G << 0.5 * dt * dt, 0, 0,
         0, 0.5 * dt * dt, 0,
         0, 0, 0.5 * dt * dt,
         dt, 0, 0,
         0, dt, 0,
         0, 0, dt;

    return G * G.transpose() * q;
}


Eigen::MatrixXd ConstantVelocityModel::getInitialCovariance() const
{
    // (可选) 同样可以将这些值配置化
    QSettings settings("Server.ini", QSettings::IniFormat);
    double pos_uncertainty = settings.value("KalmanFilter/initialPositionUncertainty", 10.0).toDouble();
    double vel_uncertainty = settings.value("KalmanFilter/initialVelocityUncertainty", 100.0).toDouble();

    Eigen::MatrixXd P = Eigen::MatrixXd::Identity(m_stateDim, m_stateDim);
    P.block<3, 3>(0, 0) *= pos_uncertainty;
    P.block<3, 3>(3, 3) *= vel_uncertainty;
    return P;
}
