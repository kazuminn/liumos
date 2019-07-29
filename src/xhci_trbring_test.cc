#include "xhci_trbring.h"

#ifdef LIUMOS_TEST

#include <stdio.h>
#include <cassert>

template <int N>
void TestTransferRequestBlockRing() {
  printf("Testing TransferRequestBlockRing<%d>...\n", N);
  TransferRequestBlockRing<N> ring;
  ring.Init(0);
  for (int i = 0; i < N; i++) {
    assert(ring.GetNextEnqueueIndex() == i);
    assert(reinterpret_cast<uint64_t>(&ring) + 16 * i ==
           ring.template GetNextEnqueueEntry<uint64_t>());
    assert(ring.GetCurrentCycleState() == 1);
    assert(ring.Push() == 1);
  }
  for (int i = 0; i < N; i++) {
    assert(ring.GetNextEnqueueIndex() == i);
    assert(reinterpret_cast<uint64_t>(&ring) + 16 * i ==
           ring.template GetNextEnqueueEntry<uint64_t>());
    assert(ring.GetCurrentCycleState() == 0);
    assert(ring.Push() == 0);
  }
  assert(ring.GetNextEnqueueIndex() == 0);
  assert(ring.GetCurrentCycleState() == 1);
}

int main() {
  TestTransferRequestBlockRing<256>();
  TestTransferRequestBlockRing<17>();
  TestTransferRequestBlockRing<1>();

  puts("PASS");
  return 0;
}

#endif
