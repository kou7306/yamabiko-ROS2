import launch
from launch import LaunchDescription
from launch_ros.actions import Node


def generate_launch_description():
    return LaunchDescription([
        Node(
            package='urg_node',
            executable='urg_node_driver',
            name='urg_node',
            output='screen',
            parameters=[{
                'serial_port': '/dev/serial/by-id/usb-Hokuyo_Data_Flex_for_USB_URG-Series_URG0-FA-AD0001-0000-if00',
                'frame_id': 'laser',
                'angle_min': -2.356194,   # -135 deg
                'angle_max': 2.356194,    #  135 deg
            }],
        ),
        Node(
            package='tf2_ros',
            executable='static_transform_publisher',
            name='base_to_laser',
            arguments=['0', '0', '0.1', '0', '0', '0', 'base_footprint', 'laser'],
        ),
    ])
