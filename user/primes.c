#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void prime(int);

int createNeighbor(int p[]) {
    int pid;
    pid = fork();
    if (pid) {
        close(p[0]);
        return pid;
    } else {
        close(p[1]);
        prime(p[0]);
    }
    return 0;
}

void prime(int lRead) {
    int baseVal, readVal;
    int neighborExist = 0;
    int rpipe[2];
    read(lRead, &baseVal, sizeof(int));
    printf("prime %d\n", baseVal);
    while (read(lRead, &readVal, sizeof(int))) {
        if (readVal % baseVal != 0) {
            if (!neighborExist) {
                pipe(rpipe);
                int pid = createNeighbor(rpipe);
                neighborExist = 1;
                if (pid == 0) {
                    break;
                }
            }
            write(rpipe[1], (void *)&readVal, sizeof(int));
        }
    }
    close(rpipe[1]);
    wait(0);
}
int
main(int argc, char *argv[]) {
    int i;
    int p[2];
    int pid;

    pipe(p);
    pid = fork();
    if (pid == 0) {
        close(p[1]);
        prime(p[0]);
    } else {
        close(p[0]);
        printf("prime %d\n", 2);
        for (i = 3; i <= 35; i += 2)
            write(p[1], (void *)&i, sizeof(int));
        close(p[1]);
        wait(0);
    }
    exit(0);
}