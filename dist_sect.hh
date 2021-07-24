#ifndef __DIST_SECT_HH__
#define __DIST_SECT_HH__

#include <hypercomm/utilities.hpp>

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
  CmiNodeLock lock_;

  association_(void): lock_(CmiCreateLock()) {}
  association_(const association_&) = delete;
};

#endif