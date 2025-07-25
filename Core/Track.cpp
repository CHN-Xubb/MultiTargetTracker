/**
 * @file Track.cpp
 * @brief 航迹跟踪类的实现文件
 * @details 实现了Track类的各种功能，包括状态估计、更新和轨迹预测
 * @author xubb
 * @date 20250711
 */

#include "Track.h"
#include "LogManager.h"
#include <QSettings>

// 定义统一的日志宏
#define LOG_DEBUG(msg) qDebug() << "[Track::" << __FUNCTION__ << "] " << msg
#define LOG_INFO(msg) qInfo() << "[Track::" << __FUNCTION__ << "] " << msg
#define LOG_WARN(msg) qWarning() << "[Track::" << __FUNCTION__ << "] " << msg
#define LOG_ERROR(msg) qCritical() << "[Track::" << __FUNCTION__ << "] " << msg

// 函数跟踪宏
#define LOG_FUNCTION_BEGIN() LOG_DEBUG("开始")
#define LOG_FUNCTION_END() LOG_DEBUG("结束")

/**
 * @brief 将Eigen向量转换为可打印的字符串
 * @param v 状态向量
 * @return 格式化的字符串表示
 */
static QString vectorToString(const StateVector& v) {
    QString s = "(";
    for(int i = 0; i < v.size(); ++i) {
        s += QString::number(v(i), 'f', 2);
        if (i < v.size() - 1) s += ", ";
    }
    s += ")";
    return s;
}

/**
 * @brief 构造函数
 * @param initialMeasurement 初始观测数据
 * @param trackId 航迹ID
 * @param model 运动模型
 * @details 基于初始观测和运动模型创建航迹
 */
Track::Track(const Measurement& initialMeasurement, int trackId, std::unique_ptr<IMotionModel> model)
    : m_id(trackId),
      m_model(std::move(model)),
      m_age(0),
      m_hits(1),
      m_misses(0),
      m_confirmationHits(0),
      maxMissesToDelete(0)
{
    LOG_FUNCTION_BEGIN();

    // 从配置文件读取参数
    QSettings settings("Server.ini", QSettings::IniFormat);

    // 读取观测噪声
    double measurement_noise_std = settings.value("KalmanFilter/measurementNoiseStd", 2.0).toDouble();
    LOG_DEBUG("观测噪声标准差: " + QString::number(measurement_noise_std));

    // 读取生命周期参数
    m_confirmationHits = settings.value("KalmanFilter/confirmationHits", 3).toInt();
    maxMissesToDelete = settings.value("KalmanFilter/maxMissesToDelete", 5).toInt();
    LOG_DEBUG("确认所需命中次数: " + QString::number(m_confirmationHits) +
              ", 删除所需丢失次数: " + QString::number(maxMissesToDelete));

    // 初始化状态向量
    m_x.resize(m_model->stateDim());
    m_x.head<3>() = initialMeasurement.position;
    m_x.tail(m_model->stateDim() - 3).setZero();

    // 初始化协方差矩阵 P
    m_P = m_model->getInitialCovariance();

    // 初始化观测噪声矩阵
    m_R = Eigen::MatrixXd::Identity(m_model->measurementDim(), m_model->measurementDim()) *
          std::pow(measurement_noise_std, 2);

    // 设置最后更新时间
    m_lastUpdateTime = initialMeasurement.timestamp;

    LOG_INFO("航迹 " + QString::number(m_id) + " 已创建。初始位置: (" +
             QString::number(initialMeasurement.position.x(), 'f', 2) + ", " +
             QString::number(initialMeasurement.position.y(), 'f', 2) + ", " +
             QString::number(initialMeasurement.position.z(), 'f', 2) + ")");

    LOG_DEBUG("初始状态向量: " + vectorToString(m_x));

    LOG_FUNCTION_END();
}

/**
 * @brief 析构函数
 */
Track::~Track() {
    LOG_INFO("航迹 " + QString::number(m_id) + " 已销毁。生命周期统计 - 年龄: " +
             QString::number(m_age) + ", 命中数: " + QString::number(m_hits) +
             ", 最后丢失数: " + QString::number(m_misses));
}

/**
 * @brief 预测航迹状态
 * @param dt 时间步长(秒)
 * @details 根据运动模型将航迹状态向前预测指定时间
 */
void Track::predict(double dt)
{
    if (dt <= 0) {
        LOG_DEBUG("时间步长为0或负值，跳过预测");
        return;
    }

    LOG_DEBUG("航迹 " + QString::number(m_id) + " 预测前状态: " + vectorToString(m_x));

    // 调用滤波器进行预测
    m_filter.predict(m_x, m_P, *m_model, dt);
    m_age++;

    LOG_DEBUG("航迹 " + QString::number(m_id) + " 预测后状态: " + vectorToString(m_x) +
              ", 时间步长: " + QString::number(dt) + "秒");
}

/**
 * @brief 更新航迹状态
 * @param measurement 观测数据
 * @details 使用观测数据更新航迹状态估计
 */
void Track::update(const Measurement& measurement)
{
    LOG_DEBUG("航迹 " + QString::number(m_id) + " 更新前状态: " + vectorToString(m_x));
    LOG_DEBUG("使用观测位置: (" +
              QString::number(measurement.position.x(), 'f', 2) + ", " +
              QString::number(measurement.position.y(), 'f', 2) + ", " +
              QString::number(measurement.position.z(), 'f', 2) + ")");

    // 调用滤波器进行更新
    m_filter.update(m_x, m_P, *m_model, measurement.position, m_R);

    // 更新航迹统计信息
    m_hits++;
    m_misses = 0;
    m_lastUpdateTime = measurement.timestamp;

    LOG_DEBUG("航迹 " + QString::number(m_id) + " 更新后状态: " + vectorToString(m_x));
    LOG_DEBUG("命中计数增加到: " + QString::number(m_hits) +
              ", 确认状态: " + (isConfirmed() ? "已确认" : "未确认"));
}

/**
 * @brief 预测未来轨迹
 * @param timeHorizon 预测时间范围(秒)
 * @param timeStep 预测时间步长(秒)
 * @return 未来位置点的向量
 * @details 基于当前状态和运动模型预测未来轨迹点
 */
std::vector<Vector3> Track::predictFutureTrajectory(double timeHorizon, double timeStep) const
{
    LOG_FUNCTION_BEGIN();

    std::vector<Vector3> trajectory;

    // 参数检查
    if (timeHorizon <= 0 || timeStep <= 0) {
        LOG_WARN("无效的预测参数: 时间范围=" + QString::number(timeHorizon) +
                 ", 时间步长=" + QString::number(timeStep));
        return trajectory;
    }

    // 从当前状态开始预测
    StateVector futureState = m_x;
    int pointCount = 0;

    // 生成预测轨迹点
    for (double t = timeStep; t <= timeHorizon; t += timeStep) {
        futureState = m_model->predict(futureState, timeStep);
        Vector3 position = m_model->observe(futureState);
        trajectory.push_back(position);
        pointCount++;

        LOG_DEBUG("预测点 " + QString::number(pointCount) + " 在t+" +
                 QString::number(t, 'f', 1) + "秒: (" +
                 QString::number(position.x(), 'f', 2) + ", " +
                 QString::number(position.y(), 'f', 2) + ", " +
                 QString::number(position.z(), 'f', 2) + ")");
    }

    LOG_DEBUG("生成了 " + QString::number(pointCount) + " 个预测轨迹点");
    LOG_FUNCTION_END();

    return trajectory;
}

/**
 * @brief 获取航迹ID
 * @return 航迹ID
 */
int Track::getId() const {
    return m_id;
}

/**
 * @brief 获取当前状态向量
 * @return 状态向量的常引用
 */
const StateVector& Track::getState() const {
    return m_x;
}

/**
 * @brief 获取命中次数
 * @return 命中次数
 */
int Track::getHits() const {
    return m_hits;
}

/**
 * @brief 获取丢失次数
 * @return 连续丢失次数
 */
int Track::getMisses() const {
    return m_misses;
}

/**
 * @brief 获取最后更新时间
 * @return 最后一次更新的时间戳
 */
double Track::getLastUpdateTime() const {
    return m_lastUpdateTime;
}

/**
 * @brief 检查航迹是否已确认
 * @return 如果航迹已确认则返回true
 */
bool Track::isConfirmed() const {
    return m_hits >= m_confirmationHits;
}

/**
 * @brief 检查航迹是否已丢失
 * @return 如果航迹已丢失则返回true
 */
bool Track::isLost() const {
    return m_misses > maxMissesToDelete;
}

/**
 * @brief 增加航迹丢失计数
 * @details 当航迹与任何观测都不匹配时调用
 */
void Track::incrementMisses()
{
    m_misses++;
    LOG_DEBUG("航迹 " + QString::number(m_id) + " 丢失计数增加到: " +
              QString::number(m_misses) + "/" + QString::number(maxMissesToDelete));
    if (isLost()) {
        LOG_INFO("航迹 " + QString::number(m_id) + " 已达到丢失阈值，将被删除");
    }
}
