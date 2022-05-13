#pragma once
#include <memory>
#include <unordered_map>
#include <mutex>

#include "core.h"
#include "jac.h"

namespace xivo {


/** Circular Buffer with Hash Table.
 * Circular Buffer with Hash table 
 */
template<typename T>
class CircBufWithHash {

public:
  CircBufWithHash(int max_items, bool is_feature);
  ~CircBufWithHash();
  T* GetItem();
  void DeactivateItem(T* item);
  void DestroyItem(T *item);

private:
  int max_items_;
  int num_slots_initialized_;
  int num_slots_active_;
  int slot_search_ind_;
  std::vector<bool> slots_initialized_;
  std::vector<bool> slots_active_;
  std::vector<T*> slots_;
  std::unordered_map<T*, int> slots_map_;
  bool is_feature_buf_; // true if storing features, false if storing groups

  void RemoveFromMapper(T* item);
};



/** Singleton memory management for feature and groups.
 *  A fixed chunk of memory is pre-allocated for features and groups,
 *  which prevents memory leaks and frequent malloc calls.
 *  Author: Xiaohan Fei (feixh@cs.ucla.edu) */
class MemoryManager {
public:
  ~MemoryManager();

  static MemoryManagerPtr Create(int max_features, int max_groups);
  static MemoryManagerPtr instance();

  FeaturePtr GetFeature();
  void DeactivateFeature(FeaturePtr);
  void DestroyFeature(FeaturePtr);
  GroupPtr GetGroup();
  void DeactivateGroup(GroupPtr);
  void DestroyGroup(GroupPtr);

private:
  MemoryManager() = delete;
  MemoryManager(const MemoryManager &) = delete;
  MemoryManager &operator=(const MemoryManager &) = delete;

  MemoryManager(int max_features = 512, int max_groups = 128);

  static std::unique_ptr<MemoryManager> instance_;

  CircBufWithHash<Feature> *feature_slots_;
  CircBufWithHash<Group> *group_slots_;
};

} // namespace xivo
