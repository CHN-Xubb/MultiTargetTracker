#include <QCoreApplication>
#include "Service.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    a.setApplicationName("MultiTargetTrackerService");
    a.setApplicationVersion("1.0");

    // 创建并启动服务
    Service service;
    service.start();

    // 启动Qt事件循环
    return a.exec();
}


