import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.conditions import IfCondition
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    pkg_dir = get_package_share_directory('fast_calib')

    params_file = os.path.join(pkg_dir, 'config', 'qr_params.yaml')
    rviz_config = os.path.join(pkg_dir, 'rviz_cfg', 'fast_livo2.rviz')

    # 数据路径：位于工作区源目录（不在安装目录下），按需修改
    ws_src = '/home/promote/data/home/promote/wfx/learn/github/FAST-Calib'
    bag_path = os.path.join(ws_src, 'rosbag2_2026_11_15-', 'rosbag2_2026_11_15-19_48_40')
    image_path = os.path.join(ws_src, 'calib_data', 'avia_multi_scene', '33.jpg')
    output_path = os.path.join(ws_src, 'output')

    rviz_arg = DeclareLaunchArgument('rviz', default_value='true')

    fast_calib_node = Node(
        package='fast_calib',
        executable='fast_calib',
        name='fast_calib',
        output='screen',
        parameters=[
            params_file,
            {
                'bag_path': bag_path,
                'image_path': image_path,
                'output_path': output_path,
            },
        ],
    )

    rviz_node = Node(
        package='rviz2',
        executable='rviz2',
        name='rviz2',
        arguments=['-d', rviz_config],
        condition=IfCondition(LaunchConfiguration('rviz')),
    )

    return LaunchDescription([
        rviz_arg,
        fast_calib_node,
        rviz_node,
    ])
