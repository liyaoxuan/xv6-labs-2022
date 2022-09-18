#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    int p[2][2];
    pipe(p[0]);
    pipe(p[1]);

    int pid = fork();
    if (pid == 0) {// child process
        close(p[0][1]);
        close(p[1][0]);
        int mypid = getpid();
        void * buf = malloc(1);
        read(p[0][0], buf, 1);
        printf("%d: received ping\n", mypid);
        write(p[1][1], buf, 1);
        free(buf);
    } else {

        close(p[0][0]);
        close(p[1][1]);
        int mypid = getpid();
        void * buf = malloc(1);
        write(p[0][1], (void *)'a', 1);
        read(p[1][0], buf, 1);
        printf("%d: received pong\n", mypid);
        free(buf);
    }
    exit(0);
}