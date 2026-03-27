# yamabiko

beego ロボット（T-frog motor driver + URG LiDAR）を ROS 2 Humble + YP-Spur で動かすためのワークスペースです。

## 構成

```
ロボット操作 (cmd_vel)
    ↓
beego_driver_node  ← libypspur (IPC)
    ↓
ypspur-coordinator ← beego.param (制御パラメータ)
    ↓ (シリアル通信)
T-frog モータドライバ → モーター

URG LiDAR → urg_node → /scan (LaserScan)
```

---

## ゼロからの環境構築

素の Ubuntu 22.04 / 24.04 マシンを想定しています。

### 1. ROS 2 Humble のインストール

```bash
# ロケール設定
sudo apt update
sudo apt install -y locales
sudo locale-gen en_US en_US.UTF-8
sudo update-locale LC_ALL=en_US.UTF-8 LANG=en_US.UTF-8
export LANG=en_US.UTF-8

# ROS 2 リポジトリの追加
sudo apt install -y software-properties-common curl
sudo add-apt-repository universe
sudo curl -sSL https://raw.githubusercontent.com/ros/rosdistro/master/ros.key \
  -o /usr/share/keyrings/ros-archive-keyring.gpg
echo "deb [arch=$(dpkg --print-architecture) \
  signed-by=/usr/share/keyrings/ros-archive-keyring.gpg] \
  http://packages.ros.org/ros2/ubuntu \
  $(. /etc/os-release && echo $UBUNTU_CODENAME) main" \
  | sudo tee /etc/apt/sources.list.d/ros2.list > /dev/null

# インストール
sudo apt update
sudo apt install -y ros-humble-desktop
sudo apt install -y python3-colcon-common-extensions
```

`.bashrc` に追加しておくと毎回 source しなくて済む：

```bash
echo "source /opt/ros/humble/setup.bash" >> ~/.bashrc
source ~/.bashrc
```

### 2. ビルドツール

```bash
sudo apt install -y cmake build-essential git
```

### 3. YP-Spur のインストール

YP-Spur はモータドライバとの通信を担当するソフトウェアです。apt では提供されていないためソースからビルドします。

```bash
cd /tmp
git clone https://github.com/openspur/yp-spur.git
cd yp-spur
```

**カーネル 6.8 以降の場合はパッチが必要です。**
Linux kernel 6.8+ では `tcflush()` のバグにより、ypspur-coordinator が接続直後に切断されます。

```bash
# カーネルバージョン確認
uname -r

# 6.8 以降なら以下を実行してからビルド
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

### 4. URG ドライバのインストール

```bash
sudo apt install -y ros-humble-urg-node
```

### 5. シリアルデバイスの権限設定

USB デバイスにアクセスするため、ユーザーを `dialout` グループに追加します。
**これをやっておかないと、起動のたびに `sudo chmod` が必要になります。**

```bash
sudo usermod -aG dialout $USER
```

**反映にはログアウト → ログインが必要。**

正しく設定できたか確認：

```bash
groups  # dialout が含まれていれば OK
```

`dialout` 未設定の場合や、設定後にログアウトしていない場合は、起動前に毎回以下を実行する：

```bash
# デバイス番号は ls -l /dev/serial/by-id/ で確認
sudo chmod 777 /dev/ttyACM0  # T-frog
sudo chmod 777 /dev/ttyACM1  # URG
```

**注意**: USB を抜き差しすると番号が変わるため、その都度確認すること。

### 6. パラメータファイルの準備

ロボットの制御パラメータファイル（`beego.param`）はリポジトリに含まれていません。別途配布されたファイルを以下に配置してください：

```
yamasemi_ws/src/beego_driver/config/beego.param
```

### 7. リポジトリのクローンとビルド

```bash
git clone https://github.com/kou7306/yamabiko.git
cd yamabiko/yamasemi_ws
source /opt/ros/humble/setup.bash
colcon build --packages-select beego_driver
source install/setup.bash
```

---

## デバイスの確認

T-frog と URG を USB 接続し、認識されているか確認します。

```bash
ls /dev/serial/by-id/
```

以下のように表示されれば OK：

```
usb-Hokuyo_Data_Flex_for_USB_URG-Series_USB_Driver-if00   # URG
usb-T-frog_project_T-frog_Driver-if00                      # T-frog
```

実際のデバイスファイル（`ttyACM0`, `ttyACM1` など）を確認するには：

```bash
ls -l /dev/serial/by-id/
```

**注意**: USB を抜き差しすると `ttyACM` の番号が変わることがあります。
`by-id` パスは固定なので、起動時はこちらを使うのが安全です。

---

## 起動

以下すべて `yamabiko/yamasemi_ws` ディレクトリで作業する前提です。

```bash
cd yamabiko/yamasemi_ws
source /opt/ros/humble/setup.bash
source install/setup.bash
```

### ターミナル 1: ypspur-coordinator

T-frog モータドライバとの通信プロセスを起動します。

```bash
ypspur-coordinator \
  -p src/beego_driver/config/beego.param \
  -d /dev/serial/by-id/usb-T-frog_project_T-frog_Driver-if00
```

以下が出れば成功：

```
YP-Spur coordinator started.
Command analyzer started.
Trajectory control loop started.
```

**もし接続直後に切断される場合は、YP-Spur のカーネルパッチが適用されていません。**
「3. YP-Spur のインストール」を確認してください。

### ターミナル 2: beego_driver ノード

```bash
source /opt/ros/humble/setup.bash
source install/setup.bash
ros2 run beego_driver beego_driver
```

`ypspur connected` が出れば OK。

### ターミナル 3: URG（測域センサ）

```bash
source /opt/ros/humble/setup.bash
source install/setup.bash
ros2 launch beego_driver urg_launch.py
```

`Streaming data.` が出れば OK。`/scan` トピックに `LaserScan` が配信されます。

**URG のデバイスパスが異なる場合：**
`urg_launch.py` の `serial_port` を実際のパスに変更してリビルドしてください。

```bash
ls /dev/serial/by-id/ | grep Hokuyo  # 実際のパスを確認
# urg_launch.py を編集後
colcon build --packages-select beego_driver
source install/setup.bash
```

### ターミナル 4: 動作テスト

```bash
source /opt/ros/humble/setup.bash

# 前進 0.1 m/s
ros2 topic pub /cmd_vel geometry_msgs/msg/Twist \
  "{linear: {x: 0.1}, angular: {z: 0.0}}" -1

# 停止
ros2 topic pub /cmd_vel geometry_msgs/msg/Twist \
  "{linear: {x: 0.0}, angular: {z: 0.0}}" -1
```

### rviz2 で可視化

```bash
source /opt/ros/humble/setup.bash
rviz2
```

1. 左上の **Fixed Frame** を `odom` に変更
2. 左下 **Add** → **By topic** タブ → `/scan` → **LaserScan** を選択
3. 同様に `/odom` → **Odometry** も追加可能

### launch でまとめて起動する場合

ypspur-coordinator と beego_driver をまとめて起動できます（URG は別）：

```bash
source /opt/ros/humble/setup.bash
source install/setup.bash
ros2 launch beego_driver beego_bringup_launch.py
```

---

## ノードの役割

### beego_driver_node

ROS 2 と YP-Spur の橋渡しを行うノード。

| 機能 | 内容 |
|------|------|
| `/cmd_vel` 購読 | `Spur_vel()` でモーターに速度指令を送る |
| `/odom` 配信 | YP-Spur から位置・速度を取得して配信 |
| `/joint_states` 配信 | 車輪の角度・速度を配信 |
| TF 配信 | `odom → base_footprint` の座標変換 |
| 再接続 | ypspur-coordinator との接続が切れた場合に自動で再接続 |

### ypspur-coordinator

YP-Spur のプロセス。T-frog モータドライバとシリアル通信し、PID 制御・エンコーダ読み取りを行う。

### urg_node

北陽電機の URG LiDAR からスキャンデータを取得し `/scan` トピックに配信する ROS 2 ノード（apt パッケージ）。

---

## トラブルシューティング

| 症状 | 原因と対処 |
|------|----------|
| `ls /dev/serial/by-id/` に何も出ない | USB ケーブルを確認。`dmesg \| tail` で認識状況を確認 |
| `Permission denied` や `could not open serial device` | `dialout` グループに追加済みか確認（`groups` コマンド）。未設定なら `sudo chmod 777 /dev/ttyACMx` で一時対処 |
| ypspur-coordinator が接続直後に切断される | カーネル 6.8+ のパッチを適用して YP-Spur を再ビルド |
| `error while loading shared libraries` | `sudo ldconfig` を実行 |
| `Firmware update is recommended` | 動作に問題なければ無視してよい |
| URG の `/scan` が全部 0.019 | URG のレンズを清掃。回転しているか確認。それでも直らなければ故障の可能性 |
| `ttyACM` の番号が変わった | USB 抜き差しで番号は変わる。`ls -l /dev/serial/by-id/` で現在の番号を確認 |
| `Package 'beego_driver' not found` | `source install/setup.bash` を忘れている |
