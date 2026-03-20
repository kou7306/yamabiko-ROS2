import os
from ament_index_python.packages import get_package_share_directory
import launch
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    pkg_dir = get_package_share_directory('yamabiko_driver')

    # ロボット名の引数（beego, speego, m1 など）
    robot_name_arg = DeclareLaunchArgument(
        'robot_name',
        default_value='beego',
        description='Robot name (beego, speego, m1)'
    )
    robot_name = LaunchConfiguration('robot_name')

    # YP-Spur パラメータファイル
    # LaunchConfigurationは文字列結合できないので、デフォルト値で直接指定
    # 変更する場合は ypspur_param 引数で直接パスを指定してください
    ypspur_param_arg = DeclareLaunchArgument(
        'ypspur_param',
        default_value=os.path.join(pkg_dir, 'config', 'beego.param'),
        description='Path to YP-Spur parameter file'
    )
    ypspur_param = LaunchConfiguration('ypspur_param')

    # ドライバノードのパラメータ
    driver_param = os.path.join(pkg_dir, 'config', 'driver_node.param.yaml')

    # ypspur-coordinator ブリッジスクリプト
    coordinator_script = os.path.join(pkg_dir, 'scripts', 'ypspur_coordinator_bridge')

    return LaunchDescription([
        robot_name_arg,
        ypspur_param_arg,

        # ypspur-coordinator を起動
        launch.actions.LogInfo(msg="Starting ypspur-coordinator..."),
        launch.actions.ExecuteProcess(
            cmd=[coordinator_script, ypspur_param],
            shell=True,
            output='screen'
        ),

        # ドライバノードを起動
        launch.actions.LogInfo(msg="Starting yamabiko_driver node..."),
        Node(
            package='yamabiko_driver',
            executable='yamabiko_driver',
            output='screen',
            parameters=[driver_param]
        ),
    ])
