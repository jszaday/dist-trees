#ifndef __NODE_LEVEL_LOC_MGR_HH__
#define __NODE_LEVEL_LOC_MGR_HH__

#include "hello.decl.h"
#include "ck.h"

#define NOT_IMPLEMENTED CkAbort("not yet implemented")

using index_hasher = IndexHasher;
using array_id_hasher = ArrayIDHasher;
using array_listener = CkArrayListener;

namespace binary_tree {
template <typename T>
inline T left_child(const T &i) {
  return (2 * i) + 1;
}

template <typename T>
inline T right_child(const T &i) {
  return (2 * i) + 2;
}

template <typename T>
inline T parent(const T &i) {
  return (i > 0) ? ((i - 1) / 2) : -1;
}

template <typename T>
inline int num_leaves(const T &n) {
  return int(n + 1) / 2;
}
}

namespace util {
inline std::string idx2str(const CkArrayIndex &idx) {
  auto &nDims = idx.dimension;
  if (nDims == 1) {
    return std::to_string(idx.data()[0]);
  } else {
    std::stringstream ss;
    ss << "(";
    for (auto i = 0; i < nDims; i += 1) {
      if (nDims >= 4) {
        ss << idx.shortData()[i] << ",";
      } else {
        ss << idx.data()[i] << ",";
      }
    }
    ss << ")";
    return ss.str();
  }
}
}

class location_manager;

template <typename T>
class manageable;

class manageable_base {
  friend class location_manager;

  template <typename T>
  friend class manageable;

  bool is_endpoint_ = false;
  bool has_upstream_ = false;
  CkArrayIndex upstream_;  // NOTE should be std::option
  std::vector<CkArrayIndex> downstream_;

  inline void set_upstream_(const CkArrayIndex &idx) {
    this->upstream_ = idx;
    this->has_upstream_ = true;
  }

  inline std::size_t num_downstream_(void) const {
    return this->downstream_.size();
  }

  virtual const CkArrayID &get_id_(void) const = 0;
  virtual const CkArrayIndex &get_index_(void) const = 0;
};

template <typename T>
class manageable : public T, public manageable_base {
  inline virtual const CkArrayID &get_id_(void) const override {
    return this->ckGetArrayID();
  }

  inline virtual const CkArrayIndex &get_index_(void) const override {
    return this->ckGetArrayIndex();
  }

 public:
  void ckPrintTree(void) {
    std::stringstream ss;

    auto upstream =
        (is_endpoint_) ? "endpoint"
                       : ((has_upstream_) ? util::idx2str(upstream_) : "unset");

    ss << util::idx2str(this->get_index_()) << "@nd" << CkMyNode();
    ss << "> has upstream " << upstream << " and downstream [";
    for (const auto &ds : downstream_) {
      ss << util::idx2str(ds) << ",";
    }
    ss << "]";

    CkPrintf("%s\n", ss.str().c_str());
  }
};

class location_manager : public CBase_location_manager, public array_listener {
 public:
  using index_type = CkArrayIndex;
  using element_type = manageable_base *;

  bool set_endpoint_ = false;

 protected:
  CmiNodeLock lock_;
  std::vector<CkArrayID> arrays_;

  using record_type = std::pair<element_type, int>;
  std::unordered_map<index_type, record_type, index_hasher> elements_;

  CkArray *spin_to_win(const CkArrayID &aid, const int &rank) {
    if (rank == CkMyRank()) {
      return aid.ckLocalBranch();
    } else {
      CkArray *inst = nullptr;
      while (nullptr == (inst = aid.ckLocalBranchOther(rank))) {
        CsdScheduler(0);
      }
      return inst;
    }
  }

  struct endpoint_ {
    element_type elt_;
    int node_;

    endpoint_(const element_type &elt) : elt_(elt), node_(-1) {}
    endpoint_(const int &node) : elt_(nullptr), node_(node) {}
  };

 public:
  location_manager(void) : CkArrayListener(0), lock_(CmiCreateLock()) {}

  virtual const PUP::able::PUP_ID &get_PUP_ID(void) const { NOT_IMPLEMENTED; }

  void unreg_array(const CkArrayID &, const CkCallback &) { NOT_IMPLEMENTED; }

  void reg_array(const CkArrayID &aid, const CkCallback &cb) {
    CmiLock(lock_);
    if (arrays_.empty()) {
      arrays_.emplace_back(aid);
    } else if (!(aid == arrays_.front())) {
      NOT_IMPLEMENTED;
    }
    CmiUnlock(lock_);

    // TODO this won't work if there's any simultaneous activity
    for (auto i = 0; i < CkMyNodeSize(); i += 1) {
      // TODO copy existing members into our table!
      spin_to_win(aid, i)->addListener(this);
    }

    this->contribute(cb);
  }

  ArrayElement *lookup(const CkArrayID &aid, const index_type &idx) {
    return dynamic_cast<ArrayElement *>(this->lookup(aid, idx, true));
  }

  void make_endpoint(const CkArrayID &aid, const CkArrayIndex &idx) {
    CmiLock(lock_);
    auto *elt = this->lookup(aid, idx, false);
    if (elt) {
      this->make_endpoint(elt);
    } else {
      // element has migrated
      NOT_IMPLEMENTED;
    }
    CmiUnlock(lock_);
  }

  void receive_upstream(const int &node, const CkArrayID &aid,
                        const index_type &idx) {
#if CMK_DEBUG
    CkPrintf("nd%d> received upstream from %d, idx=%s.\n", CkMyNode(), node,
             util::idx2str(idx).c_str());
#endif
    CmiLock(lock_);
    this->reg_upstream(endpoint_(node), aid, idx);
    CmiUnlock(lock_);
  }

  void receive_downstream(const int &node, const CkArrayID &aid,
                          const index_type &idx) {
#if CMK_DEBUG
    CkPrintf("nd%d> received downstream from %d, idx=%s.\n", CkMyNode(), node,
             util::idx2str(idx).c_str());
#endif
    CmiLock(lock_);
    auto target = this->reg_downstream(endpoint_(node), aid, idx);
    if (target != nullptr) {
      thisProxy[node].receive_upstream(CkMyNode(), aid, target->get_index_());
    }
    CmiUnlock(lock_);
  }

 protected:
  element_type lookup(const CkArrayID &aid, const index_type &idx,
                      const bool &lock) {
    if (lock) CmiLock(lock_);
    element_type elt = nullptr;
    auto search = this->elements_.find(idx);
    if (search != std::end(this->elements_)) {
      elt = search->second.first;
    }
    if (lock) CmiUnlock(lock_);
    return elt;
  }

  void make_endpoint(const element_type &elt) {
    CkAssertMsg(!this->set_endpoint_, "cannot register an endpoint twice");
    elt->is_endpoint_ = !this->set_endpoint_;
    this->set_endpoint_ = true;
  }

  void send_upstream(const endpoint_ &ep, const CkArrayID &aid,
                     const index_type &idx) {
    auto mine = CkMyNode();
    auto parent = binary_tree::parent(mine);

    if (parent >= 0) {
      auto src = ep.elt_ != nullptr ? mine : ep.node_;
      thisProxy[parent].receive_downstream(src, aid, idx);
    } else if (ep.elt_ != nullptr) {
      this->make_endpoint(ep.elt_);
    } else {
      thisProxy[ep.node_].make_endpoint(aid, idx);
      this->set_endpoint_ = true;
    }
  }

  void send_downstream(const endpoint_ &ep, const CkArrayID &aid,
                       const index_type &idx) {
    // unclear when this would occur, initial sync'ing should have destinations
    NOT_IMPLEMENTED;
  }

  using iter_type = typename decltype(elements_)::iterator;

  inline iter_type find_target(const bool &up) {
    using value_type = typename decltype(this->elements_)::value_type;
    if (up) {
      return std::find_if(std::begin(this->elements_),
                          std::end(this->elements_),
                          [](const value_type &val) -> bool {
                            auto &val_ = val.second.first;
                            return !(val_->has_upstream_ || val_->is_endpoint_);
                          });
    } else {
      return std::min_element(
          std::begin(this->elements_), std::end(this->elements_),
          [](const value_type &lhs, const value_type &rhs) -> bool {
            auto &lhs_ = lhs.second.first;
            auto &rhs_ = rhs.second.first;
            return (lhs_->num_downstream_() < rhs_->num_downstream_());
          });
    }
  }

  element_type reg_upstream(const endpoint_ &ep, const CkArrayID &aid,
                            const index_type &idx) {
    auto search = this->find_target(true);
    if (search == std::end(this->elements_)) {
      this->send_downstream(ep, aid, idx);
      return nullptr;
    } else {
      auto &target = search->second.first;
      target->set_upstream_(idx);
      return target;
    }
  }

  element_type reg_downstream(const endpoint_ &ep, const CkArrayID &aid,
                              const index_type &idx) {
    if (this->elements_.empty()) {
      this->send_upstream(ep, aid, idx);
      return nullptr;
    } else {
      auto &target = this->find_target(false)->second.first;
      target->downstream_.emplace_back(idx);
      return target;
    }
  }

  void associate(const element_type &elt) {
    auto target =
        this->reg_downstream(endpoint_(elt), elt->get_id_(), elt->get_index_());
    if (target != nullptr) {
      elt->set_upstream_(target->get_index_());
    }
  }

  void disassociate(const element_type &elt) {
    if (elt->is_endpoint_) {
      NOT_IMPLEMENTED;
    } else {
      NOT_IMPLEMENTED;
    }
  }

  void reg_element(ArrayElement *elt, const bool &created) {
    auto cast = dynamic_cast<element_type>(elt);
    const auto &idx = elt->ckGetArrayIndex();
    CmiLock(lock_);
    if (created) associate(cast);
    this->elements_[idx] = std::make_pair(cast, CkMyRank());
    CmiUnlock(lock_);
  }

  void unreg_element(ArrayElement *elt, const bool &died) {
    auto cast = dynamic_cast<element_type>(elt);
    const auto &idx = elt->ckGetArrayIndex();
    CmiLock(lock_);
    auto search = this->elements_.find(idx);
    if (search != std::end(this->elements_)) {
      this->elements_.erase(search);
    }
    if (died) disassociate(cast);
    CmiUnlock(lock_);
  }

 public:
  virtual void ckRegister(CkArray *, int) override {}

  virtual bool ckElementCreated(ArrayElement *elt) override {
    this->reg_element(elt, true);
    return array_listener::ckElementCreated(elt);
  }

  virtual bool ckElementArriving(ArrayElement *elt) override {
    this->reg_element(elt, false);
    return array_listener::ckElementArriving(elt);
  }

  virtual void ckElementDied(ArrayElement *elt) override {
    this->unreg_element(elt, true);
    array_listener::ckElementDied(elt);
  }

  virtual void ckElementLeaving(ArrayElement *elt) override {
    this->unreg_element(elt, false);
    array_listener::ckElementLeaving(elt);
  }

  virtual void pup(PUP::er &p) override { CBase_location_manager::pup(p); }
};

#endif
