/**
 * @file main.cpp
 * @brief 程序入口文件
 * @details 创建Service对象并执行其主要功能
 * @author xubb
 * @date 20250711
 */

#include "Service.h"

/**
 * @brief 程序主函数
 * @param argc 命令行参数数量
 * @param argv 命令行参数数组
 * @return 程序执行结果代码
 * @details 初始化Service对象并调用其exec方法执行主要服务功能
 */
int main(int argc, char **argv)
{
    Service service(argc, argv);
    return service.exec();
}

