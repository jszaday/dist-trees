#ifndef __NODE_LEVEL_LOC_MGR_HH__
#define __NODE_LEVEL_LOC_MGR_HH__

#include "manageable.hh"

#include <completion.h>
#include <hypercomm/core/math.hpp>
#include <hypercomm/core/locality.hpp>

// TODO move location_manager into its own module
#include "hello.decl.h"

#define NOT_IMPLEMENTED CkAbort("not yet implemented")

class location_manager;

class location_manager : public CBase_location_manager, public array_listener {
 public:
  using index_type = CkArrayIndex;
  using element_type = manageable_base_ *;
  using detector_type = CProxy_CompletionDetector;

  bool set_endpoint_ = false;

 protected:
  CmiNodeLock lock_;

  using record_type = std::pair<element_type, int>;

  std::unordered_map<CkArrayID, bool, array_id_hasher> insertion_statuses_;
  std::unordered_map<CkArrayID, detector_type, array_id_hasher> arrays_;
  std::unordered_map<index_type, record_type, array_index_hasher> elements_;

  inline CompletionDetector *detector_for(const CkArrayID &aid) const {
#if CMK_ERROR_CHECKING
    auto search = this->arrays_.find(aid);
    if (search == std::end(this->arrays_)) {
      CkAbort("missing completion detector!");
    }
    const auto &proxy = search->second;
#else
    const auto &proxy = this->arrays_[aid];
#endif
    CompletionDetector *inst = nullptr;
    while (nullptr == (inst = proxy.ckLocalBranch())) {
      CsdScheduler(0);
    }
    return inst;
  }

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

  struct contribute_helper_ {
    CProxy_location_manager manager_;
    CkCallback cb_;

    static void action(contribute_helper_ *self, void *msg) {
      self->manager_.ckLocalBranch()->contribute(self->cb_);
      delete self;
      CkFreeMsg(msg);
    }

    static contribute_helper_ *instantiate(location_manager *manager,
                                           const CkCallback &cb) {
      return new contribute_helper_{.manager_ = manager->thisProxy, .cb_ = cb};
    }
  };

 public:
  location_manager(void) : CkArrayListener(0), lock_(CmiCreateLock()) {}

  virtual const PUP::able::PUP_ID &get_PUP_ID(void) const { NOT_IMPLEMENTED; }

  void unreg_array(const CkArrayID &, const CkCallback &) { NOT_IMPLEMENTED; }

  void reg_array(const detector_type &detector, const CkArrayID &aid,
                 const CkCallback &cb) {
    CmiLock(lock_);
    auto search = this->arrays_.find(aid);
    if (search == std::end(this->arrays_)) {
      if (!this->arrays_.empty()) {
        NOT_IMPLEMENTED;
      }

      this->arrays_[aid] = detector;
    } else {
      CkAbort("array registered twice!");
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

  void begin_inserting(const CkArrayID &aid, const CkCallback &start,
                       const CkCallback &finish) {
    this->insertion_statuses_[aid] = true;
    if (thisIndex == 0) {
      auto *arg = contribute_helper_::instantiate(this, start);
      auto cb = CkCallback((CkCallbackFn)&contribute_helper_::action, arg);
      this->arrays_[aid].start_detection(CkNumNodes(), cb, CkCallback(), finish,
                                         0);
    } else {
      this->contribute(start);
    }
  }

  void done_inserting(const CkArrayID &aid) {
    this->insertion_statuses_[aid] = false;
    this->detector_for(aid)->done();
  }

  void make_endpoint(const CkArrayID &aid, const CkArrayIndex &idx) {
    CmiLock(lock_);
    auto *elt = this->lookup(aid, idx, false);
    if (elt) {
      this->make_endpoint(elt);
      this->detector_for(aid)->consume();
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
             utilities::idx2str(idx).c_str());
#endif
    CmiLock(lock_);
    this->reg_upstream(endpoint_(node), aid, idx);
    this->detector_for(aid)->consume();
    CmiUnlock(lock_);
  }

  void receive_downstream(const int &node, const CkArrayID &aid,
                          const index_type &idx) {
#if CMK_DEBUG
    CkPrintf("nd%d> received downstream from %d, idx=%s.\n", CkMyNode(), node,
             utilities::idx2str(idx).c_str());
#endif
    CmiLock(lock_);
    auto target = this->reg_downstream(endpoint_(node), aid, idx);
    if (target != nullptr) {
      // we implicitly produce (1) at this point
      thisProxy[node].receive_upstream(CkMyNode(), aid, target->get_index_());
    } else {
      // so we only consume (1) in the else branch
      this->detector_for(aid)->consume();
    }
    CmiUnlock(lock_);
  }

  inline bool is_inserting(const CkArrayID &aid) const {
    auto search = this->insertion_statuses_.find(aid);
    return (search == std::end(this->insertion_statuses_)) ? false
                                                           : search->second;
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
    this->set_endpoint_ = true;
    elt->set_endpoint_();
  }

  void send_upstream(const endpoint_ &ep, const CkArrayID &aid,
                     const index_type &idx) {
    auto mine = CkMyNode();
    auto parent = binary_tree::parent(mine);

    if (parent >= 0) {
      auto src = ep.elt_ != nullptr ? mine : ep.node_;
      thisProxy[parent].receive_downstream(src, aid, idx);
      this->detector_for(aid)->produce();
    } else if (ep.elt_ != nullptr) {
      this->make_endpoint(ep.elt_);
    } else {
      thisProxy[ep.node_].make_endpoint(aid, idx);
      this->set_endpoint_ = true;
      this->detector_for(aid)->produce();
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
      return std::find_if(
          std::begin(this->elements_), std::end(this->elements_),
          [](const value_type &val) -> bool {
            auto &val_ = val.second.first;
            return !(val_->association_ && val_->association_->valid_upstream_);
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
      target->put_upstream_(idx);
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
      target->put_downstream_(idx);
      return target;
    }
  }

  void try_reassociate(const element_type &elt) { NOT_IMPLEMENTED; }

  void associate(const element_type &elt) {
    auto &aid = elt->get_id_();
    auto &assoc = elt->association_;
    // if we are (statically) inserting an unassociated element
    if (this->is_inserting(aid) && !assoc) {
      // create a new association for the element
      assoc.reset(new ::association_);
      // and set its up/downstream
      auto target = this->reg_downstream(endpoint_(elt), elt->get_id_(),
                                         elt->get_index_());
      if (target != nullptr) {
        elt->put_upstream_(target->get_index_());
      }
    } else {
      CkAssertMsg(assoc, "dynamic insertions must be associated");
      this->try_reassociate(elt);
    }
  }

  void disassociate(const element_type &elt) {
    if (elt->is_endpoint_()) {
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
