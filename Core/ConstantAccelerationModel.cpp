#include "ConstantAccelerationModel.h"
#include <QSettings>
#include <cmath>

using Vector3 = Eigen::Vector3d;

ConstantAccelerationModel::ConstantAccelerationModel()
    : m_stateDim(9), m_measurementDim(3),m_process_noise_std(0.0)
{
    // 从配置文件加载CA模型特定的过程噪声标准差
    QSettings settings("Server.ini", QSettings::IniFormat);
    // 我们为CA模型使用一个新的配置项
    m_process_noise_std = settings.value("KalmanFilter/processNoiseStd", 1.0).toDouble();
}

int ConstantAccelerationModel::stateDim() const { return m_stateDim; }
int ConstantAccelerationModel::measurementDim() const { return m_measurementDim; }

StateVector ConstantAccelerationModel::predict(const StateVector& x, double dt) const
{
    StateVector new_x = x;
    const Vector3 pos = x.segment<3>(0);
    const Vector3 vel = x.segment<3>(3);
    const Vector3 acc = x.segment<3>(6);

    // p_new = p_old + v * dt + 0.5 * a * dt^2
    new_x.segment<3>(0) = pos + vel * dt + 0.5 * acc * dt * dt;
    // v_new = v_old + a * dt
    new_x.segment<3>(3) = vel + acc * dt;
    // a_new = a_old (在没有过程噪声的情况下)
    new_x.segment<3>(6) = acc;

    return new_x;
}

MeasurementVector ConstantAccelerationModel::observe(const StateVector& x) const
{
    // 观测仍然是位置
    return x.head<3>();
}

Eigen::MatrixXd ConstantAccelerationModel::getProcessNoiseMatrix(double dt) const
{
    // 基于离散白噪声加加速度（jerk）模型计算Q矩阵
    double q = std::pow(m_process_noise_std, 2);

    // G 矩阵将 3D jerk 噪声映射到 9D 状态
    Eigen::MatrixXd G(9, 3);
    G.setZero();
    G.block<3, 3>(0, 0) = Eigen::Matrix3d::Identity() * 0.5 * dt * dt;
    G.block<3, 3>(3, 0) = Eigen::Matrix3d::Identity() * dt;
    G.block<3, 3>(6, 0) = Eigen::Matrix3d::Identity();


    // 更精确的模型应该使用更复杂的Q矩阵，但这是一个常用且有效的简化
    // Q = G * G' * q
    // 这里为了简化，我们直接构建Q矩阵
    Eigen::MatrixXd Q = Eigen::MatrixXd::Zero(9, 9);
    double dt2 = dt * dt;
    double dt3 = dt2 * dt;
    double dt4 = dt3 * dt;
    double dt5 = dt4 * dt;

    Eigen::Matrix3d q_pos = Eigen::Matrix3d::Identity() * (dt5 / 20.0);
    Eigen::Matrix3d q_vel = Eigen::Matrix3d::Identity() * (dt3 / 3.0);
    Eigen::Matrix3d q_acc = Eigen::Matrix3d::Identity() * dt;
    Eigen::Matrix3d q_pos_vel = Eigen::Matrix3d::Identity() * (dt4 / 8.0);
    Eigen::Matrix3d q_pos_acc = Eigen::Matrix3d::Identity() * (dt3 / 6.0);
    Eigen::Matrix3d q_vel_acc = Eigen::Matrix3d::Identity() * (dt2 / 2.0);


    Q.block<3,3>(0,0) = q_pos;
    Q.block<3,3>(0,3) = q_pos_vel;
    Q.block<3,3>(3,0) = q_pos_vel;
    Q.block<3,3>(0,6) = q_pos_acc;
    Q.block<3,3>(6,0) = q_pos_acc;
    Q.block<3,3>(3,3) = q_vel;
    Q.block<3,3>(3,6) = q_vel_acc;
    Q.block<3,3>(6,3) = q_vel_acc;
    Q.block<3,3>(6,6) = q_acc;


    return Q * q;
}

Eigen::MatrixXd ConstantAccelerationModel::getInitialCovariance() const
{
    QSettings settings("Server.ini", QSettings::IniFormat);
    double pos_uncertainty = settings.value("KalmanFilter/initialPositionUncertainty", 10.0).toDouble();
    double vel_uncertainty = settings.value("KalmanFilter/initialVelocityUncertainty", 100.0).toDouble();
    double acc_uncertainty = settings.value("KalmanFilter/initialAccelerationUncertainty", 10.0).toDouble();

    Eigen::MatrixXd P = Eigen::MatrixXd::Identity(m_stateDim, m_stateDim);
    P.block<3, 3>(0, 0) *= pos_uncertainty;
    P.block<3, 3>(3, 3) *= vel_uncertainty;
    P.block<3, 3>(6, 6) *= acc_uncertainty;
    return P;
}
