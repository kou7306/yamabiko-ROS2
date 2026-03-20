#include "rclcpp/rclcpp.hpp"
#include "geometry_msgs/msg/twist.hpp"
#include "nav_msgs/msg/odometry.hpp"
#include "sensor_msgs/msg/joint_state.hpp"
#include <tf2/utils.h>
#include <tf2_geometry_msgs/tf2_geometry_msgs.hpp>
#include <tf2_ros/transform_broadcaster.h>
#include <chrono>

#include <ypspur.h>

class YamabikoDriver : public rclcpp::Node
{
public:
  YamabikoDriver() : Node("yamabiko_driver")
  {
    // パラメータ宣言
    declare_parameter("odom_frame_id", "odom");
    declare_parameter("base_frame_id", "base_footprint");
    declare_parameter("Hz", 40);
    declare_parameter("left_wheel_joint", "left_wheel_joint");
    declare_parameter("right_wheel_joint", "right_wheel_joint");
    declare_parameter("liner_vel_lim", 0.5);
    declare_parameter("liner_accel_lim", 0.5);
    declare_parameter("angular_vel_lim", 1.57);
    declare_parameter("angular_accel_lim", 1.57);
    declare_parameter("calculate_odom_from_ypspur", true);
    declare_parameter("publish_odom_tf", true);

    // パラメータ取得
    get_parameter("odom_frame_id", odom_frame_id_);
    get_parameter("base_frame_id", base_frame_id_);
    get_parameter("left_wheel_joint", left_wheel_joint_);
    get_parameter("right_wheel_joint", right_wheel_joint_);
    get_parameter("liner_vel_lim", liner_vel_lim_);
    get_parameter("liner_accel_lim", liner_accel_lim_);
    get_parameter("angular_vel_lim", angular_vel_lim_);
    get_parameter("angular_accel_lim", angular_accel_lim_);
    get_parameter("Hz", loop_hz_);
    get_parameter("calculate_odom_from_ypspur", odom_from_ypspur_);
    get_parameter("publish_odom_tf", publish_odom_tf_);

    dt_ = 1.0 / loop_hz_;

    // Publisher / Subscriber
    odom_pub_ = create_publisher<nav_msgs::msg::Odometry>("odom", 1);
    js_pub_ = create_publisher<sensor_msgs::msg::JointState>("joint_states", 1);
    tf_broadcaster_ = std::make_unique<tf2_ros::TransformBroadcaster>(*this);
    cmd_vel_sub_ = create_subscription<geometry_msgs::msg::Twist>(
        "cmd_vel", 10,
        std::bind(&YamabikoDriver::cmd_vel_cb, this, std::placeholders::_1));

    // 初期化
    reset_state();

    // YP-Spur接続
    if (!connect_ypspur()) {
      RCLCPP_ERROR(get_logger(), "YP-Spur への接続に失敗しました。ypspur-coordinator が起動しているか確認してください。");
    }

    // メインループ
    int period_ms = 1000 / loop_hz_;
    loop_timer_ = create_wall_timer(
        std::chrono::milliseconds(period_ms),
        std::bind(&YamabikoDriver::loop, this));

    RCLCPP_INFO(get_logger(), "Yamabiko driver started (%d Hz)", loop_hz_);
  }

private:
  // パラメータ
  std::string odom_frame_id_;
  std::string base_frame_id_;
  std::string left_wheel_joint_;
  std::string right_wheel_joint_;
  int loop_hz_;
  double liner_vel_lim_, liner_accel_lim_;
  double angular_vel_lim_, angular_accel_lim_;
  bool odom_from_ypspur_;
  bool publish_odom_tf_;
  double dt_;

  // ROS
  rclcpp::Publisher<nav_msgs::msg::Odometry>::SharedPtr odom_pub_;
  rclcpp::Publisher<sensor_msgs::msg::JointState>::SharedPtr js_pub_;
  rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr cmd_vel_sub_;
  rclcpp::TimerBase::SharedPtr loop_timer_;
  std::unique_ptr<tf2_ros::TransformBroadcaster> tf_broadcaster_;

  // 状態
  geometry_msgs::msg::Twist last_cmd_vel_;
  nav_msgs::msg::Odometry odom_;
  bool ypspur_connected_ = false;

  void reset_state()
  {
    last_cmd_vel_.linear.x = 0.0;
    last_cmd_vel_.angular.z = 0.0;
    odom_ = nav_msgs::msg::Odometry();
  }

  bool connect_ypspur()
  {
    if (Spur_init() > 0) {
      RCLCPP_INFO(get_logger(), "YP-Spur に接続しました");
      Spur_stop();
      Spur_free();
      Spur_set_pos_GL(0, 0, 0);
      Spur_set_vel(liner_vel_lim_);
      Spur_set_accel(liner_accel_lim_);
      Spur_set_angvel(angular_vel_lim_);
      Spur_set_angaccel(angular_accel_lim_);
      ypspur_connected_ = true;
      return true;
    }
    ypspur_connected_ = false;
    return false;
  }

  void cmd_vel_cb(const geometry_msgs::msg::Twist::SharedPtr msg)
  {
    last_cmd_vel_ = *msg;
    if (ypspur_connected_) {
      Spur_vel(msg->linear.x, msg->angular.z);
    }
  }

  void publish_joint_states()
  {
    double l_ang_pos = 0, r_ang_pos = 0;
    double l_wheel_vel = 0, r_wheel_vel = 0;
    YP_get_wheel_ang(&l_ang_pos, &r_ang_pos);
    YP_get_wheel_vel(&l_wheel_vel, &r_wheel_vel);

    sensor_msgs::msg::JointState js;
    js.header.stamp = now();
    js.header.frame_id = "base_link";
    js.name = {left_wheel_joint_, right_wheel_joint_};
    js.position = {-l_ang_pos, -r_ang_pos};
    js.velocity = {l_wheel_vel, r_wheel_vel};
    js_pub_->publish(js);
  }

  void publish_odometry()
  {
    double x, y, yaw, v, w;
    auto current_stamp = now();

    if (odom_from_ypspur_) {
      Spur_get_pos_GL(&x, &y, &yaw);
      Spur_get_vel(&v, &w);
    } else {
      v = last_cmd_vel_.linear.x;
      w = last_cmd_vel_.angular.z;
      yaw = tf2::getYaw(odom_.pose.pose.orientation) + dt_ * w;
      x = odom_.pose.pose.position.x + dt_ * v * cosf(yaw);
      y = odom_.pose.pose.position.y + dt_ * v * sinf(yaw);
    }

    tf2::Vector3 z_axis(0, 0, 1);
    auto quat = tf2::toMsg(tf2::Quaternion(z_axis, yaw));

    // Odometry メッセージ
    odom_.header.stamp = current_stamp;
    odom_.header.frame_id = odom_frame_id_;
    odom_.child_frame_id = base_frame_id_;
    odom_.pose.pose.position.x = x;
    odom_.pose.pose.position.y = y;
    odom_.pose.pose.position.z = 0;
    odom_.pose.pose.orientation = quat;
    odom_.twist.twist.linear.x = v;
    odom_.twist.twist.linear.y = 0;
    odom_.twist.twist.angular.z = w;
    odom_pub_->publish(odom_);

    // TF
    if (publish_odom_tf_) {
      geometry_msgs::msg::TransformStamped odom_trans;
      odom_trans.header.stamp = current_stamp;
      odom_trans.header.frame_id = odom_frame_id_;
      odom_trans.child_frame_id = base_frame_id_;
      odom_trans.transform.translation.x = x;
      odom_trans.transform.translation.y = y;
      odom_trans.transform.translation.z = 0;
      odom_trans.transform.rotation = quat;
      tf_broadcaster_->sendTransform(odom_trans);
    }
  }

  void loop()
  {
    if (!ypspur_connected_) {
      RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 5000,
                           "YP-Spur 未接続。再接続を試みます...");
      connect_ypspur();
      return;
    }

    if (YP_get_error_state()) {
      RCLCPP_WARN(get_logger(), "YP-Spur エラー検出。再接続を試みます...");
      ypspur_connected_ = false;
      connect_ypspur();
      return;
    }

    publish_odometry();
    publish_joint_states();
  }
};

int main(int argc, char *argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<YamabikoDriver>());
  rclcpp::shutdown();
  return 0;
}
