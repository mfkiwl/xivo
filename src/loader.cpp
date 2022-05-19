// Dataloader for ASL-compatible dataset.
// Author: Xiaohan Fei (feixh@cs.ucla.edu)
#include <fstream>
#include <iostream>
#include <sstream>

#include "glog/logging.h"

#include "message_types.h"
#include "loader.h"

namespace xivo {

DataLoader::DataLoader(const std::string &image_dir,
                         const std::string &imu_dir) {

  // load image data entries
  std::string image_data = image_dir + "/data.csv";
  if (std::ifstream is{image_data}) {
    std::string line;
    std::getline(is, line); // get rid of the header
    while (is >> line) {
      if (line.front() != '#') {
        std::vector<std::string> content = StrSplit(line, ',');
        auto ts{timestamp_t(std::stoll(content[0]))};
        std::string image_path = image_dir + "/data/" + content[1];
        entries_.emplace_back(std::make_unique<msg::Image>(ts, image_path));
      }
    }
  } else {
    LOG(FATAL) << "failed to open image csv @ " << image_data;
  }

  // load imu data
  std::string imu_data = imu_dir + "/data.csv";
  if (std::ifstream is{imu_data}) {
    std::string line;
    std::getline(is, line); // get rid of the header
    while (is >> line) {
      if (line.front() != '#') {
        std::vector<std::string> content = StrSplit(line, ',');
        auto ts{timestamp_t(std::stoll(content[0]))};
        Vec3 gyro;
        Vec3 accel;
        for (int i = 0; i < 3; ++i)
          gyro(i) = std::stod(content[i + 1]);
        for (int i = 0; i < 3; ++i)
          accel(i) = std::stod(content[i + 4]);
        entries_.emplace_back(std::make_unique<msg::IMU>(ts, gyro, accel));
      }
    }
  } else {
    LOG(FATAL) << "failed to open data.csv @ " << imu_data;
  }

  // ascend timestamps
  std::sort(entries_.begin(), entries_.end(),
            [](const auto &e1, const auto &e2) { return e1->ts_ < e2->ts_; });
}

DataLoader::DataLoader(const std::string &image_dir) {

  // load image data entries
  std::string image_data = image_dir + "/data.csv";
  if (std::ifstream is{image_data}) {
    std::string line;
    std::getline(is, line); // get rid of the header
    while (is >> line) {
      if (line.front() != '#') {
        std::vector<std::string> content = StrSplit(line, ',');
        auto ts{timestamp_t(std::stoll(content[0]))};
        std::string image_path = image_dir + "/data/" + content[1];
        entries_.emplace_back(std::make_unique<msg::Image>(ts, image_path));
      }
    }
  } else {
    LOG(FATAL) << "failed to open image csv @ " << image_data;
  }

  // ascend timestamps
  std::sort(entries_.begin(), entries_.end(),
            [](const auto &e1, const auto &e2) { return e1->ts_ < e2->ts_; });
}

std::vector<msg::Pose>
DataLoader::LoadGroundTruthState(const std::string &state_dir) {
  std::string state_data = state_dir + "/data.csv";
  if (std::ifstream is{state_data}) {
    std::string line;
    std::getline(is, line); // get rid of the header
    while (is >> line) {
      if (line.front() != '#') {
        std::vector<std::string> content = StrSplit(line, ',');
        auto ts{timestamp_t(std::stoull(content[0]))};
        // pose frame -> world frame
        Vec3 T;
        Vec4 Q; // qw, qx, qy, qz
        // FIXME: load the full state -- for now only load translation and
        // quaternion
        for (int i = 0; i < 3; ++i)
          T(i) = std::stod(content[i + 1]);
        for (int i = 0; i < 4; ++i)
          Q(i) = std::stod(content[i + 4]);
        Eigen::Quaternion<number_t> q(Q(0), Q(1), Q(2), Q(3));
        SE3 gsb{SO3{q.normalized().toRotationMatrix()}, T};

        poses_.emplace_back(ts, gsb);
      }
    }
    LOG(INFO) << StrFormat("%d ground truth poses in total loaded",
                                    poses_.size());
  } else {
    LOG(FATAL) << "failed to load ground-truth state csv @ " << state_data;
  }

  return poses_;
}

std::tuple<std::string, std::string, std::string>
GetDirs(const std::string dataset, const std::string root,
        const std::string seq, int cam_id) {
  std::string dataset_type{dataset};
  std::transform(dataset.begin(), dataset.end(), dataset_type.begin(), tolower);

  if (dataset_type == "tumvi") {
    std::string image_dir =
        StrFormat("%s/dataset-%s_512_16/mav0/cam%d/", root, seq, cam_id);
    std::string imu_dir =
        StrFormat("%s/dataset-%s_512_16/mav0/imu0/", root, seq);
    std::string mocap_dir =
        StrFormat("%s/dataset-%s_512_16/mav0/mocap0/", root, seq);
    return std::make_tuple(image_dir, imu_dir, mocap_dir);
  } else if (dataset_type == "euroc") {
    std::string image_dir =
        StrFormat("%s/%s/mav0/cam%d/", root, seq, cam_id);
    std::string imu_dir = StrFormat("%s/%s/mav0/imu0/", root, seq);
    std::string mocap_dir =
        StrFormat("%s/%s/mav0/state_groundtruth_estimate0/", root, seq);
    return std::make_tuple(image_dir, imu_dir, mocap_dir);
  } else if ((dataset_type == "xivo") ||
             (dataset_type == "void")) {
    std::string image_dir =
        StrFormat("%s/%s/cam0/", root, seq);
    std::string imu_dir = StrFormat("%s/%s/imu0/", root, seq);
    std::string mocap_dir = "";
    return std::make_tuple(image_dir, imu_dir, mocap_dir);
  } else {
    LOG(FATAL) << "Unrecognized dataset type, expecting [euroc|tumvi|xivo|void]";
  }
}

} // namespace xivo
