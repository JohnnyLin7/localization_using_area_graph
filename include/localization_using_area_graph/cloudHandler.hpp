/**
 * @file cloudHandler.hpp
 * @author Jiajie Zhang (ROS2 port)
 *         Fujing Xie (original ROS1 implementation)
 *         Sören Schwertfeger (original ROS1 implementation)
 * @brief Point cloud processing and localization using Area Graph map representation
 * @version 0.1
 * @date 2024-11-09
 * 
 * @details Core handler class for AGLoc system that processes 3D LiDAR point clouds 
 *          and performs localization within an Area Graph map. Key functionalities:
 *          
 *          - Point Cloud Processing:
 *            * Clutter removal and filtering
 *            * 3D to 2D projection for wall detection
 *            * Downsampling based on corridorness metric
 *            * Ray intersection with Area Graph polygons
 *
 *          - Localization:
 *            * Global localization using pose scoring
 *            * Pose tracking via weighted point-to-line ICP
 *            * Area detection and transition handling
 *            * Corridorness optimization for long hallways
 *
 *          - ROS2 Integration:
 *            * Point cloud subscription and processing
 *            * Transform broadcasting and handling
 *            * Parameter management
 *            * Visualization publishing
 *
 * Main changes from ROS1:
 *          - Updated message handling for ROS2
 *          - QoS profile configuration
 *          - Modernized parameter handling
 *          - Updated transform system to TF2
 *          - Improved thread safety
 *
 * @note This class inherits from CloudBase and uses CloudInitializer for
 *       initial pose estimation. It implements the core AGLoc algorithm described in:
 *       "Robust Lifelong Indoor LiDAR Localization using the Area Graph"
 * 
 * @dependencies
 *          - CloudBase - Base class providing common functionality
 *          - CloudInitializer - Global localization implementation
 *          - PCL - Point Cloud processing
 *          - Eigen - Matrix operations
 *          - OpenCV - Visualization
 *          - Message Filters - Point cloud synchronization
 *
 * @usage Example usage:
 * ```cpp
 * auto node = std::make_shared<CloudHandler>();
 * rclcpp::spin(node);
 * ```
 *
 * @warning This implementation assumes:
 *          - 3D LiDAR with dense point clouds (e.g., 64 beams)
 *          - Area Graph map with well-defined polygons
 *          - Sufficient wall visibility for localization
 * 
 * @see cloudBase.hpp, cloudInitializer.hpp, utility.hpp
 *
 * @copyright Copyright (c) 2024, ShanghaiTech University
 *            All rights reserved.
 */
#pragma once
#ifndef _CLOUD_HANDLER_HPP_
#define _CLOUD_HANDLER_HPP_

#include "utility.hpp"
#include "cloudBase.hpp"
#include "cloudInitializer.hpp"

#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/point_cloud2.hpp"
#include "sensor_msgs/msg/point_cloud.hpp"
#include "nav_msgs/msg/odometry.hpp"

// Message filters for ROS2
#include "message_filters/subscriber.h"
#include "message_filters/synchronizer.h"
#include "message_filters/sync_policies/approximate_time.hpp"

class CloudHandler : public CloudBase {
public:
    // ROS2 Subscribers
    rclcpp::Subscription<sensor_msgs::msg::PointCloud2>::SharedPtr subLaserCloud;
    rclcpp::Subscription<sensor_msgs::msg::PointCloud>::SharedPtr subInitialGuess;
    rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr subLiosamOdometry;

    // ROS2 Publishers
    rclcpp::Publisher<sensor_msgs::msg::PointCloud2>::SharedPtr pubinsideAreaPC;

    // Core functionality object
    CloudInitializer cloudInitializer;

    // Map related indices
    int insideAreaStartIndex;  // MAP PC index
    int insideAreaID;          // data structure AGindex index

    // Processing variables
    std::vector<bool> vbHistogramRemain;
    std::chrono::steady_clock::time_point sumFrameRunTime; // Using ROS2 time
    int numofFrame;
    bool getGuessOnce;
    int globalImgTimes;

    // Constructor and main functionality
    CloudHandler();

    // Point cloud processing methods
    void filterUsefulPoints();
    void optimizationICP();
    void showImg1line(const std::string& words);

    // Map and histogram processing
    void mergeMapHistogram();
    double corridornessDSRate(double maxPercentage);
    void gettingInsideWhichArea();

    // Point cloud checking methods
    bool checkWholeMap(int pc_index,
                      const pcl::PointXYZI& PCPoint,
                      double &map1x,
                      double &map1y,
                      double &map2x,
                      double &map2y,
                      double &intersectionx,
                      double &intersectiony);

    // Override methods from CloudBase
    void calClosestMapPoint(int inside_index) override;
    bool checkMap(int ring, 
                 int horizonIndex,
                 int &last_index,
                 double &minDist,
                 int inside_index) override;
    void allocateMemory() override;
    void resetParameters() override;

private:
    // Callback methods
    void cloudHandlerCB(const sensor_msgs::msg::PointCloud2::SharedPtr laserCloudMsg);
    void liosamOdometryIncrementalCB(const nav_msgs::msg::Odometry::SharedPtr odomMsg);
    void getInitialExtGuess(const sensor_msgs::msg::PointCloud::SharedPtr laserCloudMsg);

    // Initialize publishers and subscribers
    void initializePublishers() {
        auto qos = rclcpp::QoS(rclcpp::KeepLast(10));
        
        pubinsideAreaPC = this->create_publisher<sensor_msgs::msg::PointCloud2>(
            "insideAreaPC", qos);
    }

    void initializeSubscribers() {
        auto qos = rclcpp::QoS(rclcpp::KeepLast(10));
        
        subLaserCloud = this->create_subscription<sensor_msgs::msg::PointCloud2>(
            pointCloudTopic, qos,
            std::bind(&CloudHandler::cloudHandlerCB, this, std::placeholders::_1));
            
        subInitialGuess = this->create_subscription<sensor_msgs::msg::PointCloud>(
            "/particles_for_init", qos,
            std::bind(&CloudHandler::getInitialExtGuess, this, std::placeholders::_1));
            
        subLiosamOdometry = this->create_subscription<nav_msgs::msg::Odometry>(
            "/lio_sam/mapping/odometry", qos,
            std::bind(&CloudHandler::liosamOdometryIncrementalCB, this, std::placeholders::_1));
    }

    // Initialize variables
    void initializeVariables() {
        insideAreaStartIndex = 0;
        insideAreaID = 0;
        numofFrame = 0;
        getGuessOnce = false;
        globalImgTimes = 0;
        
        // Initialize time tracking
        sumFrameRunTime = std::chrono::steady_clock::now();
    }

protected:
    // Add any protected members if needed
};

#endif // _CLOUD_HANDLER_HPP_