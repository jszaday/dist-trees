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

class location_manager : public CBase_location_manager,
                         public array_listener {
 public:
  using index_type = CkArrayIndex;
  using element_type = ArrayElement *;

  // (NODE, AID, IDX)
  using update_type = std::tuple<int, CkArrayID, index_type>;

  std::vector<update_type> upstream_;
  std::vector<update_type> downstream_;

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

    // TODO fix this
    for (auto i = 0; i < CkMyNodeSize(); i += 1) {
      spin_to_win(aid, i)->addListener(this);
    }

    // TODO copy known to this chare

    this->contribute(cb);
  }

  void unreg_array(const CkArrayID&, const CkCallback&) { NOT_IMPLEMENTED; }

  element_type lookup(const CkArrayID &aid, const index_type &idx) {
    CmiLock(lock_);
    auto search = this->elements_.find(idx);
    int rank = -1;
    if (search != std::end(this->elements_)) {
      rank = search->second;
    }
    CmiUnlock(lock_);
    if (rank >= 0) {
      return spin_to_win(aid, rank)->lookup(idx);
    } else {
      return nullptr;
    }
  }

  void receive_upstream(update_type&& update) {
    CmiLock(lock_);
    this->upstream_.emplace_back(std::move(update));
    CmiUnlock(lock_);
  }

  void receive_downstream(update_type&& update) {
    CmiLock(lock_);
    this->downstream_.emplace_back(std::move(update));
    CmiUnlock(lock_);
  }

  static void send_upstream(const CkArrayID& aid, const index_type& idx) {
    auto mine = CkMyNode();
    auto parent = binary_tree::parent(mine);
    if (parent >= 0) {
      auto update = std::forward_as_tuple(mine, aid, idx);
      // TODO send
    }
  }

 protected:
  void first_element(const element_type &elt) {
    if (this->upstream_.empty()) {
      send_upstream(elt->ckGetArrayID(), elt->ckGetArrayIndex());
    }
  }

  void last_element(const element_type &elt) {}

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
    auto last = this->elements_.empty();
    if (last) {
      this->last_element(elt);
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

  virtual void pup(PUP::er& p) override {
    CBase_location_manager::pup(p);
  }
};

#endif
