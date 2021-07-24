#ifndef __DIST_SECT_HH__
#define __DIST_SECT_HH__

#include <hypercomm/utilities.hpp>

using namespace hypercomm;

using array_listener = CkArrayListener;
using array_id_hasher = ArrayIDHasher;
using array_index_hasher = IndexHasher;

template <typename T>
class manageable;

class manageable_base;

class association_ {
  friend class manageable_base_;

  std::vector<CkArrayIndex> upstream_;
  std::vector<CkArrayIndex> downstream_;
};

#endif