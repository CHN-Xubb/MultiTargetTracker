#ifndef CONSTANTACCELERATIONMODEL_H
#define CONSTANTACCELERATIONMODEL_H

#include "IMotionModel.h"

class ConstantAccelerationModel : public IMotionModel
{
public:
    ConstantAccelerationModel();

    int stateDim() const override;
    int measurementDim() const override;

    StateVector predict(const StateVector& x, double dt) const override;
    MeasurementVector observe(const StateVector& x) const override;

    Eigen::MatrixXd getProcessNoiseMatrix(double dt) const override;
    Eigen::MatrixXd getInitialCovariance() const override;

private:
    int m_stateDim;
    int m_measurementDim;
    double m_process_noise_std; // 加速度过程噪声的标准差 (jerk)
};

#endif // CONSTANTACCELERATIONMODEL_H
