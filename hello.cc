#include "nlocmgr.hh"

#include <memory>
#include <vector>

constexpr int kMultiplier = 2;

class Test : public manageable<CBase_Test> {
 public:
  Test(void) = default;

  void callRing(void) {
    this->ckPrintTree();
  }

  void ring(const int& homePe) {}
};

class Main : public CBase_Main {
  CProxy_Test testProxy;
  CProxy_location_manager locProxy;
  int n;
 public:
  Main(CkArgMsg* msg) : n(CkNumPes()) {
    testProxy = CProxy_Test::ckNew();
    locProxy = CProxy_location_manager::ckNew();
    locProxy.reg_array(testProxy, CkCallback(CkIndex_Main::run(), thisProxy));
  }

  void run(void) {
    for (auto i = 0; i < n; i += 1) {
      testProxy[i].insert();
    }

    CkWaitQD();

    testProxy.callRing();

    CkExitAfterQuiescence();
  }
};

#include "hello.def.h"
