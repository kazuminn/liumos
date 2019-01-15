#include "guid.h"

packed_struct GUIDForCompare {
  uint64_t data1;
  uint64_t data2;
};

bool IsEqualGUID(const GUID* guid1, const GUID* guid2) {
  const GUIDForCompare* g1 = reinterpret_cast<const GUIDForCompare*>(guid1);
  const GUIDForCompare* g2 = reinterpret_cast<const GUIDForCompare*>(guid2);
  return g1->data1 == g2->data1 && g1->data2 == g2->data2;
}
