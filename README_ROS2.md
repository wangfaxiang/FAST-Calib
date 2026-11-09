# FAST-Calib (ROS2) 编译与运行指南

本文件是 FAST-Calib 的 **ROS2 版本** 编译与运行说明（算法逻辑与原 ROS1 版一致，仅语法迁移到 ROS2）。

## 1. 环境要求

- 操作系统：Ubuntu 20.04
- ROS 发行版：**ROS2 Galactic**
- 依赖：PCL >= 1.10、OpenCV >= 4.0（必须含 `aruco`/`contrib` 模块）、`rosbag2_cpp`、`rosbag2_storage`
- 构建工具：`colcon`

> 注意：本机若同时存在自编译的 OpenCV（如 `/usr/local` 下的精简版，缺少 `aruco`），
> 编译脚本会自动回退到系统 OpenCV（含 `contrib`）。详见文末“常见问题”。

## 2. 编译

```bash
# 若启用了 miniconda/conda，先退出（否则 colcon 会误用 conda 的 python）
conda deactivate

# 加载 ROS2 环境
source /opt/ros/galactic/setup.bash

# 进入项目目录并编译
cd /path/to/FAST-Calib
colcon build --symlink-install

# 加载本包环境
source install/setup.bash
```

编译产物：

- `fast_calib` —— 单场景标定
- `multi_fast_calib` —— 多场景联合标定

## 3. 运行

### 3.1 单场景标定

```bash
source /opt/ros/galactic/setup.bash
source install/setup.bash

ros2 launch fast_calib calib.launch.py
```

（`rviz` 默认打开，可在 launch 中关闭：`rviz:=false`）

### 3.2 多场景联合标定

先至少完成 3 个不同场景的单场景标定（每个场景都会把圆孔圆心追加写入
`output/circle_center_record.txt`），再运行：

```bash
source /opt/ros/galactic/setup.bash
source install/setup.bash

ros2 launch fast_calib multi_calib.launch.py
```

### 3.3 直接运行可执行文件

```bash
ros2 run fast_calib fast_calib
ros2 run fast_calib multi_fast_calib
```

## 4. 参数配置

大部分参数在 `config/qr_params.yaml` 中：

- 相机内参：`fx / fy / cx / cy / k1 / k2 / p1 / p2`
- 标定板几何：`marker_size / delta_width_qr_center / delta_height_qr_center / delta_width_circles / delta_height_circles / circle_radius`
- 距离滤波：`x_min / x_max / y_min / y_max / z_min / z_max`
- 雷达话题：`lidar_topic`

以下三个路径参数由 launch 文件动态提供（ROS2 参数文件不支持 `$(find ...)` 替换）：

- `bag_path`：雷达点云 bag 路径
- `image_path`：对应图像路径
- `output_path`：结果输出目录

需要修改数据路径时，编辑 `launch/calib.launch.py` / `launch/multi_calib.launch.py`
中对应的 `bag_path`、`image_path`、`output_path` 变量。

## 5. 数据格式

- 点云数据需为 **ROS2 bag**（sqlite3 格式，`.db3` 或对应目录）。
- 原 ROS1 的 `.bag` 需先用 `rosbags` 等工具转换为 ROS2 bag。
- 代码会自动识别 bag 中雷达消息类型：
  - `CustomMsg`（Livox 固态雷达，带 `line` 字段）
  - `PointCloud2`（机械/多线雷达，可选 `ring` 字段）

## 6. 常见问题

### 6.1 OpenCV 缺 `aruco` 或版本冲突

本机 `/usr/local` 下若有自编译 OpenCV（如 4.4，无 `aruco`），会与系统 OpenCV
（4.2，含 `contrib/aruco`）冲突。CMake 已做回退处理，但链接时仍可能出现
`libopencv_*.so.4.2 ... may conflict with libopencv_*.so.4.4` 这类 warning。

彻底解决方式（二选一）：

```bash
# 方式一：编译时显式指定系统 OpenCV
colcon build --symlink-install --cmake-args -DOpenCV_DIR=/usr/lib/x86_64-linux-gnu/cmake/opencv4

# 方式二：临时屏蔽 /usr/local 的自编译 OpenCV（重命名后重新编译）
sudo mv /usr/local/lib/cmake/opencv4 /usr/local/lib/cmake/opencv4.bak
```

### 6.2 辅助工具

`scripts/distance_filter_tool.py` 为 ROS1 离线工具（依赖 `rosbag`），本迁移未改动。
如需在 ROS2 下使用，请改用 `rosbags` 库。
