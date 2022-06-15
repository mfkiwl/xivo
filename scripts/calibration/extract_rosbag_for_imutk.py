""" Extract imu data from rosbags for imu_tk to calibrate.
"""
import os
import argparse
import sys
import numpy as np

import rosbag
from sensor_msgs.msg import Imu

parser = argparse.ArgumentParser(description='Extract imu messages for calibration')
parser.add_argument('--bag_file', help='Input ROS bag.')
parser.add_argument('--imu_topic', default='/camera/imu', help='imu topic')
parser.add_argument('--output_dir', default='.', help='Where to save output data')

args = parser.parse_args()

if __name__ == '__main__':
    bag = rosbag.Bag(args.bag_file, 'r')

    gyro = []
    accel = []
    for topic, msg, t in bag.read_messages(topics=[args.imu_topic]):
        if np.linalg.norm([msg.linear_acceleration.x, msg.linear_acceleration.y, msg.linear_acceleration.z]) > 0:
            accel.append((t.to_sec(), msg.linear_acceleration.x, msg.linear_acceleration.y, msg.linear_acceleration.z))
        if np.linalg.norm([msg.angular_velocity.x, msg.angular_velocity.y, msg.angular_velocity.z]) > 0:
            gyro.append((t.to_sec(), msg.angular_velocity.x, msg.angular_velocity.y, msg.angular_velocity.z))

    gyro_path = os.path.join(args.output_dir, 'gyro_data.txt')
    accel_path = os.path.join(args.output_dir, 'accel_data.txt')
    np.savetxt(gyro_path, np.asarray(gyro))
    np.savetxt(accel_path, np.asarray(accel))
