#ifndef _BCD_UTILS_H_
#define _BCD_UTILS_H_
#include <mutex>
#include <condition_variable>
#include <vector>
#include <algorithm>
#include "dmlc/data.h"
#include "difacto/sarray.h"
namespace difacto {
namespace bcd {

class FeatureBlock {
 public:
  /**
   * \brief partition the whole feature space into blocks
   *
   * @param feagrp_nbits number of bit for encoding the feature group
   * @param feagrps a list of (feature_group, num_partitions_this_group)
   * @param feablks a list of feature blocks with the start and end ID
   */
  static void Partition(int feagrp_nbits,
                        const std::vector<std::pair<int,int>>& feagrps,
                        std::vector<Range>* feablks) {
    CHECK_EQ(feagrp_nbits % 4, 0) << "should be 0, 4, 8, ...";
    feablks->clear();
    feaid_t key_max = std::numeric_limits<feaid_t>::max();
    for (auto f : feagrps) {
      CHECK_GT(1<<feagrp_nbits, f.first);
      Range g;
      feaid_t begin = static_cast<feaid_t>(f.first) << (FEAID_NBITS - feagrp_nbits);
      g.begin = ReverseBytes(begin);
      g.end = ReverseBytes((key_max >> feagrp_nbits) | begin);
      for (int i = 0; i < f.second; ++i) {
        feablks->push_back(g.Segment(i, f.second));
        CHECK(feablks->back().Valid());
      }
    }
    std::sort(feablks->begin(), feablks->end(),
              [](const Range& a, const Range& b) { return a.begin < b.begin;});
    for (size_t i = 1; i < feablks->size(); ++i) {
      auto& before = feablks->at(i-1), after = feablks->at(i);
      if (before.end < after.begin) ++before.end;
      CHECK_LE(before.end, after.begin);
    }
  }
  /**
   * \brief find the positionn of each feature block in the list of feature IDs
   *
   * @param feaids
   * @param feablks
   * @param positions
   */
  static void FindPosition(const SArray<feaid_t>& feaids,
                           const std::vector<Range>& feablks,
                           std::vector<Range>* positions) {
    size_t n = feablks.size();
    for (size_t i = 0; i < n; ++i) CHECK(feablks[i].Valid());
    for (size_t i = 1; i < n; ++i) CHECK_LE(feablks[i-1].end, feablks[i].begin);

    positions->resize(n);
    feaid_t const* begin = feaids.begin();
    feaid_t const* end = feaids.end();
    feaid_t const* cur = begin;

    for (size_t i = 0; i < n; ++i) {
      auto lb = std::lower_bound(cur, end, feablks[i].begin);
      auto ub = std::lower_bound(lb, end, feablks[i].end);
      cur = ub;
      positions->at(i) = Range(lb - begin, ub - begin);
    }
  }
};


/**
 * \brief count statistics for feature groups
 */
class FeaGroupStats {
 public:
  FeaGroupStats(int nbit) {
    CHECK_EQ(nbit % 4, 0) << "should be 0, 4, 8, ...";
    CHECK_LE(nbit, 16);
    nbit_ = nbit;
    value_.resize((1<<nbit)+2);
  }

  void Add(const dmlc::RowBlock<feaid_t>& rowblk) {
    real_t nrows = 0;
    for (size_t i = 0; i < rowblk.size; i+=skip_) {
      for (size_t j = rowblk.offset[i]; j < rowblk.offset[i+1]; ++j) {
        feaid_t f = rowblk.index[j];
        ++value_[f>>(FEAID_NBITS - nbit_)];
      }
      ++nrows;
    }
    value_[1<<nbit_] += nrows;
    value_[(1<<nbit_)+1] += rowblk.size;
  }

  void Get(std::vector<real_t>* value) {
    *value = value_;
  }

 private:
  int nbit_;
  int skip_ = 10; // only count 10% data
  std::vector<real_t> value_;
};

/**
 * \brief monitor if or not a block is finished, thread safe
 */
class BlockTracker {
 public:
  BlockTracker(int num_blks) : done_(num_blks) { }
  /** \brief mark id as finished */
  void Finish(int id) {
    mu_.lock();
    done_[id] = 1;
    mu_.unlock();
    cond_.notify_all();
  }
  /** \brief block untill id is finished */
  void Wait(int id) {
    std::unique_lock<std::mutex> lk(mu_);
    cond_.wait(lk, [this, id] {return done_[id] == 1; });
  }
 private:
  std::mutex mu_;
  std::condition_variable cond_;
  std::vector<int> done_;
};

class Delta {
 public:
  /**
   * \brief init delta
   * @param delta
   */
  static void Init(size_t len, SArray<real_t>* delta, real_t init_val = 1.0) {
    delta->resize(len);
    for (size_t i = 0; i < len; ++i) (*delta)[i] = init_val;
  }

  /**
   * \brief update delta given the change of w
   */
  static void Update(real_t delta_w, real_t* delta, real_t max_val = 5.0) {
    *delta = std::min(max_val, static_cast<real_t>(std::abs(delta_w) * 2.0 + .1));
  }

};

}  // namespace bcd
}  // namespace difacto
#endif  // _BCD_UTILS_H_
