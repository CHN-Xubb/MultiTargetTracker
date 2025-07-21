#ifndef CKF_H
#define CKF_H

#include "IMotionModel.h"
#include <vector>

class CKF
{
public:
    CKF();

    // 预测步骤
    void predict(StateVector& x, Eigen::MatrixXd& P,
                 const IMotionModel& model, double dt);

    // 更新步骤
    void update(StateVector& x, Eigen::MatrixXd& P,
                const IMotionModel& model,
                const MeasurementVector& z, const Eigen::MatrixXd& R);

private:
    // 计算 Cubature 点
    std::vector<StateVector> generateCubaturePoints(const StateVector& x, const Eigen::MatrixXd& P);
};

#endif // CKF_H