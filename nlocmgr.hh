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

class location_manager;

class manageable {
  friend class location_manager;
  bool is_endpoint_ = false;
};

class location_manager : public CBase_location_manager, public array_listener {
 public:
  using index_type = CkArrayIndex;
  using element_type = ArrayElement *;

  // (NODE, AID, IDX)
  using update_type = std::tuple<int, CkArrayID, index_type>;

  std::vector<update_type> upstream_;
  std::vector<update_type> downstream_;

  bool set_endpoint_ = false;

 protected:
  CmiNodeLock lock_;
  std::vector<CkArrayID> arrays_;
  std::unordered_map<index_type, int, index_hasher> elements_;

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

 public:
  location_manager(void) : CkArrayListener(0), lock_(CmiCreateLock()) {}

  virtual const PUP::able::PUP_ID &get_PUP_ID(void) const { NOT_IMPLEMENTED; }

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

  void unreg_array(const CkArrayID &, const CkCallback &) { NOT_IMPLEMENTED; }

  element_type lookup(const CkArrayID &aid, const index_type &idx) {
    return this->lookup(aid, idx, true);
  }

  void make_endpoint(const CkArrayID &aid, const CkArrayIndex &idx) {
    CmiLock(lock_);
    auto *elt = this->lookup(aid, idx, false);
    if (elt) {
      this->make_endpoint(elt);
    } else {
      NOT_IMPLEMENTED;
    }
    CmiUnlock(lock_);
  }

  void receive_upstream(const int &node, const CkArrayID &aid,
                        const index_type &idx) { NOT_IMPLEMENTED; }

  void receive_downstream(const int &node, const CkArrayID &aid,
                          const index_type &idx) {
    CmiLock(lock_);
    if (this->elements_.empty()) {
      if (!this->send_upstream(aid, idx)) {
        thisProxy[node].make_endpoint(aid, idx);
        this->set_endpoint_ = true;
      }
    } else {
      NOT_IMPLEMENTED;
    }
    CmiUnlock(lock_);
  }

 protected:
  element_type lookup(const CkArrayID &aid, const index_type &idx,
                      const bool &lock) {
    if (lock) CmiLock(lock_);
    auto search = this->elements_.find(idx);
    int rank = -1;
    if (search != std::end(this->elements_)) {
      rank = search->second;
    }
    if (lock) CmiUnlock(lock_);
    if (rank >= 0) {
      return spin_to_win(aid, rank)->lookup(idx);
    } else {
      return nullptr;
    }
  }

  void make_endpoint(const element_type &elt) {
    CkAssertMsg(!this->set_endpoint_, "cannot register an endpoint twice");
    dynamic_cast<manageable *>(elt)->is_endpoint_ = !this->set_endpoint_;
    this->set_endpoint_ = true;
  }

  bool send_upstream(const CkArrayID &aid, const index_type &idx) {
    auto mine = CkMyNode();
    auto parent = binary_tree::parent(mine);

    if (parent >= 0) {
      thisProxy[parent].receive_downstream(mine, aid, idx);
      return true;
    } else {
      return false;
    }
  }

  void first_element(const element_type &elt) {
    auto sent =
        this->send_upstream(elt->ckGetArrayID(), elt->ckGetArrayIndex());
    if (!sent) {
      this->make_endpoint(elt);
    }
  }

  void reg_element(const element_type &elt) {
    const auto &idx = elt->ckGetArrayIndex();
    CmiLock(lock_);
    auto first = this->elements_.empty();
    this->elements_[idx] = CkMyRank();
    if (first) {
      this->first_element(elt);
    }
    CmiUnlock(lock_);
  }

  void unreg_element(const element_type &elt) {
    const auto &idx = elt->ckGetArrayIndex();
    CmiLock(lock_);
    auto search = this->elements_.find(idx);
    if (search != std::end(this->elements_)) {
      this->elements_.erase(search);
    }
    if (dynamic_cast<manageable *>(elt)->is_endpoint_) {
      NOT_IMPLEMENTED;
    }
    CmiUnlock(lock_);
  }

 public:
  virtual void ckRegister(CkArray *, int) override {}

  virtual bool ckElementCreated(ArrayElement *elt) override {
    this->reg_element(elt);
    return array_listener::ckElementCreated(elt);
  }

  virtual bool ckElementArriving(ArrayElement *elt) override {
    this->reg_element(elt);
    return array_listener::ckElementArriving(elt);
  }

  virtual void ckElementDied(ArrayElement *elt) override {
    this->unreg_element(elt);
    array_listener::ckElementDied(elt);
  }

  virtual void ckElementLeaving(ArrayElement *elt) override {
    this->unreg_element(elt);
    array_listener::ckElementLeaving(elt);
  }

  virtual void pup(PUP::er &p) override { CBase_location_manager::pup(p); }
};

#endif
