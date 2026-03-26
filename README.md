# yamabiko

beego / T-frog motor driver を ROS 2 Humble + YP-Spur で動かすための最小ワークスペースです。
シリアルデバイスは `T-frog` の `by-id` パスに固定しています。

## 前提環境

- Ubuntu 22.04 / 24.04
- ROS 2 Humble
- T-frog motor driver が USB 接続されていること

確認コマンド:

```bash
echo $ROS_DISTRO          # humble
ls /dev/serial/by-id/     # usb-T-frog_project_T-frog_Driver-if00 が見えること
```

## セットアップ

### 1. YP-Spur のインストール

```bash
cd /tmp
git clone https://github.com/openspur/yp-spur.git
cd yp-spur
mkdir build && cd build
cmake ..
make -j$(nproc)
sudo make install
sudo ldconfig
```

### カーネル 6.8 以降のワークアラウンド

Linux kernel 6.8+ では `tcflush()` のバグにより ypspur-coordinator が接続直後に切断される。
ビルド前に以下のパッチを適用すること。

参考: https://github.com/openspur/yp-spur/issues/245

```diff
--- a/src/serial.c
+++ b/src/serial.c
@@ -533,7 +533,7 @@ int encode_write(char* data, int len)
   {
     return -1;
   }
-  serial_flush_out();
+  // serial_flush_out();

   return 0;
 }
```

### 2. ワークスペースのビルド

```bash
cd yamasemi_ws
source /opt/ros/humble/setup.bash
colcon build --packages-select beego_driver
source install/setup.bash
```

## 起動

### ターミナル 1: ypspur-coordinator

```bash
sudo chmod 777 /dev/ttyACM0
ypspur-coordinator \
  -p $(pwd)/yamasemi_ws/src/beego_driver/config/beego.param \
  -d /dev/serial/by-id/usb-T-frog_project_T-frog_Driver-if00
```

`Trajectory control loop started.` が出れば OK。

または launch で coordinator とノードをまとめて起動:

```bash
cd yamasemi_ws
source /opt/ros/humble/setup.bash
source install/setup.bash
ros2 launch beego_driver beego_bringup_launch.py
```

### ターミナル 2: beego_driver ノード (手動起動の場合)

```bash
source /opt/ros/humble/setup.bash
source yamasemi_ws/install/setup.bash
ros2 run beego_driver beego_driver
```

### ターミナル 3: 動作テスト

```bash
# 前進 0.1 m/s
ros2 topic pub /cmd_vel geometry_msgs/msg/Twist "{linear: {x: 0.1}, angular: {z: 0.0}}" -1

# 停止
ros2 topic pub /cmd_vel geometry_msgs/msg/Twist "{linear: {x: 0.0}, angular: {z: 0.0}}" -1
```

## ノードの役割

`beego_driver` は以下を担当する。

- `/cmd_vel` を購読して `Spur_vel()` に流す
- YP-Spur から姿勢と速度を取得して `/odom` を publish
- wheel angle / velocity を取得して `/joint_states` を publish
- `odom -> base_footprint` の TF を publish
- 切断時は YP-Spur へ再接続を試みる

## トラブルシューティング

| 症状 | 対処 |
|------|------|
| デバイスが見つからない | USB ケーブル確認。`dmesg` で認識を確認 |
| `Permission denied` | `sudo chmod 777 /dev/ttyACM0` |
| 接続直後に切断 | カーネル 6.8+ のワークアラウンドを適用 |
| `error while loading shared libraries` | `sudo ldconfig` |
| `Firmware update is recommended` | 動作に問題なければ無視してよい |
