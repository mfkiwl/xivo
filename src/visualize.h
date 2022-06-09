// Drawing functions to overlay feature tracks & system
// info on input images.
// Author: Xiaohan Fei (feixh@cs.ucla.edu)
#pragma once
#include <filesystem>
#include <list>

#include "opencv2/core/core.hpp"

#include "core.h"
#include "imu.h"

namespace xivo {

class Canvas;
using CanvasPtr = Canvas *;

class Canvas {
public:
  static CanvasPtr instance();

  static void Delete();
  void Update(const cv::Mat &img);
  void UpdatePointCloud(const MatX2 &px);
  void Draw(const FeaturePtr f);
  void OverlayStateInfo(const State &X, const IMUState &IMU, const Vec9 &Cam,
                        int vspace = 12, int hspace = 12,
                        int thickness = 1, double font_scale = 0.9);
  const cv::Mat &display() const { return disp_; }
  const void SaveFrame();

private:
  Canvas(const Canvas &) = delete;
  Canvas &operator=(const Canvas &) = delete;
  Canvas();
  static std::unique_ptr<Canvas> instance_;

  cv::Mat disp_;

  // Parameters for saving each file
  bool save_frames_;
  std::filesystem::path save_folder_;
  int frame_number_;
};
}
