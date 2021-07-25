#ifndef __COMMON_HH__
#define __COMMON_HH__

#include <hypercomm/utilities.hpp>
#include <hypercomm/components/identifiers.hpp>

using namespace hypercomm;

using array_listener = CkArrayListener;
using array_id_hasher = ArrayIDHasher;
using array_index_hasher = IndexHasher;

template <typename T>
class manageable;

class manageable_base_;

struct association_ {
  std::vector<CkArrayIndex> upstream_;
  std::vector<CkArrayIndex> downstream_;
  bool valid_upstream_ = false;

  association_(void) = default;
  association_(const association_&) = delete;

  void pup(PUP::er& p) {
    p | this->upstream_;
    p | this->downstream_;
    p | this->valid_upstream_;
  }
};

#endif