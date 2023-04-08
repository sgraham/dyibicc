#include "test.h"

typedef struct StRegSized {
    unsigned char r;
} StRegSized;


void func1(int a, int b, int c, int d, StRegSized st) {
  ASSERT(1, a);
  ASSERT(2, b);
  ASSERT(3, c);
  ASSERT(4, d);
  ASSERT(99, st.r);
}

void func2(int a, int b, int c, int d, int e, StRegSized st) {
  ASSERT(1, a);
  ASSERT(2, b);
  ASSERT(3, c);
  ASSERT(4, d);
  ASSERT(5, e);
  ASSERT(99, st.r);
}

void func3(int a, int b, int c, int d, int e, int f, StRegSized st) {
  ASSERT(1, a);
  ASSERT(2, b);
  ASSERT(3, c);
  ASSERT(4, d);
  ASSERT(5, e);
  ASSERT(6, f);
  ASSERT(99, st.r);
}

void func4(int a, int b, int c, int d, int e, int f, int g, StRegSized st) {
  ASSERT(1, a);
  ASSERT(2, b);
  ASSERT(3, c);
  ASSERT(4, d);
  ASSERT(5, e);
  ASSERT(6, f);
  ASSERT(7, g);
  ASSERT(99, st.r);
}

void func5(int a, int b, int c, StRegSized st) {
  ASSERT(1, a);
  ASSERT(2, b);
  ASSERT(3, c);
  ASSERT(99, st.r);
}

void func6(int a, int b, StRegSized st) {
  ASSERT(1, a);
  ASSERT(2, b);
  ASSERT(99, st.r);
}

void func7(int a, StRegSized st) {
  ASSERT(1, a);
  ASSERT(99, st.r);
}

void func8(int a, int b, int c, StRegSized st, int d) {
  ASSERT(1, a);
  ASSERT(2, b);
  ASSERT(3, c);
  ASSERT(99, st.r);
  ASSERT(4, d);
}

void func9(int a, int b, StRegSized st, int c, int d) {
  ASSERT(1, a);
  ASSERT(2, b);
  ASSERT(99, st.r);
  ASSERT(3, c);
  ASSERT(4, d);
}

void funca(int a, StRegSized st, int b, int c, int d) {
  ASSERT(1, a);
  ASSERT(99, st.r);
  ASSERT(2, b);
  ASSERT(3, c);
  ASSERT(4, d);
}

void funcb(int a, StRegSized st, int b, StRegSized st2, int c) {
  ASSERT(1, a);
  ASSERT(99, st.r);
  ASSERT(2, b);
  ASSERT(98, st2.r);
  ASSERT(3, c);
}

void funcc(int a, StRegSized st, int b, StRegSized st2, StRegSized st3) {
  ASSERT(1, a);
  ASSERT(99, st.r);
  ASSERT(2, b);
  ASSERT(98, st2.r);
  ASSERT(97, st3.r);
}

void funcd(int a, StRegSized st, int b, StRegSized st2, StRegSized st3, int c) {
  ASSERT(1, a);
  ASSERT(99, st.r);
  ASSERT(2, b);
  ASSERT(98, st2.r);
  ASSERT(97, st3.r);
  ASSERT(3, c);
}


int main(void) {
  StRegSized x = {99};
  StRegSized y = {98};
  StRegSized z = {97};
  func1(1, 2, 3, 4, x);
  func2(1, 2, 3, 4, 5, x);
  func3(1, 2, 3, 4, 5, 6, x);
  func4(1, 2, 3, 4, 5, 6, 7, x);
  func5(1, 2, 3, x);
  func6(1, 2, x);
  func7(1, x);
  func8(1, 2, 3, x, 4);
  func9(1, 2, x, 3, 4);
  funca(1, x, 2, 3, 4);
  funcb(1, x, 2, y, 3);
  funcc(1, x, 2, y, z);
  funcd(1, x, 2, y, z, 3);
}
