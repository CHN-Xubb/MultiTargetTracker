QT       += core network
TARGET   = MultiTargetTrackerService
TEMPLATE = app

CONFIG += c++11
CONFIG -= app_bundle

# The following define makes your compiler emit warnings if you use
# any feature of Qt which as been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

msvc{
 QMAKE_CFLAGS += /utf-8
 QMAKE_CXXFLAGS += /utf-8
}


INCLUDEPATH += $$PWD/dds
INCLUDEPATH += $$PWD/Core
INCLUDEPATH += $$PWD/Core/Filter
INCLUDEPATH += $$PWD/Core/Tracker
INCLUDEPATH += $$PWD/Service
INCLUDEPATH += $$PWD/External
INCLUDEPATH += $$PWD/Tools

DESTDIR += $$PWD/binr

SOURCES += main.cpp \
    Tools/LogManager.cpp \
    Service/MessageRelayManager.cpp \
    Service/Service.cpp \
    Service/Worker.cpp \
    Core/DataStructures.cpp \
    Core/ConstantVelocityModel.cpp \
    Core/Track.cpp \
    Core/TrackManager.cpp \
    Core/CKF.cpp \
    Service/HealthCheckServer.cpp \
    Core/ConstantAccelerationModel.cpp


HEADERS += \
    Tools/LogManager.h \
    Service/MessageRelayManager.h \
    Service/Service.h \
    Service/Worker.h \
    Core/DataStructures.h \
    Core/ConstantVelocityModel.h \
    Core/IMotionModel.h \
    Core/Track.h \
    Core/TrackManager.h \
    Core/CKF.h \
    Service/HealthCheckServer.h \
    Core/ConstantAccelerationModel.h

# 平台特定设置
win32 {
    # 链接 Windows 服务管理库
    LIBS += -ladvapi32
    RC_FILE = $$PWD/Res/resources.rc
}
