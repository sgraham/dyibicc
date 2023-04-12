// RUN: -Itest test/common.c {self}
// RET: 0
#include "test.h"

int main() {
  ASSERT(35, (float)(char)35);
  ASSERT(35, (float)(short)35);
  ASSERT(35, (float)(int)35);
  ASSERT(35, (float)(long)35);
  ASSERT(35, (float)(unsigned char)35);
  ASSERT(35, (float)(unsigned short)35);
  ASSERT(35, (float)(unsigned int)35);
  ASSERT(35, (float)(unsigned long)35);

  ASSERT(35, (double)(char)35);
  ASSERT(35, (double)(short)35);
  ASSERT(35, (double)(int)35);
  ASSERT(35, (double)(long)35);
  ASSERT(35, (double)(unsigned char)35);
  ASSERT(35, (double)(unsigned short)35);
  ASSERT(35, (double)(unsigned int)35);
  ASSERT(35, (double)(unsigned long)35);

  ASSERT(35, (char)(float)35);
  ASSERT(35, (short)(float)35);
  ASSERT(35, (int)(float)35);
  ASSERT(35, (long)(float)35);
  ASSERT(35, (unsigned char)(float)35);
  ASSERT(35, (unsigned short)(float)35);
  ASSERT(35, (unsigned int)(float)35);
  ASSERT(35, (unsigned long)(float)35);

  ASSERT(35, (char)(double)35);
  ASSERT(35, (short)(double)35);
  ASSERT(35, (int)(double)35);
  ASSERT(35, (long)(double)35);
  ASSERT(35, (unsigned char)(double)35);
  ASSERT(35, (unsigned short)(double)35);
  ASSERT(35, (unsigned int)(double)35);
  ASSERT(35, (unsigned long)(double)35);

  ASSERT(-2147483648, (double)(unsigned long)(long)-1);

  ASSERT(14, (signed char)(long double)14);
  ASSERT(14, (unsigned char)(long double)14);

  ASSERT(14, (signed short)(long double)14);
  ASSERT(14, (unsigned short)(long double)14);
  ASSERT(414, (signed short)(long double)414);
  ASSERT(414, (unsigned short)(long double)414);

  ASSERT(14, (signed int)(long double)14);
  ASSERT(14, (unsigned int)(long double)14);
  ASSERT(414, (signed int)(long double)414);
  ASSERT(414, (unsigned int)(long double)414);
  ASSERT(555414, (signed int)(long double)555414);
  ASSERT(555414, (unsigned int)(long double)555414);

  ASSERT(14, (signed long long)(long double)14);
  ASSERT(14, (unsigned long long)(long double)14);
  ASSERT(414, (signed long long)(long double)414);
  ASSERT(414, (unsigned long long)(long double)414);
  ASSERT(555414, (signed long long)(long double)555414);
  ASSERT(555414, (unsigned long long)(long double)555414);
  ASSERT(123456789555414ull, (signed long long)(long double)123456789555414);
  ASSERT(123456789555414ull, (unsigned long long)(long double)123456789555414);

  ASSERT(-13, (signed char)(long double)-13);
  ASSERT(243, (unsigned char)(long double)-13);

  ASSERT(-13, (signed short)(long double)-13);
  ASSERT(65523, (unsigned short)(long double)-13);
  ASSERT(-413, (signed short)(long double)-413);
  ASSERT(65123, (unsigned short)(long double)-413);

  ASSERT(-13, (signed int)(long double)-13);
  ASSERT((unsigned int)-13, (unsigned int)(long double)-13);
  ASSERT(-413, (signed int)(long double)-413);
  ASSERT((unsigned int)-413, (unsigned int)(long double)-413);
  ASSERT(-555413, (signed int)(long double)-555413);
  ASSERT((unsigned int)-555413, (unsigned int)(long double)-555413);

  ASSERT(-13, (signed long long)(long double)-13);
  ASSERT((unsigned long long)-13, (unsigned long long)(long double)-13);
  ASSERT(-413, (signed long long)(long double)-413);
  ASSERT((unsigned long long)-413, (unsigned long long)(long double)-413);
  ASSERT(-555413, (signed long long)(long double)-555413);
  ASSERT((unsigned long long)-555413, (unsigned long long)(long double)-555413);
  ASSERT(-123456789555413ull, (signed long long)(long double)-123456789555413);
  ASSERT((unsigned long long)-123456789555413ull,
         (unsigned long long)(long double)-123456789555413);

  ASSERT(1, 2e3==2e3);
  ASSERT(0, 2e3==2e5);
  ASSERT(1, 2.0==2);
  ASSERT(0, 5.1<5);
  ASSERT(0, 5.0<5);
  ASSERT(1, 4.9<5);
  ASSERT(0, 5.1<=5);
  ASSERT(1, 5.0<=5);
  ASSERT(1, 4.9<=5);

  ASSERT(1, 2e3f==2e3);
  ASSERT(0, 2e3f==2e5);
  ASSERT(1, 2.0f==2);
  ASSERT(0, 5.1f<5);
  ASSERT(0, 5.0f<5);
  ASSERT(1, 4.9f<5);
  ASSERT(0, 5.1f<=5);
  ASSERT(1, 5.0f<=5);
  ASSERT(1, 4.9f<=5);

  ASSERT(6, 2.3+3.8);
  ASSERT(-1, 2.3-3.8);
  ASSERT(-3, -3.8);
  ASSERT(13, 3.3*4);
  ASSERT(2, 5.0/2);

  ASSERT(6, 2.3f+3.8f);
  ASSERT(6, 2.3f+3.8);
  ASSERT(-1, 2.3f-3.8);
  ASSERT(-3, -3.8f);
  ASSERT(13, 3.3f*4);
  ASSERT(2, 5.0f/2);

  ASSERT(0, 0.0/0.0 == 0.0/0.0);
  ASSERT(1, 0.0/0.0 != 0.0/0.0);

  ASSERT(0, 0.0/0.0 < 0);
  ASSERT(0, 0.0/0.0 <= 0);
  ASSERT(0, 0.0/0.0 > 0);
  ASSERT(0, 0.0/0.0 >= 0);

  ASSERT(0, !3.);
  ASSERT(1, !0.);
  ASSERT(0, !3.f);
  ASSERT(1, !0.f);

  ASSERT(5, 0.0 ? 3 : 5);
  ASSERT(3, 1.2 ? 3 : 5);

  printf("OK\n");
  return 0;
}
