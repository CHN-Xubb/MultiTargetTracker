#include "MessageRelayManager.h"

MessageRelayManager& MessageRelayManager::getInstance()
{
    // C++11 保证了静态局部变量的初始化是线程安全的
    static MessageRelayManager instance;
    return instance;
}

bool MessageRelayManager::sendData(const SimulatorData &data)
{
    if(m_pSimData){
        return m_pSimData->publishMessage(data);
    }
    return false;
}

void MessageRelayManager::OnMsgData(SimulatorData data)
{
    emit messageReceived(data.json);
}

MessageRelayManager::MessageRelayManager(QObject *parent) : QObject(parent)
{
    m_pSimData = getSimulatorDataInstance(1, QCoreApplication::applicationDirPath() + "/dds");
    if(m_pSimData){
        m_pSimData->registListener(this);
    }
}

MessageRelayManager::~MessageRelayManager()
{
    if(m_pSimData){
        delete m_pSimData;
        m_pSimData = nullptr;
    }
}

void MessageRelayManager::sendMessage(const std::string &data)
{
    if(data.empty())
    {
        return;
    }
    m_relayData.json = data;
    sendData(m_relayData);
}
