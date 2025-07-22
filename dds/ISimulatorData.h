#pragma once

//SIMULATORDATA
#include <string>
struct SimulatorData
{
	std::string json;
};


class ISimulatorDataListener
{
public:
    virtual void OnMsgData(SimulatorData data)=0;
    virtual void OnMsgData(SimulatorData data,int nDomain,std::string strTopic)
    {}


};

class ISimulatorData
{
public:
    virtual ~ISimulatorData() = default;

    //注册订阅者
    virtual bool registListener(ISimulatorDataListener* pListener) = 0;

    //发布信息
    virtual bool publishMessage(const SimulatorData& data) = 0;

    virtual void close() = 0;

};


#ifdef _WIN32
extern "C" __declspec(dllexport) ISimulatorData* getSimulatorDataInter(int nDomainID,bool allowLose = true);
#else
extern "C" ISimulatorData* getSimulatorDataInter(int nDomainID,bool allowLose = true);
#endif
