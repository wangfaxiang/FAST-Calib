/* 
Developer: Chunran Zheng <zhengcr@connect.hku.hk>

This file is subject to the terms and conditions outlined in the 'LICENSE' file,
which is included as part of this source code package.
*/

#ifndef DATA_PREPROCESS_HPP
#define DATA_PREPROCESS_HPP

#include <fast_calib/msg/custom_msg.hpp>
#include <fast_calib/msg/custom_point.hpp>
#include <Eigen/Core>
#include <pcl/io/pcd_io.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl_conversions/pcl_conversions.h>
#include <rclcpp/rclcpp.hpp>
#include <rclcpp/serialization.hpp>
#include <rclcpp/serialized_message.hpp>
#include <rosbag2_cpp/reader.hpp>
#include <rosbag2_cpp/storage_options.hpp>
#include <rosbag2_cpp/converter_options.hpp>
#include <rosbag2_storage/storage_options.hpp>
#include <sensor_msgs/msg/point_cloud2.hpp>
#include <sensor_msgs/point_cloud2_iterator.hpp>
#include <fstream>
#include <cstring>
#include "common_lib.h"

using namespace std;

enum class LiDARType : int {
    Unknown = 0,
    Solid   = 1,   // 固态（如 Livox）
    Mech    = 2    // 机械式多线
};

class DataPreprocess
{
public:
    // 改成带线号的点云
    pcl::PointCloud<Common::Point>::Ptr cloud_input_;
    cv::Mat img_input_;
    LiDARType lidar_type_{LiDARType::Unknown};
    LiDARType lidarType() const { return lidar_type_; }

    DataPreprocess(Params &params)
        : cloud_input_(new pcl::PointCloud<Common::Point>)
    {
        string bag_path   = params.bag_path;
        string image_path = params.image_path;
        string lidar_topic = params.lidar_topic;

        auto logger = rclcpp::get_logger("fast_calib");

        // 读图像
        img_input_ = cv::imread(image_path, cv::IMREAD_UNCHANGED);
        if (img_input_.empty())
        {
            std::string msg = "Loading the image " + image_path + " failed";
            RCLCPP_ERROR_STREAM(logger, msg);
            return;
        }

        // 先检查包是否存在
        std::fstream file_;
        file_.open(bag_path, ios::in);
        if (!file_)
        {
            std::string msg = "Loading the rosbag " + bag_path + " failed";
            RCLCPP_ERROR_STREAM(logger, msg);
            return;
        }
        RCLCPP_INFO(logger, "Loading the rosbag %s", bag_path.c_str());

        rosbag2_cpp::Reader reader;
        rosbag2_storage::StorageOptions storage_options;
        storage_options.uri = bag_path;
        storage_options.storage_id = "sqlite3";
        rosbag2_cpp::ConverterOptions converter_options;
        converter_options.input_serialization_format = "cdr";
        converter_options.output_serialization_format = "cdr";
        try {
            reader.open(storage_options, converter_options);
        } catch (const std::exception &e) {
            RCLCPP_ERROR_STREAM(logger, "LOADING BAG FAILED: " << e.what());
            return;
        }

        // 查找 lidar topic 对应的消息类型
        std::string lidar_msg_type;
        for (const auto &topic_meta : reader.get_all_topics_and_types())
        {
            if (topic_meta.name == lidar_topic)
            {
                lidar_msg_type = topic_meta.type;
                break;
            }
        }

        bool is_custom_msg = (lidar_msg_type.find("CustomMsg") != std::string::npos);
        bool is_pc2        = (lidar_msg_type.find("PointCloud2") != std::string::npos);

        // 累计读取
        while (reader.has_next())
        {
            auto serialized_msg = reader.read_next();
            if (serialized_msg->topic_name != lidar_topic) continue;

            // 1) Livox 自定义消息（含 line 字段）
            if (is_custom_msg)
            {
                fast_calib::msg::CustomMsg livox_custom_msg;
                rclcpp::Serialization<fast_calib::msg::CustomMsg> serializer;
                rclcpp::SerializedMessage extracted;
                extracted.reserve(serialized_msg->serialized_data->buffer_length);
                memcpy(extracted.get_rcl_serialized_message().buffer,
                       serialized_msg->serialized_data->buffer,
                       serialized_msg->serialized_data->buffer_length);
                extracted.get_rcl_serialized_message().buffer_length =
                    serialized_msg->serialized_data->buffer_length;
                serializer.deserialize_message(&extracted, &livox_custom_msg);

                lidar_type_ = LiDARType::Solid;
                cloud_input_->reserve(livox_custom_msg.point_num);
                for (uint32_t i = 0; i < livox_custom_msg.point_num; ++i)
                {
                    Common::Point p;
                    p.x = livox_custom_msg.points[i].x;
                    p.y = livox_custom_msg.points[i].y;
                    p.z = livox_custom_msg.points[i].z;
                    // Livox 的 CustomPoint 有 line 字段（uint8 / uint16 视版本而定）
                    p.ring = static_cast<std::uint16_t>(livox_custom_msg.points[i].line);
                    cloud_input_->push_back(p);
                }
                continue;
            }

            // 2) 机械雷达 / 通用 PointCloud2
            if (is_pc2)
            {
                sensor_msgs::msg::PointCloud2 pcl_msg;
                rclcpp::Serialization<sensor_msgs::msg::PointCloud2> serializer;
                rclcpp::SerializedMessage extracted;
                extracted.reserve(serialized_msg->serialized_data->buffer_length);
                memcpy(extracted.get_rcl_serialized_message().buffer,
                       serialized_msg->serialized_data->buffer,
                       serialized_msg->serialized_data->buffer_length);
                extracted.get_rcl_serialized_message().buffer_length =
                    serialized_msg->serialized_data->buffer_length;
                serializer.deserialize_message(&extracted, &pcl_msg);

                // 优先判断是否有 ring 字段
                bool has_ring = false;
                for (const auto &f : pcl_msg.fields)
                {
                    if (f.name == "ring") { has_ring = true; break; }
                }

                // 使用 iterator 安全读取
                sensor_msgs::PointCloud2ConstIterator<float> it_x(pcl_msg, "x");
                sensor_msgs::PointCloud2ConstIterator<float> it_y(pcl_msg, "y");
                sensor_msgs::PointCloud2ConstIterator<float> it_z(pcl_msg, "z");

                // ring 可能不存在：不存在时用 0xFFFF 表示未知
                std::unique_ptr<sensor_msgs::PointCloud2ConstIterator<std::uint16_t>> it_ring_ptr;
                if (has_ring)
                {
                    it_ring_ptr.reset(new sensor_msgs::PointCloud2ConstIterator<std::uint16_t>(pcl_msg, "ring"));
                    lidar_type_ = LiDARType::Mech;
                }
                else
                {
                    lidar_type_ = LiDARType::Solid;
                }

                const size_t n = static_cast<size_t>(pcl_msg.width) * pcl_msg.height;
                cloud_input_->reserve(n);

                // cout << "Loading PointCloud2 with " << n << " points. Has ring: " << has_ring << endl;

                for (size_t i = 0; i < n; ++i, ++it_x, ++it_y, ++it_z)
                {
                    Common::Point p;
                    p.x = *it_x;
                    p.y = *it_y;
                    p.z = *it_z;

                    if (has_ring)
                    {
                        // 解引用 ring 迭代器并前进
                        p.ring = **it_ring_ptr;
                        ++(*it_ring_ptr);
                        // if (i % 32 == 0) cout << "ring: " << p.ring << endl;
                        // if (i % 32 == 1) cout << "ring: " << p.ring << endl;
                    }
                    else
                    {
                        p.ring = 0xFFFF; // 未知线号
                    }

                    cloud_input_->push_back(p);
                }
                continue;
            }

            // 其他类型忽略
        }

        RCLCPP_INFO(logger, "Loaded %zu points from the rosbag.", cloud_input_->size());
    }
};

typedef std::shared_ptr<DataPreprocess> DataPreprocessPtr;

#endif // DATA_PREPROCESS_HPP