#ifndef __MANAGEABLE_BASE_HH__
#define __MANAGEABLE_BASE_HH__

#include "dist_sect.hh"

class manageable_base {
  friend class location_manager;

  template <typename T>
  friend class manageable;

  template <typename T>
  friend class managed_identity;

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

  inline bool known_upstream_(void) const {
    return this->has_upstream_ || this->is_endpoint_;
  }

  virtual const CkArrayID &get_id_(void) const = 0;
  virtual const CkArrayIndex &get_index_(void) const = 0;
};

#include "managed_identity.hh"

#endif
