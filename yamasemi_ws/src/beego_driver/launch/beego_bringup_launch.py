import os

from ament_index_python.packages import get_package_share_directory
import launch
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    beego_driver_dir = get_package_share_directory('beego_driver')

    # Robot param file for ypspur-coordinator
    ypspur_param = os.path.join(beego_driver_dir, 'config', 'beego.param')

    # Driver node param file
    driver_param = os.path.join(beego_driver_dir, 'config', 'driver_node.param.yaml')

    # ypspur-coordinator bridge script
    ypspur_coordinator_path = os.path.join(beego_driver_dir, 'scripts', 'ypspur_coordinator_bridge')

    # Launch arguments
    device_arg = DeclareLaunchArgument(
        'device', default_value='/dev/ttyACM0',
        description='Serial device for the motor driver')

    return LaunchDescription([
        device_arg,

        launch.actions.LogInfo(msg="Launching ypspur-coordinator for beego..."),

        launch.actions.ExecuteProcess(
            cmd=[ypspur_coordinator_path, ypspur_param, LaunchConfiguration('device')],
            shell=True,
            output='screen'
        ),

        launch.actions.LogInfo(msg="Launching beego_driver node..."),

        Node(
            package='beego_driver',
            executable='beego_driver',
            output='screen',
            parameters=[driver_param]
        )
    ])
