#!/usr/bin/env python3
"""订阅相机图像话题，保存第一帧为图片后自动退出。

用法（直接运行）：
    source /opt/ros/galactic/setup.bash
    python3 scripts/save_image_node.py

默认参数：
    topic     = /camera/color/image_raw
    save_path = output/camera_frame.jpg

可用 ROS2 参数覆盖（直接运行或 ros2 run 均可）：
    python3 scripts/save_image_node.py --ros-args \
        -p topic:=/camera/color/image_raw \
        -p save_path:=output/my_frame.jpg
"""

import os

import cv2
import rclpy
from cv_bridge import CvBridge
from rclpy.node import Node
from rclpy.qos import qos_profile_sensor_data
from sensor_msgs.msg import Image


class SaveImageNode(Node):
    def __init__(self):
        super().__init__('save_image_node')

        self.declare_parameter('topic', '/camera/color/image_raw')
        self.declare_parameter('save_path', 'output/camera_frame.jpg')

        self.topic = self.get_parameter('topic').get_parameter_value().string_value
        self.save_path = self.get_parameter('save_path').get_parameter_value().string_value

        self.bridge = CvBridge()
        self.saved = False

        # 相机图像通常是 best_effort（sensor_data），用 sensor_data 的订阅方
        # 既兼容 best_effort 也兼容 reliable 的发布方。
        self.sub = self.create_subscription(
            Image, self.topic, self.callback, qos_profile_sensor_data)

        self.get_logger().info(f'Waiting for first image on "{self.topic}" ...')

    def callback(self, msg: Image):
        if self.saved:
            return

        try:
            img = self.bridge.imgmsg_to_cv2(msg, desired_encoding='bgr8')
        except Exception as e:
            self.get_logger().error(f'Failed to convert image: {e}')
            return

        out_dir = os.path.dirname(os.path.abspath(self.save_path))
        os.makedirs(out_dir, exist_ok=True)

        if not cv2.imwrite(self.save_path, img):
            self.get_logger().error(f'Failed to write image to {self.save_path}')
            return

        self.saved = True
        self.get_logger().info(
            f'Saved {msg.width}x{msg.height} image -> {os.path.abspath(self.save_path)}')
        self.get_logger().info('One frame saved, shutting down.')
        rclpy.shutdown()


def main(args=None):
    rclpy.init(args=args)
    node = SaveImageNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        if rclpy.ok():
            node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
