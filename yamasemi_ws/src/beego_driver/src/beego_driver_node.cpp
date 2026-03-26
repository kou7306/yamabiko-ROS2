#include "rclcpp/rclcpp.hpp"
#include "geometry_msgs/msg/twist.hpp"
#include "nav_msgs/msg/odometry.hpp"
#include "sensor_msgs/msg/joint_state.hpp"
#include <tf2/utils.h>
#include <tf2_geometry_msgs/tf2_geometry_msgs.hpp>
#include <tf2_ros/transform_broadcaster.h>
#include <chrono>

#include <ypspur.h>

class BeegoDriver : public rclcpp::Node
{
public:
  BeegoDriver() : Node("beego_driver")
  {
    read_params();
    reset_params();

    odom_pub_ = this->create_publisher<nav_msgs::msg::Odometry>("odom", 1);
    js_pub_ = this->create_publisher<sensor_msgs::msg::JointState>("joint_states", 1);
    tf_broadcaster_ = std::make_unique<tf2_ros::TransformBroadcaster>(*this);

    cmd_vel_sub_ = this->create_subscription<geometry_msgs::msg::Twist>(
        "cmd_vel", 10, std::bind(&BeegoDriver::cmd_vel_cb, this, std::placeholders::_1));

    int loop_ms = 1000 / loop_hz_;
    dt_ = 1.0 / static_cast<double>(loop_hz_);
    loop_timer_ = this->create_wall_timer(
        std::chrono::milliseconds(loop_ms), std::bind(&BeegoDriver::loop, this));

    bringup_ypspur();

    RCLCPP_INFO(this->get_logger(), "beego_driver initialized (Hz=%d)", loop_hz_);
  }

  ~BeegoDriver() override
  {
    if (ypspur_connected_) {
      Spur_stop();
      Spur_free();
    }
  }

private:
  std::string odom_frame_id_;
  std::string base_frame_id_;
  std::string left_wheel_joint_;
  std::string right_wheel_joint_;
  int loop_hz_ = 40;
  double liner_vel_lim_ = 0.5;
  double liner_accel_lim_ = 1.0;
  double angular_vel_lim_ = 3.14;
  double angular_accel_lim_ = 6.28;
  bool odom_from_ypspur_ = true;
  bool publish_odom_tf_ = true;
  bool debug_mode_ = false;
  double dt_ = 0.025;
  double tf_time_offset_ = 0.0;
  bool ypspur_connected_ = false;

  rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr cmd_vel_sub_;
  rclcpp::Publisher<nav_msgs::msg::Odometry>::SharedPtr odom_pub_;
  rclcpp::Publisher<sensor_msgs::msg::JointState>::SharedPtr js_pub_;
  rclcpp::TimerBase::SharedPtr loop_timer_;
  std::unique_ptr<tf2_ros::TransformBroadcaster> tf_broadcaster_;

  geometry_msgs::msg::Twist::SharedPtr cmd_vel_ = std::make_shared<geometry_msgs::msg::Twist>();
  sensor_msgs::msg::JointState js_;
  nav_msgs::msg::Odometry odom_;
  geometry_msgs::msg::TransformStamped odom_trans_;
  tf2::Vector3 z_axis_;

  void read_params()
  {
    declare_parameter("odom_frame_id", "odom");
    declare_parameter("base_frame_id", "base_footprint");
    declare_parameter("Hz", 40);
    declare_parameter("left_wheel_joint", "left_wheel_joint");
    declare_parameter("right_wheel_joint", "right_wheel_joint");
    declare_parameter("liner_vel_lim", 0.5);
    declare_parameter("liner_accel_lim", 1.0);
    declare_parameter("angular_vel_lim", 3.14);
    declare_parameter("angular_accel_lim", 6.28);
    declare_parameter("calculate_odom_from_ypspur", true);
    declare_parameter("publish_odom_tf", true);
    declare_parameter("debug_mode", false);

    get_parameter("odom_frame_id", odom_frame_id_);
    get_parameter("base_frame_id", base_frame_id_);
    get_parameter("Hz", loop_hz_);
    get_parameter("left_wheel_joint", left_wheel_joint_);
    get_parameter("right_wheel_joint", right_wheel_joint_);
    get_parameter("liner_vel_lim", liner_vel_lim_);
    get_parameter("liner_accel_lim", liner_accel_lim_);
    get_parameter("angular_vel_lim", angular_vel_lim_);
    get_parameter("angular_accel_lim", angular_accel_lim_);
    get_parameter("calculate_odom_from_ypspur", odom_from_ypspur_);
    get_parameter("publish_odom_tf", publish_odom_tf_);
    get_parameter("debug_mode", debug_mode_);

    RCLCPP_INFO(this->get_logger(), "Parameters loaded");
  }

  void reset_params()
  {
    z_axis_.setX(0);
    z_axis_.setY(0);
    z_axis_.setZ(1);

    cmd_vel_->linear.x = 0.0;
    cmd_vel_->angular.z = 0.0;

    odom_.pose.pose.position.x = 0;
    odom_.pose.pose.position.y = 0;
    odom_.pose.pose.position.z = 0;
    odom_.pose.pose.orientation = tf2::toMsg(tf2::Quaternion(z_axis_, 0));
    odom_.twist.twist.linear.x = 0;
    odom_.twist.twist.linear.y = 0;
    odom_.twist.twist.angular.z = 0;

    js_.name.push_back(left_wheel_joint_);
    js_.name.push_back(right_wheel_joint_);
    js_.position.resize(2);
    js_.velocity.resize(2);
  }

  bool bringup_ypspur()
  {
    if (Spur_init() <= 0) {
      ypspur_connected_ = false;
      RCLCPP_WARN(this->get_logger(), "ypspur connection failed");
      return false;
    }

    RCLCPP_INFO(this->get_logger(), "ypspur connected");
    Spur_stop();
    Spur_set_vel(liner_vel_lim_);
    Spur_set_accel(liner_accel_lim_);
    Spur_set_angvel(angular_vel_lim_);
    Spur_set_angaccel(angular_accel_lim_);
    ypspur_connected_ = true;
    return true;
  }

  void cmd_vel_cb(const geometry_msgs::msg::Twist::SharedPtr msg)
  {
    cmd_vel_ = msg;
    if (ypspur_connected_) {
      Spur_vel(msg->linear.x, msg->angular.z);
    }
  }

  void publish_joint_states()
  {
    rclcpp::Time now = this->now();
    double l_ang_pos{}, r_ang_pos{}, l_wheel_vel{}, r_wheel_vel{};
    YP_get_wheel_ang(&l_ang_pos, &r_ang_pos);
    YP_get_wheel_vel(&l_wheel_vel, &r_wheel_vel);

    js_.header.stamp = now;
    js_.header.frame_id = "base_link";
    js_.position[0] = -l_ang_pos;
    js_.position[1] = -r_ang_pos;
    js_.velocity[0] = l_wheel_vel;
    js_.velocity[1] = r_wheel_vel;
    js_pub_->publish(js_);
  }

  void publish_odometry()
  {
    double x, y, yaw, v, w;
    rclcpp::Time now = this->now();

    if (odom_from_ypspur_) {
      Spur_get_pos_GL(&x, &y, &yaw);
      Spur_get_vel(&v, &w);
    } else {
      v = cmd_vel_->linear.x;
      w = cmd_vel_->angular.z;
      yaw = tf2::getYaw(odom_.pose.pose.orientation) + dt_ * w;
      x = odom_.pose.pose.position.x + dt_ * v * cosf(yaw);
      y = odom_.pose.pose.position.y + dt_ * v * sinf(yaw);
    }

    odom_.header.stamp = now;
    odom_.header.frame_id = odom_frame_id_;
    odom_.child_frame_id = base_frame_id_;
    odom_.pose.pose.position.x = x;
    odom_.pose.pose.position.y = y;
    odom_.pose.pose.position.z = 0;
    odom_.pose.pose.orientation = tf2::toMsg(tf2::Quaternion(z_axis_, yaw));
    odom_.twist.twist.linear.x = v;
    odom_.twist.twist.linear.y = 0;
    odom_.twist.twist.angular.z = w;
    odom_pub_->publish(odom_);

    if (publish_odom_tf_) {
      odom_trans_.header.stamp = now + rclcpp::Duration::from_seconds(tf_time_offset_);
      odom_trans_.header.frame_id = odom_frame_id_;
      odom_trans_.child_frame_id = base_frame_id_;
      odom_trans_.transform.translation.x = x;
      odom_trans_.transform.translation.y = y;
      odom_trans_.transform.translation.z = 0;
      odom_trans_.transform.rotation = odom_.pose.pose.orientation;
      tf_broadcaster_->sendTransform(odom_trans_);
    }
  }

  void loop()
  {
    if (!ypspur_connected_) {
      RCLCPP_WARN_THROTTLE(this->get_logger(), *this->get_clock(), 5000,
                           "ypspur disconnected, attempting reconnect...");
      bringup_ypspur();
      return;
    }

    int spur_error = YP_get_error_state();
    if (spur_error) {
      RCLCPP_WARN(this->get_logger(), "ypspur error detected: %d", spur_error);
      ypspur_connected_ = false;
      bringup_ypspur();
      return;
    }

    publish_odometry();
    publish_joint_states();
  }
};

int main(int argc, char *argv[])
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<BeegoDriver>();
  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}
