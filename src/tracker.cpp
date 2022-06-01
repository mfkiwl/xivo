// The feature tracking module;
// Multi-scale Lucas-Kanade tracker from OpenCV.
// Author: Xiaohan Fei (feixh@cs.ucla.edu)
#include <fstream>
#include <algorithm>

#include "glog/logging.h"
#include "opencv2/video/video.hpp"
#include "opencv2/xfeatures2d.hpp"

#include "feature.h"
#include "tracker.h"
#include "visualize.h"

namespace xivo {


cv::Ptr<cv::FeatureDetector> GetOpenCVDetectorDescriptor(
  std::string feature_type, Json::Value feature_cfg)
{
  if (feature_type == "FAST") {
    return cv::FastFeatureDetector::create(
      feature_cfg.get("threshold", 5).asInt(),
      feature_cfg.get("nonmaxSuppression", true).asBool());
  } else if (feature_type == "BRISK") {
    return cv::BRISK::create(
      feature_cfg.get("thresh", 5).asInt(),
      feature_cfg.get("octaves", 3).asInt(),
      feature_cfg.get("patternScale", 1.0).asFloat());
  } else if (feature_type == "ORB") {
    return cv::ORB::create(
      feature_cfg.get("nfeatures", 500).asInt(),
      feature_cfg.get("scaleFactor", 1.2).asFloat(),
      feature_cfg.get("nlevels", 4).asInt(),
      feature_cfg.get("edgeThreshold", 31).asInt(),
      feature_cfg.get("firstLevel", 0).asInt(),
      feature_cfg.get("WTA_K", 2).asInt(),
      feature_cfg.get("patchSize", 31).asInt(),
      feature_cfg.get("fastThreshold", 20).asInt());
  } else if (feature_type == "AGAST") {
    return cv::AgastFeatureDetector::create(
      feature_cfg.get("threshold", 10).asInt(),
      feature_cfg.get("nonmaxSuppression", true).asBool());
  } else if (feature_type == "GFTT") {
    return cv::GFTTDetector::create(
      feature_cfg.get("maxCorners", 1000).asInt(),
      feature_cfg.get("qualityLevel", true).asDouble(),
      feature_cfg.get("minDistance", 1.0).asDouble(),
      feature_cfg.get("blockSize", 3).asInt(),
      feature_cfg.get("useHarrisDetector", false).asBool(),
      feature_cfg.get("k", 0.04).asDouble());
  } else if (feature_type == "SIFT") {
    return cv::SIFT::create(
      feature_cfg.get("nfeatures", 0).asInt(),
      feature_cfg.get("nOctaveLayers", 3).asInt(),
      feature_cfg.get("contrastThreshold", 0.04).asDouble(),
      feature_cfg.get("edgeThreshold", 10.0).asDouble(),
      feature_cfg.get("sigma", 1.6).asDouble());
  } else if (feature_type == "SURF") {
    return cv::xfeatures2d::SURF::create(
      feature_cfg.get("hessianThreshold", 100).asDouble(),
      feature_cfg.get("nOctaves", 4).asInt(),
      feature_cfg.get("nOctaveLayers", 3).asInt(),
      feature_cfg.get("extended", false).asBool(),
      feature_cfg.get("upright", false).asBool());
  } else if (feature_type == "BRIEF") {
    return cv::xfeatures2d::BriefDescriptorExtractor::create(
      feature_cfg.get("bytes", 64).asInt(),
      feature_cfg.get("use_orientation", false).asBool());
  } else if (feature_type == "FREAK") {
    return cv::xfeatures2d::FREAK::create(
      feature_cfg.get("orientationNormalized", true).asBool(),
      feature_cfg.get("scaleNormalized", true).asBool(),
      feature_cfg.get("patternScale", 22.0).asDouble(),
      feature_cfg.get("nOctaves", 4).asInt());
  } else {
    throw std::invalid_argument("unrecognized detector or descriptor type");
  }
}


std::unique_ptr<Tracker> Tracker::instance_ = nullptr;

TrackerPtr Tracker::Create(const Json::Value &cfg) {
  if (instance_ == nullptr) {
    instance_ = std::unique_ptr<Tracker>(new Tracker(cfg));
  } else {
    LOG(WARNING) << "tracker already created";
  }
  return instance_.get();
}

Tracker::Tracker(const Json::Value &cfg) : cfg_{cfg} {
  initialized_ = false;
  mask_size_ = cfg_.get("mask_size", 15).asInt();
  margin_ = cfg_.get("margin", 16).asInt();
  num_features_min_ = cfg_.get("num_features_min", 120).asInt();
  num_features_max_ = cfg_.get("num_features_max", 150).asInt();
  max_pixel_displacement_ = cfg_.get("max_pixel_displacement", 64).asInt();
  differential_ = cfg_.get("differential", true).asBool();

  auto klt_cfg = cfg_["KLT"];
  win_size_ = klt_cfg.get("win_size", 15).asInt();
  max_level_ = klt_cfg.get("max_level", 4).asInt();
  max_iter_ = klt_cfg.get("max_iter", 15).asInt();
  eps_ = klt_cfg.get("eps", 0.01).asDouble();

  std::string detector_type = cfg_.get("detector", "FAST").asString();
  LOG(INFO) << "detector type=" << detector_type;
  if ((detector_type == "FAST") ||
      (detector_type == "BRISK") ||
      (detector_type == "ORB") ||
      (detector_type == "AGAST") ||
      (detector_type == "GFFT") ||
      (detector_type == "SIFT") ||
      (detector_type == "SURF")) {
    detector_ = GetOpenCVDetectorDescriptor(detector_type,
                                            cfg_[detector_type]);
    LOG(INFO) << "detector created";
  } else {
    LOG(FATAL) << "Invalid Feature Detector: " << detector_type;
  }

  descriptor_distance_thresh_ =
      cfg_.get("descriptor_distance_thresh", -1).asInt();
  extract_descriptor_ = cfg_.get("extract_descriptor", false).asBool() ||
                        descriptor_distance_thresh_ > -1;
  LOG(INFO) << "descriptor extraction " << extract_descriptor_ ? "ENABLED"
                                                               : "DISABLED";

  if (extract_descriptor_) {
    std::string descriptor_type = cfg_.get("descriptor", "BRIEF").asString();
    LOG(INFO) << "descriptor type=" << descriptor_type;
    if ((descriptor_type == "BRIEF") ||
        (descriptor_type == "BRISK") ||
        (descriptor_type == "ORB") ||
        (descriptor_type == "FREAK") ||
        (descriptor_type == "SIFT") ||
        (descriptor_type == "SURF")) {
      extractor_ = GetOpenCVDetectorDescriptor(descriptor_type,
                                               cfg_[descriptor_type]);
    } else {
      LOG(FATAL) << "Invalid feature descriptor: " << descriptor_type;
    }
  }

  // Rescuing dropped tracks
  match_dropped_tracks_ = cfg_.get("match_dropped_tracks", false).asBool();
  if (match_dropped_tracks_ && !extract_descriptor_) {
    throw std::invalid_argument("must extract descriptors in order to match dropped tracks");
  }
  if (match_dropped_tracks_) {
    // The number of dropped tracks to match should not be that large, so
    // using Brute-Force matcher instead of FLANN-based matcher.
    matcher_ = cv::BFMatcher::create(extractor_->defaultNorm(), true);
  }
}


void Tracker::Detect(const cv::Mat &img, int num_to_add) {
  std::vector<cv::KeyPoint> kps;
  detector_->detect(img, kps, mask_);
  // sort
  std::sort(kps.begin(), kps.end(),
            [](const cv::KeyPoint &kp1, const cv::KeyPoint &kp2) {
              return kp1.response > kp2.response;
            });

  cv::Mat descriptors;
  if (extract_descriptor_) {
    descriptors.reserveBuffer(kps.size() * extractor_->descriptorSize());
    extractor_->compute(img, kps, descriptors);
  }

  // now every keypoint is equipped with a descriptor


  // match keypoints to old features - indices of these vectors correspond to
  // new features
  std::vector<bool> matched;
  std::vector<int> matchIdx;
  for (int i=0;  i<kps.size(); i++) {
    matched.push_back(false);
    matchIdx.push_back(-1);
  }

  if (match_dropped_tracks_ && (newly_dropped_tracks_.size() > 0)) {

    // Get matrix of old descriptors
    cv::Mat newly_dropped_descriptors(newly_dropped_tracks_.size(),
                                      extractor_->descriptorSize(),
                                      extractor_->descriptorType());
    int i = 0;
    for (auto f: newly_dropped_tracks_) {
      newly_dropped_descriptors.row(i) = f->descriptor();
      i++;
    }

    // radius match - this really just checks for inaccurate optical flows
    // query = newly-dropped descriptors
    // train = just-found descriptors
    std::vector<std::vector<cv::DMatch>> matches;
    matcher_->radiusMatch(newly_dropped_descriptors, descriptors, matches,
                          max_pixel_displacement_);
    for (int i=0; i<matches.size(); i++) {
      if (matches[i].size() > 0) {
        cv::DMatch D = matches[i][0];

        // Check descriptor accuracy
        number_t descriptor_distance =
          cv::norm(newly_dropped_descriptors.row(D.queryIdx),
                   descriptors.row(D.trainIdx),
                   extractor_->defaultNorm());
        if (descriptor_distance < descriptor_distance_thresh_) {
          matched[D.trainIdx] = true;
          matchIdx[D.trainIdx] = D.queryIdx;
        }
      }
    }
  }

  // collect keypoints
  for (int i = 0; i < kps.size(); ++i) {
    const cv::KeyPoint &kp = kps[i];
    if (MaskValid(mask_, kp.pt.x, kp.pt.y)) {

      if (match_dropped_tracks_ && matched[i]) {
        int idx = matchIdx[i];
        FeaturePtr f1 = newly_dropped_tracks_[idx];
        if (differential_) {
          f1->SetDescriptor(descriptors.row(i));
        }
        f1->UpdateTrack(kp.pt.x, kp.pt.y);
        f1->SetTrackStatus(TrackStatus::TRACKED);
        LOG(INFO) << "Rescued dropped feature #" << f1->id();
        MaskOut(mask_, kp.pt.x, kp.pt.y, mask_size_);
        --num_to_add;
        continue;
      }

      // Didn't match to a previously-dropped track, so create a new feature
      FeaturePtr f = Feature::Create(kp.pt.x, kp.pt.y);
      features_.push_back(f);

      if (extract_descriptor_) {
        f->SetDescriptor(descriptors.row(i));
      }
      f->SetKeypoint(kp);

      // mask out
      MaskOut(mask_, kp.pt.x, kp.pt.y, mask_size_);
      --num_to_add;
    }
    if (num_to_add <= 0 || kp.response < 5)
      break;
  }
}

void Tracker::Update(const cv::Mat &image) {
  img_ = image.clone();
  if (cfg_.get("normalize", false).asBool()) {
    cv::normalize(image, img_, 0, 255, cv::NORM_MINMAX);
  }

  if (!initialized_) {
    rows_ = img_.rows;
    cols_ = img_.cols;
    mask_ = cv::Mat(rows_, cols_, CV_8UC1);
    mask_.setTo(0);

    // build image pyramid
    cv::buildOpticalFlowPyramid(img_, pyramid_, cv::Size(win_size_, win_size_),
                                max_level_);
    // setup the mask
    ResetMask(mask_(
        cv::Rect(margin_, margin_, cols_ - 2 * margin_, rows_ - 2 * margin_)));
    // detect an initial set of features
    Detect(img_, num_features_max_);
    initialized_ = true;
    return;
  }
  // reset mask
  ResetMask(mask_(
      cv::Rect(margin_, margin_, cols_ - 2 * margin_, rows_ - 2 * margin_)));
  // std::cout << "reset mask\n";

  // build new pyramid
  std::vector<cv::Mat> pyramid;
  cv::buildOpticalFlowPyramid(img_, pyramid, cv::Size(win_size_, win_size_),
                              max_level_);

  // prepare for optical flow
  cv::TermCriteria criteria(cv::TermCriteria::MAX_ITER | cv::TermCriteria::EPS,
                            max_iter_, eps_);

  std::vector<cv::Point2f> pts0, pts1;
  std::vector<uint8_t> status;
  std::vector<float> err;

  pts0.reserve(features_.size());
  pts1.reserve(pts0.size());

  for (auto f : features_) {
    const Vec2 &pt(f->xp());
    pts0.emplace_back(pt[0], pt[1]);

    // fill in predicted locations
    auto pred = f->pred();
    if (pred(0) != -1 && pred(1) != -1) {
      pts1.emplace_back(pred(0), pred(1));
      f->ResetPred(); // reset
    } else {
      pts1.emplace_back(pt[0], pt[1]);
    }
  }

  if (pts0.size() == 0) {
    initialized_ = false;
    return;
  }

  cv::calcOpticalFlowPyrLK(pyramid_, pyramid, pts0, pts1, status, err,
                           cv::Size(win_size_, win_size_), max_level_, criteria,
                           cv::OPTFLOW_USE_INITIAL_FLOW);

  std::vector<cv::KeyPoint> kps;
  cv::Mat descriptors;
  if (extract_descriptor_) {
    std::vector<FeaturePtr> vf{features_.begin(), features_.end()};
    kps.reserve(vf.size());
    descriptors.reserveBuffer(vf.size() * extractor_->descriptorSize());
    for (int i = 0; i < vf.size(); ++i) {
      auto f = vf[i];
      cv::KeyPoint kp =
          f->keypoint(); // preserve all the properties of the initial keypoint
      kp.pt.x = pts1[i].x; // with updated pixel location
      kp.pt.y = pts1[i].y;
      kp.class_id = i;
      kps.push_back(kp);
    }
    extractor_->compute(img_, kps, descriptors);

    for (int i = 0; i < kps.size(); ++i) {
      auto f = vf[kps[i].class_id];
      if (descriptor_distance_thresh_ != -1) {
        int dist =
            cv::norm(f->descriptor(), descriptors.row(i), extractor_->defaultNorm());
        if (dist > descriptor_distance_thresh_) {
          status[i] = 0; // enforce to be dropped
        } else {
          if (differential_) {
            f->SetDescriptor(descriptors.row(i));
          }
        }
      } else {
        if (differential_) {
          f->SetDescriptor(descriptors.row(i));
        }
      }
    }
  }

  // iterate through features and mark bad ones
  int num_valid_features = 0;
  int i = 0;

  // Clear list of newly dropped tracks from last time
  newly_dropped_tracks_.clear();
  newly_dropped_tracks_hlpr_.clear();

  for (auto it = features_.begin(); it != features_.end(); ++it, ++i) {
    FeaturePtr f(*it);

    Vec2 last_pos(f->xp());
    if (status[i]) {
      if (MaskValid(mask_, pts1[i].x, pts1[i].y) &&
          (last_pos - Vec2{pts1[i].x, pts1[i].y}).norm() <
              max_pixel_displacement_) {
        // update track status
        f->SetTrackStatus(TrackStatus::TRACKED);
        f->UpdateTrack(pts1[i].x, pts1[i].y);
        MaskOut(mask_, pts1[i].x, pts1[i].y, mask_size_);
        ++num_valid_features;
      } else {
        // failed to extract descriptors or invalid mask
        newly_dropped_tracks_.push_back(f);
        newly_dropped_tracks_hlpr_.insert(f);
      }
    } else {
      // failed to track, reject
      newly_dropped_tracks_.push_back(f);
      newly_dropped_tracks_hlpr_.insert(f);
    }
  }

  // detect a new set of features
  // this can rescue dropped featuers by matching them to newly detected ones
  if (num_valid_features < num_features_min_) {
    Detect(img_, num_features_max_ - num_valid_features);
  }

  // Mark all features that are still in newly_dropped_tracks_ at this point
  // as dropped
  for (auto f: newly_dropped_tracks_) {
    f->SetTrackStatus(TrackStatus::DROPPED);
  }

  // swap buffers ...
  std::swap(pyramid, pyramid_);

}

////////////////////////////////////////
// helpers
////////////////////////////////////////
void ResetMask(cv::Mat mask) { mask.setTo(255); }

void MaskOut(cv::Mat mask, number_t x, number_t y, int mask_size) {
  static int half_size = (mask_size >> 1);
  cv::rectangle(mask, cv::Point2d(x - half_size, y - half_size),
                cv::Point2d(x + half_size, y + half_size), cv::Scalar(0), -1);
}

bool MaskValid(const cv::Mat &mask, number_t x, number_t y) {
  int col = static_cast<int>(x);
  int row = static_cast<int>(y);
  if (col < 0 || col >= mask.cols || row < 0 || row >= mask.rows)
    return false;
  return static_cast<bool>(mask.at<uint8_t>(row, col));
}

} // namespace xivo
