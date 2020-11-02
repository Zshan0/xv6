
#include "types.h"
#include "user.h"

int number_of_processes = 10;

int main(int argc, char *argv[]) {
    //int parent_pid = getpid();
    for (int proc_num = 0; proc_num < number_of_processes; proc_num++) {
        int pid = fork();
        if (pid < 0) {
            printf(1, "Fork failed\n");
            continue;
        }
        if (pid == 0) {
            int total = 0;
            if (proc_num % 3 == 0) {
                // CPU
                for (int i = 0; i < 1e9; i++) {
                    total += i;
                }
            }
            if (proc_num % 3 == 1) {
                // IO
                for(int i = 0; i < 10; i++){
                    sleep(70);
                    total += i;
                }
            }
            if (proc_num % 3 == 2) {
                // IO and CPU
                sleep(500);
                for(int i = 0; i < 5; i++){
                    total += i;
                    for(int j = 0; j < 1e8; j++){
                        total += j;
                    }
                }
            }
            printf(1, "Benchmark: %d Exited\n", proc_num);
            exit();
        } else {
            set_priority(100 - (20 + proc_num) % 2,
                         pid); // will only matter for PBS, comment it out if not implemented yet (better priorty for more IO intensive jobs)
        }
    }

    for (int j = 0; j < number_of_processes + 5; j++) {
        wait();
    }
    exit();
}
