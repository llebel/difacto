/**
 * Copyright (c) 2015 by Contributors
 * @file   sgd.h
 * @brief  the stochastic gradient descent solver
 */
#ifndef DIFACTO_SGD_SGD_UPDATER_H_
#define DIFACTO_SGD_SGD_UPDATER_H_
#include <string>
#include <vector>
#include <limits>
#include "difacto/updater.h"
#include "dmlc/parameter.h"
#include "dmlc/io.h"
namespace difacto {

struct SGDUpdaterParam : public dmlc::Parameter<SGDUpdaterParam> {
  /** \brief the l1 regularizer for :math:`w`: :math:`\lambda_1 |w|_1` */
  float l1;
  /** \brief the l2 regularizer for :math:`w`: :math:`\lambda_2 \|w\|_2^2` */
  float l2;
  /** \brief the l2 regularizer for :math:`V`: :math:`\lambda_2 \|V_i\|_2^2` */
  float V_l2;

  /** \brief the learning rate :math:`\eta` (or :math:`\alpha`) for :math:`w` */
  float lr;
  /** \brief learning rate :math:`\beta` */
  float lr_beta;
  /** \brief learning rate :math:`\eta` for :math:`V`. */
  float V_lr;
  /** \brief leanring rate :math:`\beta` for :math:`V`. */
  float V_lr_beta;
  /**
   * \brief the scale to initialize V.
   * namely V is initialized by uniform random number in
   *   [-V_init_scale, +V_init_scale]
   */
  float V_init_scale;
  /** \brief the embedding dimension */
  int V_dim;
  /** \brief the minimal feature count for allocating V */
  int V_threshold;
  /** \brief random seed */
  unsigned int seed;
  DMLC_DECLARE_PARAMETER(SGDUpdaterParam) {
    DMLC_DECLARE_FIELD(l1).set_range(0, 1e10).set_default(1);
    DMLC_DECLARE_FIELD(l2).set_range(0, 1e10).set_default(0);
    DMLC_DECLARE_FIELD(V_l2).set_range(0, 1e10).set_default(.01);
    DMLC_DECLARE_FIELD(lr).set_range(0, 10).set_default(.01);
    DMLC_DECLARE_FIELD(lr_beta).set_range(0, 1e10).set_default(1);
    DMLC_DECLARE_FIELD(V_lr).set_range(0, 1e10).set_default(.01);
    DMLC_DECLARE_FIELD(V_lr_beta).set_range(0, 10).set_default(1);
    DMLC_DECLARE_FIELD(V_init_scale).set_range(0, 10).set_default(.01);
    DMLC_DECLARE_FIELD(V_threshold).set_default(10);
    DMLC_DECLARE_FIELD(V_dim);
    DMLC_DECLARE_FIELD(seed).set_default(0);
  }
};

/**
 * \brief the weight entry for one feature
 */
struct SGDEntry {
 public:
  SGDEntry() { fea_cnt = 0; w = 0; sqrt_g = 0; z = 0; V = nullptr; }
  ~SGDEntry() { delete [] V; }

  /** \brief the number of appearence of this feature in the data so far */
  real_t fea_cnt;
  /** \brief w and its aux data */
  real_t w, sqrt_g, z;
  /** \brief V and its aux data */
  real_t *V;
};

/**
 * \brief store all weights
 */
class SGDModel {
 public:
  SGDModel() { }
  ~SGDModel() { }
  /**
   * \brief init model
   *
   * @param V_dim the dimension of V
   * @param start_id the minimal feature id
   * @param end_id the maximal feature id
   */
  void Init(int V_dim, feaid_t start_id, feaid_t end_id);
  /**
   * \brief get the weight entry for a feature id
   * \param id the feature id
   */
  inline SGDEntry& operator[] (feaid_t id) {
    CHECK_GE(id, start_id_);
    id -= start_id_;
    return dense_ ? model_vec_[id] : model_map_[id];
  }
  /**
   * \brief load model
   * \param fi input stream
   */
  void Load(dmlc::Stream* fi, bool* has_aux);
  /**
   * \brief save model
   * \param fo output stream
   */
  void Save(bool save_aux, dmlc::Stream *fo) const;

 private:
  /** \brief load one entry */
  inline void Load(dmlc::Stream* fi, int len, SGDEntry* entry);

  /** \brief save one entry */
  inline void Save(
      bool save_aux, feaid_t id, const SGDEntry& entry, dmlc::Stream *fo) const;

  int V_dim_;
  bool dense_;
  feaid_t start_id_, end_id_;
  std::vector<SGDEntry> model_vec_;
  std::unordered_map<feaid_t, SGDEntry> model_map_;
};

/**
 * \brief sgd updater
 *
 * - w is updated by FTRL, which is a smooth version of adagrad works well with
 *   the l1 regularizer
 * - V is updated by adagrad
 */
class SGDUpdater : public Updater {
 public:
  SGDUpdater() : new_w_(0), new_V_(0), has_aux_(true) {
    // ppmonitor_ = ProgressMonitor::Create();
  }
  virtual ~SGDUpdater() {
    // delete ppmonitor_;
  }

  KWArgs Init(const KWArgs& kwargs) override;

  void Load(dmlc::Stream* fi, bool* has_aux) override {
    model_.Load(fi, has_aux);
    has_aux_ = *has_aux;
  }

  void Save(bool save_aux, dmlc::Stream *fo) const override {
    model_.Save(save_aux, fo);
  }

  void Get(const SArray<feaid_t>& fea_ids,
           int value_type,
           SArray<real_t>* weights,
           SArray<int>* offsets) override;


  void Update(const SArray<feaid_t>& fea_ids,
              int value_type,
              const SArray<real_t>& values,
              const SArray<int>& offsets) override;

 private:
  /** \brief update w by FTRL */
  void UpdateW(real_t gw, SGDEntry* e);

  /** \brief update V by adagrad */
  void UpdateV(real_t const* gV, SGDEntry* e);

  /** \brief init V */
  void InitV(SGDEntry* e);

  SGDModel model_;
  SGDUpdaterParam param_;

  // ProgressMonitor* ppmonitor_;
  int64_t new_w_;
  int64_t new_V_;
  bool has_aux_;
};


}  // namespace difacto
#endif  // DIFACTO_SGD_SGD_UPDATER_H_
