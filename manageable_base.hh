#ifndef __MANAGEABLE_BASE_HH__
#define __MANAGEABLE_BASE_HH__

#include "dist_sect.hh"

// NOTE eventually this will be replaced with a map
//      of sections to assocations :3
using association_ptr_ = std::unique_ptr<association_>;

class manageable_base_ {
  friend class location_manager;

  template <typename T>
  friend class manageable;

  template <typename T>
  friend class managed_identity;

  association_ptr_ association_;

  manageable_base_(void) : association_(nullptr) {}
  manageable_base_(association_ptr_&& association)
      : association_(std::forward<association_ptr_>(association)) {}

  inline void set_endpoint_(void) {
    this->association_->valid_upstream_ = true;
  }

  inline bool is_endpoint_(void) const {
    return this->association_ && this->association_->valid_upstream_ &&
           this->association_->upstream_.empty();
  }

  inline void put_upstream_(const CkArrayIndex& idx) {
    this->association_->valid_upstream_ = true;
    this->association_->upstream_.emplace_back(idx);
  }

  inline void put_downstream_(const CkArrayIndex& idx) {
    this->association_->downstream_.emplace_back(idx);
  }

  inline std::size_t num_downstream_(void) const {
    if (this->association_) {
      return this->association_->downstream_.size();
    } else {
      return std::size_t{};
    }
  }

  virtual const CkArrayID& get_id_(void) const = 0;
  virtual const CkArrayIndex& get_index_(void) const = 0;
};

#endif
