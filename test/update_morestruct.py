from test_helpers_for_update import *

HOST = r'''
#include <stdlib.h>

typedef struct Color {
    unsigned char r;        // Color red value
    unsigned char g;        // Color green value
    unsigned char b;        // Color blue value
    unsigned char a;        // Color alpha value
} Color;

Color Fade(Color color, float alpha)
{
    if (alpha < 0.0f) alpha = 0.0f;
    else if (alpha > 1.0f) alpha = 1.0f;

    return (Color){ color.r, color.g, color.b, (unsigned char)(255.0f*alpha) };
}

'''

SRC = r'''
typedef struct Color {
    unsigned char r;        // Color red value
    unsigned char g;        // Color green value
    unsigned char b;        // Color blue value
    unsigned char a;        // Color alpha value
} Color;

Color Fade(Color color, float alpha);

#define RED        (Color){ 230, 41, 55, 255 }
#define GRID 18

int printf();

void stuff(void) {
  Color x = Fade(RED, 1.f);

  // This is a regression test for a return buffer of only 4 bytes. Previously
  // the first local was being calculated incorrectly and happened to mostly work,
  // as long as it was 8 bytes large. This struct return of only 4 bytes was
  // writing to rbp-4 rather, overwriting the saved stack frame.
}

int main(void) {
  stuff();
  return 0;
}
'''

# This is not really an "update" test, but because the update tests happen to
# build a separate host binary rather than using the standard dyibicc.exe, it
# gives us a way to have C code to call that's test-specific and compiled by
# the host compiler, rather than by ours.

add_to_host(HOST);
add_host_helper_func("Fade")

initial({'main.c': SRC})
update_ok()
expect(0)

done()
