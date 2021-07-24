#ifndef __MANAGEABLE_HH__
#define __MANAGEABLE_HH__

#include "manageable_base.hh"

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
                       : ((has_upstream_) ? utilities::idx2str(upstream_) : "unset");

    ss << utilities::idx2str(this->get_index_()) << "@nd" << CkMyNode();
    ss << "> has upstream " << upstream << " and downstream [";
    for (const auto &ds : downstream_) {
      ss << utilities::idx2str(ds) << ",";
    }
    ss << "]";

    CkPrintf("%s\n", ss.str().c_str());
  }
};

#endif