from test_helpers_for_update import *

HOST = '''\
#include <stdio.h>

struct B {
    int x;
    int y;
    int z;
};

void test_f(struct B b) {
  printf("%d %d %d\\n", b.x, b.y, b.z);
}
'''

SRC = '''\
#include <stdio.h>

struct B {
    int x;
    int y;
    int z;
};

extern void test_f(struct B b);

// This was causing stack un-alignment due to how struct copying was
// implemented in codegen.
int main(void) {
  struct B b = {10, 20, 30};
  test_f(b);
  printf("OK\\n");
  return 0;
}
'''

# This is not really an "update" test, but because the update tests happen to
# build a separate host binary rather than using the standard dyibicc.exe, it
# gives us a way to have C code to call that's test-specific and compiled by
# the host compiler, rather than by ours.

add_to_host(HOST);
add_host_helper_func("test_f")
include_path("../../test")

initial({'main.c': SRC})
update_ok()
expect(0)

done()

