#include "Service.h"
//#include <QDebug>

#include <windows.h> // 用于 Windows API
#include <Shlwapi.h>
#include <string>    // 用于 C++ 字符串处理
#include <iostream>  // 用于调试输出

int main(int argc, char **argv)
{
    Service service(argc, argv);
    return service.exec();
}


