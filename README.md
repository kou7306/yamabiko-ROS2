# yamabiko

beego / T-frog motor driver を ROS 2 Humble + YP-Spur で動かすための最小ワークスペースです。
シリアルデバイスは `T-frog` の `by-id` パスに固定しています。

## ゼロからの環境構築

素の Ubuntu 22.04 / 24.04 マシンに、このリポジトリを動かす環境を構築する手順です。

### 1. ROS 2 Humble のインストール

```bash
# ロケール設定
sudo apt update && sudo apt install -y locales
sudo locale-gen en_US en_US.UTF-8
sudo update-locale LC_ALL=en_US.UTF-8 LANG=en_US.UTF-8
export LANG=en_US.UTF-8

# ROS 2 リポジトリの追加
sudo apt install -y software-properties-common curl
sudo add-apt-repository universe
sudo curl -sSL https://raw.githubusercontent.com/ros/rosdistro/master/ros.key \
  -o /usr/share/keyrings/ros-archive-keyring.gpg
echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/ros-archive-keyring.gpg] \
  http://packages.ros.org/ros2/ubuntu $(. /etc/os-release && echo $UBUNTU_CODENAME) main" \
  | sudo tee /etc/apt/sources.list.d/ros2.list > /dev/null

# インストール
sudo apt update
sudo apt install -y ros-humble-desktop

# colcon ビルドツール
sudo apt install -y python3-colcon-common-extensions
```

シェルに ROS 2 を自動読み込みさせる場合:

```bash
echo "source /opt/ros/humble/setup.bash" >> ~/.bashrc
source ~/.bashrc
```

### 2. ビルドに必要なパッケージ

```bash
sudo apt install -y cmake build-essential git
```

### 3. YP-Spur のインストール

```bash
cd /tmp
git clone https://github.com/openspur/yp-spur.git
cd yp-spur
```

**カーネル 6.8 以降の場合**（`uname -r` で確認）、ビルド前にパッチを適用する:

```bash
# カーネルバージョン確認
uname -r

# 6.8 以降なら以下を実行
sed -i '536s/serial_flush_out();/\/\/ serial_flush_out();/' src/serial.c
```

参考: https://github.com/openspur/yp-spur/issues/245

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
sudo make install
sudo ldconfig
```

### 4. シリアルデバイスの権限設定

毎回 `chmod` しなくて済むように、ユーザーを `dialout` グループに追加する:

```bash
sudo usermod -aG dialout $USER
```

反映にはログアウト→ログインが必要。

### 5. リポジトリのクローンとビルド

```bash
git clone https://github.com/kou7306/yamabiko.git
cd yamabiko/yamasemi_ws
source /opt/ros/humble/setup.bash
colcon build --packages-select beego_driver
source install/setup.bash
```

### 6. 動作確認

T-frog motor driver を USB 接続し、認識を確認:

```bash
ls /dev/serial/by-id/usb-T-frog_project_T-frog_Driver-if00
```

## 起動

### ターミナル 1: ypspur-coordinator

```bash
ypspur-coordinator \
  -p $(pwd)/src/beego_driver/config/beego.param \
  -d /dev/serial/by-id/usb-T-frog_project_T-frog_Driver-if00
```

`Trajectory control loop started.` が出れば OK。

### ターミナル 2: beego_driver ノード

```bash
source /opt/ros/humble/setup.bash
source install/setup.bash
ros2 run beego_driver beego_driver
```

`ypspur connected` が出れば OK。

### ターミナル 3: 動作テスト

```bash
# 前進 0.1 m/s
ros2 topic pub /cmd_vel geometry_msgs/msg/Twist "{linear: {x: 0.1}, angular: {z: 0.0}}" -1

# 停止
ros2 topic pub /cmd_vel geometry_msgs/msg/Twist "{linear: {x: 0.0}, angular: {z: 0.0}}" -1
```

### launch でまとめて起動する場合

```bash
cd yamabiko/yamasemi_ws
source /opt/ros/humble/setup.bash
source install/setup.bash
ros2 launch beego_driver beego_bringup_launch.py
```

## ノードの役割

`beego_driver` は以下を担当する。

- `/cmd_vel` を購読して `Spur_vel()` に流す
- YP-Spur から姿勢と速度を取得して `/odom` を publish
- wheel angle / velocity を取得して `/joint_states` を publish
- `odom -> base_footprint` の TF を publish
- 切断時は YP-Spur へ再接続を試みる

## URG（測域センサ）

北陽電機の URG LiDAR を ROS2 で使用する。

### インストール

```bash
sudo apt install -y ros-humble-urg-node
```

### URG の起動

URG を USB 接続し、認識を確認:

```bash
ls /dev/serial/by-id/ | grep Hokuyo
```

beego_driver と別ターミナルで起動:

```bash
source /opt/ros/humble/setup.bash
source install/setup.bash
ros2 launch beego_driver urg_launch.py
```

`/scan` トピックに `sensor_msgs/LaserScan` が配信される。

### rviz2 で可視化

```bash
rviz2
```

1. Fixed Frame を `laser` に変更
2. Add → By topic → `/scan` → LaserScan を選択

### デバイスパスの変更

`urg_launch.py` の `serial_port` パラメータを実際のデバイスパスに合わせること。
`ls /dev/serial/by-id/` で確認できる。

## トラブルシューティング

| 症状 | 対処 |
|------|------|
| デバイスが見つからない | USB ケーブル確認。`dmesg` で認識を確認 |
| `Permission denied` | `sudo chmod 777 /dev/ttyACM0` または `dialout` グループに追加 |
| 接続直後に切断 | カーネル 6.8+ のパッチを適用して YP-Spur を再ビルド |
| `error while loading shared libraries` | `sudo ldconfig` |
| `Firmware update is recommended` | 動作に問題なければ無視してよい |
