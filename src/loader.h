// Dataloader for ASL-compatible dataset.
// Author: Xiaohan Fei (feixh@cs.ucla.edu)
#pragma once
#include <memory>
#include <string>
#include <vector>

#include "core.h"
#include "message_types.h"

namespace xivo {

class DataLoader {
public:
  EIGEN_MAKE_ALIGNED_OPERATOR_NEW

  DataLoader(const std::string &image_dir, const std::string &imu_dir);
  DataLoader(const std::string &image_dir);
  std::vector<msg::Pose> LoadGroundTruthState(const std::string &state_dir);

  msg::Message *Get(int i) const { return entries_[i].get(); };
  int size() const { return entries_.size(); }

private:
  std::vector<std::unique_ptr<msg::Message>> entries_;
  std::vector<msg::Pose> poses_;
};

using TUMVILoader = DataLoader;
using EuRoCLoader = DataLoader;

// Get image, imu and groundtruth directories for TUMVI and EuRoC dataset
std::tuple<std::string, std::string, std::string>
GetDirs(const std::string dataset, const std::string root,
        const std::string seq, int cam_id);

} // namespace xivo
