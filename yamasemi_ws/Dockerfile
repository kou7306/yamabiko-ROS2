FROM ros:noetic

USER root

WORKDIR /root/yamasemi_ws

COPY src /root/yamasemi_ws/src

RUN <<EOF
  set -e
  apt-get update
  apt-get upgrade -y --no-install-recommends
  apt-get install -y --no-install-recommends ros-noetic-desktop
  rosdep update --rosdistro=noetic
  rosdep install -iyr --from-paths src --rosdistro=noetic
  apt-get clean
  rm -rf /var/lib/apt/lists/*
EOF
