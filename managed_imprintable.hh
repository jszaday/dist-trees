#ifndef __MANAGED_IMPRINTABLE_HH__
#define __MANAGED_IMPRINTABLE_HH__

#include "managed_identity.hh"

#include <hypercomm/sections/imprintable.hpp>

template <typename Index>
class managed_imprintable : public imprintable<Index> {
 public:
  managed_imprintable(void) = default;

  managed_imprintable(PUP::reconstruct) {}

  // TODO include proxy in this as well :)
  virtual bool is_member(const Index&) const { return true; }

  virtual bool equals(const std::shared_ptr<comparable>& other) const {
    return (bool)std::dynamic_pointer_cast<managed_imprintable<Index>>(other);
  }

  // pick the root for the spanning tree, with a favored candidate
  virtual const Index& pick_root(const proxy_ptr& proxy,
                                 const Index* favored) const;

  using identity_ptr = typename imprintable<Index>::identity_ptr;
  using typename imprintable<Index>::locality_ptr;

  virtual identity_ptr imprint(const locality_ptr&) const { NOT_IMPLEMENTED; }

  virtual hash_code hash(void) const {
    return hash_type<managed_imprintable<Index>>();
  }

  virtual void __pup__(serdes&) {}

  static const std::shared_ptr<imprintable<Index>>& instance(void) {
    static std::shared_ptr<imprintable<Index>> instance_ =
        std::make_shared<managed_imprintable<Index>>();
    return instance_;
  }
};

template <typename Index>
std::shared_ptr<imprintable_base_> managed_identity<Index>::get_imprintable(
    void) const {
  return managed_imprintable<Index>::instance();
}

#endif
