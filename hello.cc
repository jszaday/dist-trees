#include "nlocmgr.hh"

#include <memory>
#include <vector>

constexpr int kMultiplier = 2;

/* readonly */ CProxy_Main mainProxy;

void enroll_polymorphs(void) {
  hypercomm::init_polymorph_registry();

  if (CkMyRank() == 0) {
    hypercomm::enroll<persistent_port>();
    hypercomm::enroll<reduction_port<int>>();
    hypercomm::enroll<broadcaster<int>>();
    hypercomm::enroll<generic_section<int>>();
  }
}

template <typename T>
class adder : public core::combiner {
  virtual combiner::return_type send(combiner::argument_type&& args) override {
    if (args.empty()) {
      return {};
    } else {
      auto accum = value2typed<T>(std::move(args.back()));
      args.pop_back();
      for (auto& arg : args) {
        auto typed = value2typed<T>(std::move(arg));
        accum->value() += typed->value();
      }
      return accum;
    }
  }

  virtual void __pup__(hypercomm::serdes& s) override {}
};

class Test : public manageable<vil<CBase_Test, int>> {
  identity_ptr ident_;

 public:
  Test(void) { ident_ = std::make_shared<managed_identity<int>>(this); }

  void make_contribution(void) {
    auto val = std::make_shared<typed_value<int>>(this->__index__());
    auto fn = std::make_shared<adder<int>>();
    auto cb = CkCallback(CkIndex_Main::done(nullptr), mainProxy);
    auto icb = std::make_shared<inter_callback>(cb);
    this->local_contribution(this->ident_, std::move(val), fn, icb);
  }
};

class Main : public CBase_Main {
  CProxy_Test testProxy;
  CProxy_location_manager locProxy;
  int n;

 public:
  Main(CkArgMsg* msg) : n(kMultiplier * CkNumPes()) {
    mainProxy = thisProxy;
    testProxy = CProxy_Test::ckNew();
    locProxy = CProxy_location_manager::ckNew();
    locProxy.reg_array(testProxy, CkCallback(CkIndex_Main::run(), thisProxy));
  }

  void run(void) {
    for (auto i = 0; i < n; i += 1) {
      testProxy[i].insert();
    }

    CkWaitQD();

    testProxy.make_contribution();
  }

  void done(CkMessage* msg) {
    auto value = std::make_shared<typed_value<int>>(msg);
    CkPrintf("main> got value %d from reduction.\n", value->value());
    CkExit();
  }
};

#include "hello.def.h"
