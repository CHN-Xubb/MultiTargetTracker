/**
 * @file MessageRelayManager.cpp
 * @brief 消息中继管理器实现文件
 * @details 实现了MessageRelayManager类的各种功能，包括消息的发送和接收
 * @author xubb
 * @date 20250711
 */

#include "MessageRelayManager.h"
#include <QCoreApplication>

// 定义统一的日志宏
#define LOG_DEBUG(msg) qDebug() << "[MessageRelayManager::" << __FUNCTION__ << "] " << msg
#define LOG_INFO(msg) qInfo() << "[MessageRelayManager::" << __FUNCTION__ << "] " << msg
#define LOG_WARN(msg) qWarning() << "[MessageRelayManager::" << __FUNCTION__ << "] " << msg
#define LOG_ERROR(msg) qCritical() << "[MessageRelayManager::" << __FUNCTION__ << "] " << msg

// 函数跟踪宏
#define LOG_FUNCTION_BEGIN() LOG_DEBUG("开始")
#define LOG_FUNCTION_END() LOG_DEBUG("结束")

/**
 * @brief 获取单例实例
 * @return MessageRelayManager的引用
 * @details 线程安全的单例实现，确保全局唯一的消息管理器实例
 */
MessageRelayManager& MessageRelayManager::getInstance()
{
    // C++11 保证了静态局部变量的初始化是线程安全的
    static MessageRelayManager instance;
    return instance;
}

/**
 * @brief 发送数据
 * @param data 要发送的模拟器数据
 * @return 是否发送成功
 * @details 通过模拟器数据接口发布消息
 */
bool MessageRelayManager::sendData(const SimulatorData &data)
{
    LOG_FUNCTION_BEGIN();

    if(m_pSimData) {
        bool result = m_pSimData->publishMessage(data);
        if(result) {
            LOG_DEBUG("消息发布成功");
        } else {
            LOG_WARN("消息发布失败");
        }
        LOG_FUNCTION_END();
        return result;
    } else {
        LOG_ERROR("模拟器数据接口为空，无法发送消息");
        LOG_FUNCTION_END();
        return false;
    }
}

/**
 * @brief 消息数据处理函数
 * @param data 接收到的模拟器数据
 * @details 实现ISimulatorDataListener接口的回调方法，接收外部消息并发出信号
 */
void MessageRelayManager::OnMsgData(SimulatorData data)
{
    LOG_FUNCTION_BEGIN();

    LOG_DEBUG("收到外部消息，大小: " + QString::number(data.json.size()) + " 字节");

    // 通过信号将消息广播给所有订阅者
    emit messageReceived(data.json);

    LOG_FUNCTION_END();
}

/**
 * @brief 构造函数
 * @param parent 父对象指针
 * @details 初始化消息中继管理器，创建模拟器数据接口并注册监听器
 */
MessageRelayManager::MessageRelayManager(QObject *parent)
    : QObject(parent),
      m_pSimData(nullptr)
{
    LOG_FUNCTION_BEGIN();

    // 获取模拟器数据实例
    QString ddsPath = QCoreApplication::applicationDirPath() + "/dds";
    LOG_INFO("初始化模拟器数据接口，DDS路径: " + ddsPath);

    m_pSimData = getSimulatorDataInstance(1, ddsPath);

    if(m_pSimData) {
        // 注册自身为监听器
        m_pSimData->registListener(this);
        LOG_INFO("成功初始化模拟器数据接口并注册监听器");
    } else {
        LOG_ERROR("获取模拟器数据实例失败");
    }

    LOG_INFO("消息中继管理器已创建");
    LOG_FUNCTION_END();
}

/**
 * @brief 析构函数
 * @details 释放模拟器数据接口资源
 */
MessageRelayManager::~MessageRelayManager()
{
    LOG_FUNCTION_BEGIN();

    if(m_pSimData) {
        delete m_pSimData;
        m_pSimData = nullptr;
        LOG_INFO("模拟器数据接口已释放");
    }

    LOG_INFO("消息中继管理器已销毁");
    LOG_FUNCTION_END();
}

/**
 * @brief 发送消息
 * @param data 消息数据（JSON字符串）
 * @details 将消息通过模拟器数据接口发布出去
 */
void MessageRelayManager::sendMessage(const std::string &data)
{
    LOG_FUNCTION_BEGIN();

    if(data.empty()) {
        LOG_WARN("尝试发送空消息，已忽略");
        LOG_FUNCTION_END();
        return;
    }

    LOG_DEBUG("准备发送消息，大小: " + QString::number(data.size()) + " 字节");

    // 设置中继数据
    m_relayData.json = data;

    // 发送数据
    bool result = sendData(m_relayData);
    if(result) {
        LOG_INFO("消息发送成功");
    } else {
        LOG_ERROR("消息发送失败");
    }

    LOG_FUNCTION_END();
}
