/**
 * @file DataStructures.h
 * @brief 数据结构定义文件
 * @details 定义了系统中使用的基本数据结构，如观测数据等
 * @author xubb
 * @date 20250711
 */

#pragma once
#include <Eigen/Dense>
#include "nlohmann/json.hpp"

/**
 * @brief 3D向量类型别名
 * @details 使用Eigen库的Vector3d表示三维空间中的向量
 */
using Vector3 = Eigen::Vector3d;

/**
 * @brief JSON类型别名
 * @details 使用nlohmann/json库实现JSON数据处理
 */
using json = nlohmann::json;

/**
 * @brief 观测数据类
 * @details 表示单个观测点的位置、时间戳和观测者ID
 */
class Measurement {
public:
    /**
     * @brief 观测位置
     * @details 三维空间中的位置坐标(x,y,z)
     */
    Vector3 position;

    /**
     * @brief 观测时间戳
     * @details 观测数据的获取时间
     */
    double timestamp;

    /**
     * @brief 观测者ID
     * @details 产生此观测数据的观测者标识
     */
    int observerId;

    /**
     * @brief 默认构造函数
     */
    Measurement() = default;

    /**
     * @brief 带参数的构造函数
     * @param pos 观测位置
     * @param time 观测时间戳
     * @param obsId 观测者ID
     */
    Measurement(const Vector3& pos, double time, int obsId);
};
