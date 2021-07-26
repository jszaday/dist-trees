#ifndef __MANAGEABLE_HH__
#define __MANAGEABLE_HH__

#include "managed_identity.hh"

template <typename T>
class manageable : public T, public manageable_base_ {
  using index_type_ = typename T::index_type;
  using port_type_ = reduction_port<index_type_>;
  using identity_type_ = managed_identity<index_type_>;
  using identity_ptr_ = std::shared_ptr<identity<index_type_>>;

  identity_ptr_ identity_;

  inline virtual const CkArrayID& get_id_(void) const override {
    return this->ckGetArrayID();
  }

  inline virtual const CkArrayIndex& get_index_(void) const override {
    return this->ckGetArrayIndex();
  }

  virtual const stamp_type& get_stamp_(void) const override {
    return this->identity_->last_reduction();
  }

  virtual const component_map& get_components_(void) const override {
    return this->components;
  }

  virtual CkLocMgr* get_loc_mgr_(void) const override {
    return this->thisProxy.ckLocMgr();
  }

  enum transaction_type_ { kReplace, kDelete };

  struct transaction_ {
    transaction_type_ type;
    CkArrayIndex from;
    CkArrayIndex to;
    stamp_type stamp;

    transaction_(const CkArrayIndex& _1, stamp_type&& _2)
        : type(kDelete), from(_1), stamp(_2) {}

    transaction_(const CkArrayIndex& _1, const CkArrayIndex& _2,
                 stamp_type&& _3)
        : type(kReplace), from(_1), to(_2), stamp(_3) {}
  };

  std::vector<transaction_> staged_;

  template <typename Fn>
  void affect_ports(const index_type_& idx, const stamp_type& stamp,
                    const Fn& fn) {
    for (auto& val : this->entry_ports) {
      auto port = std::dynamic_pointer_cast<port_type_>(val.first);
      if (port && (port->id >= stamp) && (port->index == idx)) {
        fn(port);
      }
    }
  }

  void resolve_transactions(void) {
    auto& ds = this->association_->downstream_;
    for (auto it = std::begin(this->staged_); it != std::end(this->staged_);
         it++) {
      auto& idx = reinterpret_index<index_type_>(it->from);
      auto search = std::find(std::begin(ds), std::end(ds), it->from);
      if (search != std::end(ds)) {
        switch (it->type) {
          case kDelete: {
            ds.erase(search);
            this->affect_ports(
                idx, it->stamp, [&](const std::shared_ptr<port_type_>& port) {
#if CMK_DEBUG
                  CkPrintf("info> sending invalidation to %s.\n",
                           std::to_string(idx).c_str());
#endif
                  this->receive_value(port, std::move(value_ptr{}));
                });
            break;
          }
          case kReplace: {
            *search = it->to;
            auto& next = reinterpret_index<index_type_>(it->to);
            this->affect_ports(idx, it->stamp,
                               [&](const std::shared_ptr<port_type_>& port) {
#if CMK_DEBUG
                                 CkPrintf("info> replacing %s with %s.\n",
                                          std::to_string(idx).c_str(),
                                          std::to_string(next).c_str());
#endif
                                 port->index = next;
                               });
            break;
          }
          default:
            break;
        }
        this->staged_.erase(it);
        break; // TODO resweep through?
      }
    }
  }

 public:
  // used for static insertion, default initialize
  manageable(void) : identity_(new identity_type_(this)) {}

  // used for dynamic insertion, imprints child with parent data
  manageable(association_ptr_&& association, const reduction_id_t& seed)
      : manageable_base_(std::forward<association_ptr_>(association)),
        identity_(new identity_type_(this, seed)) {}

  void delete_downstream(const CkArrayIndex& idx, stamp_type&& stamp) {
    this->update_context();
    this->staged_.emplace_back(idx, std::move(stamp));
    this->resolve_transactions();
  }

  void replace_downstream(const CkArrayIndex& from, const CkArrayIndex& to,
                          stamp_type&& stamp) {
    this->update_context();
    this->staged_.emplace_back(from, to, std::move(stamp));
    this->resolve_transactions();
  }

  // accessor for ( identity ) spanning all members of the array
  inline const identity_ptr_& ckAllIdentity(void) const {
    return this->identity_;
  }

  // debugging helper method
  inline void ckPrintTree(void) const {
    std::stringstream ss;

    auto upstream =
        !(association_ && association_->valid_upstream_)
            ? "unset"
            : (association_->upstream_.empty()
                   ? "endpoint"
                   : utilities::idx2str(association_->upstream_.front()));

    ss << utilities::idx2str(this->get_index_()) << "@nd" << CkMyNode();
    ss << "> has upstream " << upstream << " and downstream [";
    for (const auto& ds : association_->downstream_) {
      ss << utilities::idx2str(ds) << ",";
    }
    ss << "]";

    CkPrintf("%s\n", ss.str().c_str());
  }
};

#endif
