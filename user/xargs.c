#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"
#define MAXLEN 256


void parseArgLine(char *line, char *newArgv[], int idx_argv) {
    char *p;
    char *tmpStart;
    int count;
    count = 0;
    for(p = line; *p != '\0';) {
        while (*p != '\0' && *p == ' ')
            p++;
        if (*p == '\0')
            break;
        tmpStart = p;
        while (*p != '\0' && *p != ' ')
            p++;
        if (*p == ' ') {
            *p = '\0'; //end this argv at first space
            p++;
        }
        if (idx_argv + count >= MAXARG) {
            fprintf(2, "xargs: too many arguments\n");
            exit(1);
        }
        newArgv[idx_argv+(count++)] = tmpStart;
    }
}


int
main(int argc, char *argv[]) {
    char buf[MAXLEN] = {0};
    char *newArgv[MAXARG] = {0};
    int idx_argv, i, pid;
    for(idx_argv = 0; idx_argv < argc-1; idx_argv++) {
        newArgv[idx_argv] = argv[idx_argv+1];
    }
    i = 0;
    while(read(0, &buf[i++], sizeof(char))) {
        if (buf[i-1] == '\n') { // end of a line
            buf[i-1] = '\0';
            parseArgLine(buf, newArgv, idx_argv);
            pid = fork();
            if (pid == 0) {
                exec(argv[1], newArgv);
            } else {
                wait(0);
                memset(buf, 0, MAXLEN);
                for (i = idx_argv; i < MAXARG; ++i)
                    newArgv[i] = 0;
                i = 0;
            }
        }
    }
    exit(0);
}