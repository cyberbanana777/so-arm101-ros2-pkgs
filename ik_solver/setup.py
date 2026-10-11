import os
from glob import glob

from setuptools import find_packages, setup

package_name = 'ik_solver'

setup(
    name=package_name,
    version='0.1.0',
    packages=find_packages(exclude=['test']),
    data_files=[
        ('share/ament_index/resource_index/packages', ['resource/' + package_name]),
        ('share/' + package_name, ['package.xml']),
        (os.path.join('share', package_name, 'launch'), glob('launch/*.launch.py')),
        (os.path.join('share', package_name, 'config'), glob('config/*.yaml')),
    ],
    install_requires=['setuptools', 'numpy'],
    zip_safe=True,
    maintainer='Your Name',
    maintainer_email='you@example.com',
    description='Closed-form inverse kinematics for the SO-101 arm: xyz target to JointTrajectory',
    license='MIT',
    tests_require=['pytest'],
    entry_points={
        'console_scripts': [
            'xyz_to_joint_trajectory = ik_solver.xyz_to_joint_trajectory_node:main',
            'ik_check = ik_solver.ik_check:main',
        ],
    },
)
