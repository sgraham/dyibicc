#include <sys/wait.h>

pid_t MYwait(int *wstatus) {
  return wait(wstatus);
}
