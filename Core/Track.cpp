#include "Track.h"
#include "LogManager.h"
#include <QSettings>


// 一个辅助函数，用于将 Eigen 向量转换为可打印的 QString
static QString vectorToString(const StateVector& v) {
    QString s = "(";
    for(int i = 0; i < v.size(); ++i) {
        s += QString::number(v(i), 'f', 2);
        if (i < v.size() - 1) s += ", ";
    }
    s += ")";
    return s;
}

Track::Track(const Measurement& initialMeasurement, int trackId, std::unique_ptr<IMotionModel> model)
    : m_id(trackId), m_model(std::move(model)), m_age(0), m_hits(1),
      m_misses(0),m_confirmationHits(0),maxMissesToDelete(0)
{

    QSettings settings("Server.ini", QSettings::IniFormat);
    // 读取观测噪声
    double measurement_noise_std = settings.value("KalmanFilter/measurementNoiseStd", 2.0).toDouble();
    // 读取生命周期参数
    m_confirmationHits = settings.value("KalmanFilter/confirmationHits", 3).toInt();
    maxMissesToDelete = settings.value("KalmanFilter/maxMissesToDelete", 5).toInt();


    // 初始化状态
    m_x.resize(m_model->stateDim());
    m_x.head<3>() = initialMeasurement.position;
    m_x.tail(m_model->stateDim() - 3).setZero();

    // 初始化协方差矩阵 P
    m_P = m_model->getInitialCovariance();

    // 初始化观测噪声
    m_R = Eigen::MatrixXd::Identity(m_model->measurementDim(), m_model->measurementDim()) * std::pow(measurement_noise_std, 2);

    m_lastUpdateTime = initialMeasurement.timestamp;

    //qDebug() << "[创建] 新航迹" << m_id << "已创建。初始状态:" << vectorToString(m_x);
}

Track::~Track() {
    //qInfo() << "航迹" << m_id << "已销毁。";
}

void Track::predict(double dt)
{
    if (dt <= 0) return;

    //qDebug().nospace() << "[预测] 航迹 " << m_id << " | 之前: " << vectorToString(m_x);

    // 调用纯算法进行预测，传递 P
    m_filter.predict(m_x, m_P, *m_model, dt);
    m_age++;

    //qDebug().nospace() << "[预测] 航迹 " << m_id << " | 之后:  " << vectorToString(m_x) << " (dt=" << dt << "s)";
}

void Track::update(const Measurement& measurement)
{
    //qDebug().nospace() << "[更新] 航迹 " << m_id << " | 之前: " << vectorToString(m_x);
    //qDebug() << "         使用量测位置: (" << measurement.position.x() << "," << measurement.position.y() << "," << measurement.position.z() << ")";

    // 调用纯算法进行更新，传递 P
    m_filter.update(m_x, m_P, *m_model, measurement.position, m_R);

    m_hits++;
    m_misses = 0;
    m_lastUpdateTime = measurement.timestamp;

    //qDebug().nospace() << "[更新] 航迹 " << m_id << " | 之后:  " << vectorToString(m_x);
    //qDebug() << "         命中！命中计数现在是:" << m_hits;
}

std::vector<Vector3> Track::predictFutureTrajectory(double timeHorizon, double timeStep) const
{
    std::vector<Vector3> trajectory;
    if (timeHorizon <= 0 || timeStep <= 0) {
        return trajectory;
    }

    StateVector futureState = m_x;

    for (double t = timeStep; t <= timeHorizon; t += timeStep) {
        futureState = m_model->predict(futureState, timeStep);
        trajectory.push_back(m_model->observe(futureState));
    }

    return trajectory;
}

// --- 航迹管理实现 (无变化) ---
int Track::getId() const { return m_id; }
const StateVector& Track::getState() const { return m_x; }
int Track::getHits() const { return m_hits; }
int Track::getMisses() const { return m_misses; }
double Track::getLastUpdateTime() const { return m_lastUpdateTime; }
bool Track::isConfirmed() const { return m_hits >= m_confirmationHits; }
bool Track::isLost() const { return m_misses > maxMissesToDelete; }
void Track::incrementMisses()
{
    m_misses++;
    //qDebug() << "[管理] 航迹" << m_id << "丢失！丢失计数现在是:" << m_misses;
}
