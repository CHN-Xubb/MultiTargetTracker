#pragma once
#include "ISimulatorData.h"
#include <QLibrary>
#include <QCoreApplication>
#include <QDebug>
//传入域ID
inline ISimulatorData *getSimulatorDataInstance(int nDomainId,QString relativePath,bool allowLose = true)
{
#ifdef _WIN32
#ifdef DEBUG
    QLibrary lib(relativePath + "/SimulatorDatad.dll");
#endif
#ifdef NDEBUG
    QLibrary lib(relativePath + "/SimulatorData.dll");
#endif
    typedef ISimulatorData*(*InitFunc)(int,bool);
    InitFunc initFun = (InitFunc)lib.resolve("getSimulatorDataInter");
    if(initFun)
    {
        return initFun(nDomainId, allowLose);
    }
    return nullptr;
#else
#ifdef DEBUG
    QLibrary lib(relativePath + "/libSimulatorData.so");
#endif
#ifdef NDEBUG
    QLibrary lib(relativePath + "/libSimulatorData.so");
#endif
    typedef ISimulatorData*(*InitFunc)(int,bool);
    InitFunc initFun = (InitFunc)lib.resolve("getSimulatorDataInter");
    if(initFun)
    {
        return initFun(nDomainId, allowLose);
    }
    return nullptr;
#endif
}


