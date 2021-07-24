#ifndef __MANAGEABLE_HH__
#define __MANAGEABLE_HH__

#include "managed_identity.hh"

template <typename T>
class manageable : public T, public manageable_base_ {
  using index_type_ = typename T::index_type;
  using identity_type_ = managed_identity<index_type_>;
  using identity_ptr_ = std::shared_ptr<identity<index_type_>>;

  identity_ptr_ identity_;

  inline virtual const CkArrayID& get_id_(void) const override {
    return this->ckGetArrayID();
  }

  inline virtual const CkArrayIndex& get_index_(void) const override {
    return this->ckGetArrayIndex();
  }

 public:
  // used for static insertion, default initialize
  manageable(void) : identity_(new identity_type_(this)) {}

  // used for dynamic insertion, imprints child with parent data
  manageable(association_ptr_&& association, const reduction_id_t& seed)
      : manageable_base_(std::forward<association_ptr_>(association)),
        identity_(new identity_type_(this, seed)) {}

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