import os
from glob import glob
from setuptools import find_packages, setup

package_name = 'soarm101_teleoperate'

setup(
    name=package_name,
    version='0.0.0',
    packages=find_packages(exclude=['test']),
    data_files=[
        ('share/ament_index/resource_index/packages',
            ['resource/' + package_name]),
        ('share/' + package_name, ['package.xml']),
                (
            os.path.join("share", package_name, "launch"),
            glob("launch/*.launch.py"),
        ),
        (
            os.path.join("share", package_name, "rviz"),
            glob("rviz/*.rviz"),
        ),
        (
            os.path.join("share", package_name, "config"),
            glob("config/*.yaml"),
        ),
    ],
    install_requires=['setuptools'],
    zip_safe=True,
    maintainer='banana-killer',
    maintainer_email='sashagrachev2005@gmail.com',
    description='The pkg allow teleoperate follower-arm from leader-arm',
    license='MIT',
    extras_require={
        'test': [
            'pytest',
        ],
    },
    entry_points={
        'console_scripts': [
            'teleoperate_node = soarm101_teleoperate.teleoperate_node:main',
            'marker_publisher = soarm101_teleoperate.marker_publisher:main',
            'static_transform_publisher_world_to_arms = soarm101_teleoperate.static_transform_publisher_world_to_arms:main',
            'gripper_relay = soarm101_teleoperate.gripper_relay:main',
            'arm_relay = soarm101_teleoperate.arm_relay:main'
        ],
    },
)
