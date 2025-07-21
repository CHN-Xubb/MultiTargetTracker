#ifndef CONSTANTVELOCITYMODEL_H
#define CONSTANTVELOCITYMODEL_H

#include "IMotionModel.h"

class ConstantVelocityModel : public IMotionModel
{
public:
    ConstantVelocityModel();

    int stateDim() const override;
    int measurementDim() const override;

    StateVector predict(const StateVector& x, double dt) const override;

    MeasurementVector observe(const StateVector& x) const override;


    Eigen::MatrixXd getProcessNoiseMatrix(double dt) const override;

    Eigen::MatrixXd getInitialCovariance() const override;

private:
    int m_stateDim;
    int m_measurementDim;
    double m_process_noise_std; // 保存噪声标准差以供计算时使用
};

#endif // CONSTANTVELOCITYMODEL_H
