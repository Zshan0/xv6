#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"


int main(int argc, char *argv[])
{
  if(argc < 2) {
    printf(1, "invalid number of arguments\n");
    exit();
  }
  int ret = set_priority(atoi(argv[1]), atoi(argv[2]));
  printf(1, "Old Priority %d \n", ret);
  exit();
}
