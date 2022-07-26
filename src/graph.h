// The feature-group visibility graph.
// Author: Xiaohan Fei (feixh@cs.ucla.edu)
#pragma once
#include <functional>
#include <list>
#include <unordered_map>

#include "core.h"
#include "feature.h"
#include "group.h"
#include "graphbase.h"

namespace xivo {

/** Graph of all features and groups that are currently being tracked in the
 * EKF.
 */
class Graph : public GraphBase {
public:
  static Graph* Create();
  static Graph* instance();
  ~Graph() {
    delete random_generator;
    delete random_device;
  }

  void RemoveFeature(const FeaturePtr f);
  void RemoveFeatures(const std::vector<FeaturePtr> &);

  void RemoveGroup(const GroupPtr g);
  void RemoveGroups(const std::vector<GroupPtr> &);

  /** Add a new feature to the graph. Called when `f` is first detected by
   * the `Tracker`. */
  void AddFeature(FeaturePtr f);
  /** Add a new group to the graph. Called once every time a new RGB image
   * was acquired. */
  void AddGroup(GroupPtr g);

  /** Marks that feature `f` was visible at group `g`. Function is called
   * when a feature is first detected by the tracker, and whenever it continues
   * to be tracked by the `Tracker`.
   */
  void AddGroupToFeature(GroupPtr g, FeaturePtr f);
  /** Marks that feature `f` was visible at group `g`. Function is called
   * when a feature is first detected by the tracker, and whenever it continues
   * to be tracked by the `Tracker`.
   */
  void AddFeatureToGroup(FeaturePtr f, GroupPtr g);

  // transfer ownership of features owned by g, i.e., with g as reference
  // return those for which a new owner cannot be found
  // gsc is the camera to spatial frame transformation
  // This function is called when a group `g` is discarded to make room for
  // more groups in the `MemoryManager`. Features that were first created at
  // group `g` have their coordinates changed to a new group.
  std::vector<FeaturePtr> TransferFeatureOwnership(
    GroupPtr g, const SE3 &gbc, number_t cov_factor);

  // Helper function to `TransferFeatureOwnership`. Finds a new reference frame
  // for a feature when its group is removed from the graph to make room for new
  // groups.
  GroupPtr FindNewOwner(FeaturePtr f);

  /** Shortcut functions */
  std::vector<FeaturePtr> GetInstateFeatures();
  std::vector<GroupPtr> GetInstateGroups();
  std::vector<FeaturePtr> GetGaugeFeatureCandidates(GroupPtr owner);
  std::vector<GroupPtr> GetInstateGroupCandidates(int degrees_fixed);
  std::vector<FeaturePtr> GetFeaturesOwnedBy(GroupPtr g);
  int NumFeaturesOwnedBy(GroupPtr g);
  std::vector<FeaturePtr> GetFeatureCandidatesOwnedBy(GroupPtr g);

  GroupPtr LastAddedGroup() const { return last_added_group_; }

  /** Checks that
   * 1. Every member of `features_` has at least one observation
   * 2. The group in every `Observation` of all features is a member of `groups_`
   * 3. The reference group of every feature is not NULL
   * 4. The reference group of every feature is a member of `groups_`
   * 5. There is a set of features associated with each group
   * 6. Every feature associated with a group is a member of `features_`
   * (Function only used for debugging.)
  */
  void SanityCheck();

  /** Removes (and deletes) all groups that have no adjacent features.
   * (Function not actually used.)
  */
  void CleanIsolatedGroups();
  /** Removes (and deletes all features) that have no observations in
   * `feature_adj_`.
   * (Function not actualy used.)
   */
  void CleanIsolatedFeatures();
  /** Calls `CleanIsolatedGroups()` and `CleanIsolatedFeatures()`.
   * (Function not actually used.)
   */
  void CleanIsolatedNodes();

  std::vector<FeaturePtr> FindNewGaugeFeatures(GroupPtr g);

private:
  Graph() {
    random_device = new std::random_device;
    random_generator = new std::mt19937((*random_device)());
  }

  Graph(const Graph&) = delete;
  Graph& operator=(const Graph&) = delete;
  static std::unique_ptr<Graph> instance_;

  GroupPtr last_added_group_;

  /** Maps group id (int) to a unordered set of features whose (x,y) coordinates
   *  of `Feature::x_` is held constant. */
  std::unordered_map<GroupPtr, std::unordered_set<FeaturePtr>> gauge_features_;

  /** For generating random permutations in `FindNewGaugeFeatures` */
  std::random_device *random_device;
  std::mt19937 *random_generator;
};

} // namespace xivo