
# Component-Based Distributed Tree Construction 
This repo establishes a prototype, component-based scheme for distributed spanning tree construction. Its per-element operating granularity bestows it with robust guarantees regarding element migration, creation, and deletion. Effectively, each element always "knows" its parent and children, which allows them to fire-and-forget reductions at-will without having to be concerned about these complications. The exact nature of this scheme is discussed more thoroughly in [this document](https://docs.google.com/document/d/1hv-9qm1dXR8R1VJXgtyFHuhTUoa_izrm-jDXPqqkpas/edit#).

To build and experiment with this repo, you will need a built copy of Charm++ with [PR#3431](https://github.com/UIUC-PPL/charm/pull/3431), and a built copy of Hypercomm with [PR#40](https://github.com/jszaday/hypercomm/pull/40) (the latter has been merged). Note, Hypercomm currently has to be built in parallel (`make -j`). These should be present at `CHARM_HOME` and `HYPERCOMM_HOME` respectively.

Once these features are merged into their respective repos, it is intended for this repo to be merged into Hypercomm. That said, as Charm++'s section-related efforts progress, it could be adopted by Charm++ as well.
