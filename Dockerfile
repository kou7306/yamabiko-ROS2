FROM ros:humble

# 必要なパッケージのインストール
RUN apt-get update && apt-get install -y \
    cmake build-essential git \
    ros-humble-urg-node \
    python3-colcon-common-extensions \
    && rm -rf /var/lib/apt/lists/*

# YP-Spur のビルド・インストール（カーネル 6.8+ パッチ適用済み）
RUN cd /tmp \
    && git clone https://github.com/openspur/yp-spur.git \
    && cd yp-spur \
    && sed -i '536s/serial_flush_out();/\/\/ serial_flush_out();/' src/serial.c \
    && mkdir build && cd build \
    && cmake .. \
    && make -j$(nproc) \
    && make install \
    && ldconfig \
    && rm -rf /tmp/yp-spur

# ワークスペースのコピーとビルド
WORKDIR /ws
COPY yamasemi_ws/src /ws/src
RUN . /opt/ros/humble/setup.sh \
    && colcon build --packages-select beego_driver

# エントリポイント
COPY docker-entrypoint.sh /docker-entrypoint.sh
RUN chmod +x /docker-entrypoint.sh
ENTRYPOINT ["/docker-entrypoint.sh"]
