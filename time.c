#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"


int main(int argc, char *argv[])
{
  int pid = fork();
  if(pid == 0) {
    // sleep(10);
    exec(argv[1], argv + 1);
  } else {
    int r, w;
    waitx(&r, &w);
    printf(1, "\nProcess completed\n runtime:%d\n waitime:%d\n", r, w);
  }
  exit();
}
