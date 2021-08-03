#ifndef __MANAGED_IMPRINTABLE_HH__
#define __MANAGED_IMPRINTABLE_HH__

#include "managed_identity.hh"

#include <hypercomm/sections/imprintable.hpp>

template <typename Index>
class managed_imprintable : public imprintable<Index> {
 public:
  managed_imprintable(void) = default;

  // TODO include proxy in this as well :)
  virtual bool is_member(const Index&) const { return true; }

  virtual hash_code hash(void) const { return typeid(*this).hash_code(); }

  virtual bool equals(const std::shared_ptr<comparable>& other) const {
    return (bool)std::dynamic_pointer_cast<managed_imprintable<Index>>(other);
  }

  // pick the root for the spanning tree, with a favored candidate
  virtual const Index& pick_root(const proxy_ptr& proxy,
                                 const Index* favored) const;

  using identity_ptr = typename imprintable<Index>::identity_ptr;
  using typename imprintable<Index>::locality_ptr;

  // apply this imprintable to a locality (generating an identity)
  virtual const identity_ptr& imprint(const locality_ptr&) const;
};

#endif
