
#ifndef __MANAGED_IDENTITY_HH__
#define __MANAGED_IDENTITY_HH__

#include "dist_sect.hh"

#include <hypercomm/sections/identity.hpp>

template<typename Index>
class managed_identity: public identity<Index> {
  const manageable_base* inst_;
 public:
  managed_identity(const manageable_base *inst)
  : inst_(inst) {}

  virtual const Index& mine(void) const override {
    return reinterpret_index<Index>(inst_->get_index_());
  }

  virtual std::vector<Index> downstream(void) const {
    CkAssert(inst_->known_upstream_());
    if (inst_->is_endpoint_) {
      return {};
    } else {
      return { reinterpret_index<Index>(inst_->upstream_) };
    }
  }

  virtual std::vector<Index> upstream(void) const {
    std::vector<Index> ds(inst_->num_downstream_());
    std::transform(std::begin(inst_->downstream_), std::end(inst_->downstream_), std::begin(ds),
                  [](const CkArrayIndex& val) { return reinterpret_index<Index>(val); });
    return ds;
  }
};

#endif
