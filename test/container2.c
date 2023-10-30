#include "test.h"

int main(void) {
  $vec(char*) vc = {0};

  vc..push_back("zip");
  vc..push_back("zap");
  vc..push_back("zoot");

  ASSERT(0, strcmp("zip", *vc..at(0)));
  ASSERT(0, strcmp("zap", *vc..at(1)));
  ASSERT(0, strcmp("zoot", *vc..at(2)));

  vc..drop();
}
