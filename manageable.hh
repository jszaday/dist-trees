#ifndef __MANAGEABLE_HH__
#define __MANAGEABLE_HH__

#include <hypercomm/core/locality.hpp>

#include "managed_imprintable.hh"

template <typename Index>
class identity_holder_ {
 public:
  using identity_ptr_ = std::shared_ptr<identity<Index>>;

  virtual const identity_ptr_& get_identity_(void) const = 0;
};

template <typename T>
class manageable : public T, public manageable_base_, public identity_holder_<typename T::index_type> {
  using index_type_ = typename T::index_type;
  using port_type_ = reduction_port<index_type_>;
  using identity_type_ = managed_identity<index_type_>;
  using typename identity_holder_<index_type_>::identity_ptr_;

  identity_ptr_ identity_;

  inline virtual const identity_ptr_& get_identity_(void) const override {
    return this->identity_;
  }

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
    auto copy = this->entry_ports;
    for (auto& val : copy) {
      auto port = std::dynamic_pointer_cast<port_type_>(val.first);
      if (port && (port->id <= stamp) && (port->index == idx)) {
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
            this->affect_ports(
                idx, it->stamp, [&](const std::shared_ptr<port_type_>& port) {
#if CMK_DEBUG
                  CkPrintf("info> replacing %s with %s.\n",
                           std::to_string(idx).c_str(),
                           std::to_string(next).c_str());
#endif
                  auto iter = this->entry_ports.find(port);
                  auto value = iter->second;
                  this->entry_ports.erase(iter);
                  port->index = next;
                  this->entry_ports.emplace(port, std::move(value));
                });
            break;
          }
          default:
            break;
        }
        this->staged_.erase(it);
        this->resolve_transactions();
        return;  // TODO resweep through?
      }
    }
  }

  void add_downstream(const CkArrayIndex& idx, const stamp_type& stamp) {
    auto &down = reinterpret_index<index_type_>(idx);
    this->association_->downstream_.emplace_back(idx);

    for (auto& pair : this->get_components_()) {
      auto rdcr = dynamic_cast<reducer*>(pair.second.get());
      if (rdcr != nullptr && rdcr->redn_no <= stamp) {
        auto port = std::make_shared<reduction_port<index_type_>>(rdcr->redn_no, down);
        // open another input port in the reducer (via increment)
        access_context_()->connect(port, rdcr->id, rdcr->n_ustream++);
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

  void replace_downstream(const CkArrayIndex& from,
                          std::vector<CkArrayIndex>&& to, stamp_type&& stamp) {
    this->update_context();
    if (to.empty()) {
      this->staged_.emplace_back(from, std::move(stamp));
    } else {
      for (auto it = (std::begin(to) + 1); it != std::end(to); it += 1) {
        this->add_downstream(*it, stamp);
      }

      this->staged_.emplace_back(from, *(std::begin(to)), std::move(stamp));
    }
    this->resolve_transactions();
  }

  // debugging helper method
  inline void ckPrintTree(const char* msg) const {
    std::stringstream ss;

    auto upstream =
        !(association_ && association_->valid_upstream_)
            ? "unset"
            : (association_->upstream_.empty()
                   ? "endpoint"
                   : utilities::idx2str(association_->upstream_.front()));

    ss << utilities::idx2str(this->get_index_()) << "@nd" << CkMyNode();
    ss << "> " << msg << ", has upstream " << upstream << " and downstream [";
    for (const auto& ds : association_->downstream_) {
      ss << utilities::idx2str(ds) << ",";
    }
    ss << "]";

    CkPrintf("%s\n", ss.str().c_str());
  }
};

template<typename Index>
const typename managed_imprintable<Index>::identity_ptr& managed_imprintable<Index>::imprint(const locality_ptr& locality) const {
  return dynamic_cast<identity_holder_<Index>*>(locality)->get_identity_();
}


template<typename Index>
const Index& managed_imprintable<Index>::pick_root(const proxy_ptr& proxy, Index* favored) const {
  if (favored) {
    return *favored;
  } else {
    // TODO handle this more carefully -- stepwise ensure validity
    // auto aid = std::dynamic_pointer_cast<array_proxy>(proxy)->id();
    // auto* locMgr = CProxy_ArrayBase(aid).ckLocMgr();
    // return reinterpret_index<Index>(std::begin(locMgr->idx2id)->first);
    NOT_IMPLEMENTED;
  }
}

#endif
