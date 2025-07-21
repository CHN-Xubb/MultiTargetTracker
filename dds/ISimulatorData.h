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

    //ע�ᶩ����
    virtual bool registListener(ISimulatorDataListener* pListener)=0;

    //������Ϣ
    virtual bool publishMessage(const SimulatorData& data)=0;

};

extern "C" __declspec(dllexport) ISimulatorData* getSimulatorDataInter(int nDomainID,bool allowLose = true);

