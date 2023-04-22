from test_helpers_for_update import *

HOST = '''\
#include <math.h>
typedef struct TestVec2 { float x; float y; } TestVec2;
typedef struct TestCam { TestVec2 offset; TestVec2 target; float rotation; float zoom; } TestCam;

#ifndef DEG2RAD
    #define DEG2RAD (PI/180.0f)
#endif
#ifndef PI
    #define PI 3.14159265358979323846f
#endif

typedef struct TestVec3 {
    float x;                // Vector x component
    float y;                // Vector y component
    float z;                // Vector z component
} TestVec3;

typedef struct TestMat {
    float m0, m4, m8, m12;  // Matrix first row (4 components)
    float m1, m5, m9, m13;  // Matrix second row (4 components)
    float m2, m6, m10, m14; // Matrix third row (4 components)
    float m3, m7, m11, m15; // Matrix fourth row (4 components)
} TestMat;

TestMat MatrixMultiply(TestMat left, TestMat right)
{
    TestMat result = { 0 };

    result.m0 = left.m0*right.m0 + left.m1*right.m4 + left.m2*right.m8 + left.m3*right.m12;
    result.m1 = left.m0*right.m1 + left.m1*right.m5 + left.m2*right.m9 + left.m3*right.m13;
    result.m2 = left.m0*right.m2 + left.m1*right.m6 + left.m2*right.m10 + left.m3*right.m14;
    result.m3 = left.m0*right.m3 + left.m1*right.m7 + left.m2*right.m11 + left.m3*right.m15;
    result.m4 = left.m4*right.m0 + left.m5*right.m4 + left.m6*right.m8 + left.m7*right.m12;
    result.m5 = left.m4*right.m1 + left.m5*right.m5 + left.m6*right.m9 + left.m7*right.m13;
    result.m6 = left.m4*right.m2 + left.m5*right.m6 + left.m6*right.m10 + left.m7*right.m14;
    result.m7 = left.m4*right.m3 + left.m5*right.m7 + left.m6*right.m11 + left.m7*right.m15;
    result.m8 = left.m8*right.m0 + left.m9*right.m4 + left.m10*right.m8 + left.m11*right.m12;
    result.m9 = left.m8*right.m1 + left.m9*right.m5 + left.m10*right.m9 + left.m11*right.m13;
    result.m10 = left.m8*right.m2 + left.m9*right.m6 + left.m10*right.m10 + left.m11*right.m14;
    result.m11 = left.m8*right.m3 + left.m9*right.m7 + left.m10*right.m11 + left.m11*right.m15;
    result.m12 = left.m12*right.m0 + left.m13*right.m4 + left.m14*right.m8 + left.m15*right.m12;
    result.m13 = left.m12*right.m1 + left.m13*right.m5 + left.m14*right.m9 + left.m15*right.m13;
    result.m14 = left.m12*right.m2 + left.m13*right.m6 + left.m14*right.m10 + left.m15*right.m14;
    result.m15 = left.m12*right.m3 + left.m13*right.m7 + left.m14*right.m11 + left.m15*right.m15;

    return result;
}

TestMat GetCameraMatrix2D(TestCam camera)
{
    (void)camera;
    TestMat matTransform = { 0 };
    matTransform = MatrixMultiply(matTransform, matTransform);
    return matTransform;
}

TestVec3 TestVec3Transform(TestVec3 v, TestMat mat)
{
    TestVec3 result = { 0 };

    float x = v.x;
    float y = v.y;
    float z = v.z;

    result.x = mat.m0*x + mat.m4*y + mat.m8*z + mat.m12;
    result.y = mat.m1*x + mat.m5*y + mat.m9*z + mat.m13;
    result.z = mat.m2*x + mat.m6*y + mat.m10*z + mat.m14;

    return result;
}

TestVec2 GetScreenToWorld2D(TestCam camera)
{
    TestMat x = GetCameraMatrix2D(camera);

    return (TestVec2){ x.m0, x.m1 };
}

'''

SRC = '''\
#include "test.h"
#include <stdlib.h>

typedef struct TestVec2 {
  float x;
  float y;
} TestVec2;

typedef struct TestCam {
  TestVec2 offset;
  TestVec2 target;
  float rotation;
  float zoom;
} TestCam;

TestVec2 GetScreenToWorld2D(TestCam camera);

int main(void) {
  TestCam cam = {(TestVec2){0.f, 0.f}, (TestVec2){0.f, 0.f}, 0.f, 1.f};
  TestVec2 w = GetScreenToWorld2D(cam);
  if (w.x != 0.f || w.y != 0.f) abort();

  // This test was a regression test for non-stack alignment that was
  // only happening in Windows /Ox builds. It's not a great test, but
  // we hope that it'll crash if the bug manifests again.

  printf("OK\\n");
  return 0;
}
'''

# This is not really an "update" test, but because the update tests happen to
# build a separate host binary rather than using the standard dyibicc.exe, it
# gives us a way to have C code to call that's test-specific and compiled by
# the host compiler, rather than by ours.

add_to_host(HOST);
add_host_helper_func("GetScreenToWorld2D", "abort")
include_path("../../test")

initial({'main.c': SRC})
update_ok()
expect(0)

done()
