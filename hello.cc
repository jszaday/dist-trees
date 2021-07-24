#include "nlocmgr.hh"

#include <memory>
#include <vector>

#include <hypercomm/core/typed_value.hpp>
#include <hypercomm/core/inter_callback.hpp>

constexpr int kMultiplier = 2;

/* readonly */ CProxy_Main mainProxy;

void enroll_polymorphs(void) {
  hypercomm::init_polymorph_registry();

  if (CkMyRank() == 0) {
    hypercomm::enroll<reduction_port<int>>();
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
 public:
  Test(void) = default;

  void make_contribution(void) {
    auto val = std::make_shared<typed_value<int>>(this->__index__());
    auto fn = std::make_shared<adder<int>>();
    auto cb = CkCallback(CkIndex_Main::done(nullptr), mainProxy);
    auto icb = std::make_shared<inter_callback>(cb);
    this->local_contribution(this->ckAllIdentity(), std::move(val), fn, icb);
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

    // each array requires its own completion detector for static
    // insertions, ensuring the spanning tree is ready before completion
    locProxy.reg_array(CProxy_CompletionDetector::ckNew(), testProxy,
                       CkCallback(CkIndex_Main::run(), thisProxy));
  }

  void run(void) {
    // initiate a phase of static insertion, suspending the thread
    // and resuming when we can begin. insertions are allowed when
    // all nodes have called "done_inserting"
    locProxy.begin_inserting(
        testProxy, CkCallbackResumeThread(),
        CkCallback(CkIndex_Test::make_contribution(), testProxy));

    for (auto i = 0; i < n; i += 1) {
      testProxy[i].insert();
    }

    locProxy.done_inserting(testProxy);
  }

  inline int expected(void) const {
    auto sum = 0;
    for (auto i = 0; i < n; i += 1) {
      sum += i;
    }
    return sum;
  }

  void done(CkMessage* msg) {
    auto value = std::make_shared<typed_value<int>>(msg);
    auto expected = this->expected();

    if (value->value() != expected) {
      CkAbort("fatal> got value %d from reduction (expected %d).\n",
              value->value(), expected);
    } else {
      CkPrintf("info> got value %d from reduction.\n", value->value());
    }

    CkExit();
  }
};

#include "hello.def.h"
