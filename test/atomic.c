#include "test.h"
#include <stdatomic.h>
#include <stddef.h>
#ifdef _WIN64
// Not quite ready to brave windows.h yet.
void* CreateThread(void* lpThreadAttributes,
                   size_t dwStackSize,
                   void* lpStartAddress,
                   void* lpParameter,
                   unsigned int dwCreationFlags,
                   unsigned int* dwThreadId);
unsigned int WaitForSingleObject(void* hHandle, unsigned int dwMilliseconds);
unsigned int CloseHandle(void* hObject);
#define INFINITE            0xFFFFFFFF
#else
#include <pthread.h>
#endif

static int incr(_Atomic int *p) {
  int oldval = *p;
  int newval;
  do {
    newval = oldval + 1;
  } while (!atomic_compare_exchange_weak(p, &oldval, newval));
  return newval;
}

static int add1(void *arg) {
  _Atomic int *x = arg;
  for (int i = 0; i < 1000*1000; i++)
    incr(x);
  return 0;
}

static int add2(void *arg) {
  _Atomic int *x = arg;
  for (int i = 0; i < 1000*1000; i++)
    (*x)++;
  return 0;
}

static int add3(void *arg) {
  _Atomic int *x = arg;
  for (int i = 0; i < 1000*1000; i++)
    *x += 5;
  return 0;
}

static int add_millions(void) {
  _Atomic int x = 0;

#ifdef _WIN64
  void* thr1 = CreateThread(NULL, 0, add1, &x, 0, NULL);
  void* thr2 = CreateThread(NULL, 0, add2, &x, 0, NULL);
  void* thr3 = CreateThread(NULL, 0, add3, &x, 0, NULL);
#else
  pthread_t thr1;
  pthread_t thr2;
  pthread_t thr3;

  pthread_create(&thr1, NULL, add1, &x);
  pthread_create(&thr2, NULL, add2, &x);
  pthread_create(&thr3, NULL, add3, &x);
#endif

  for (int i = 0; i < 1000*1000; i++)
    x--;

#ifdef _WIN64
  WaitForSingleObject(thr1, INFINITE);
  WaitForSingleObject(thr2, INFINITE);
  WaitForSingleObject(thr3, INFINITE);
  CloseHandle(thr1);
  CloseHandle(thr2);
  CloseHandle(thr3);
#else
  pthread_join(thr1, NULL);
  pthread_join(thr2, NULL);
  pthread_join(thr3, NULL);
#endif
  return x;
}

int main() {
  ASSERT(6*1000*1000, add_millions());

  ASSERT(3, ({ int x=3; atomic_exchange(&x, 5); }));
  ASSERT(5, ({ int x=3; atomic_exchange(&x, 5); x; }));

  printf("OK\n");
  return 0;
}
