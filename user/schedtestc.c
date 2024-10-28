#include "kernel/types.h"
#include "user/user.h"
#include "kernel/syscall.h"

#define N  1000

void
test1(void)
{
    int pid1, pid2;
    uint64 i;
    uint runtime1, runtime2;
    
    printf("test1\n");

    // first child process
    pid1 = fork();
    if(pid1 == 0){
        stride(getpid(), 4); // child 1 with stride 4
        for(i = 0; i < N; i++){} // cpu burst
        runtime1 = getruntime(pid1);
        printf("Child 1 finished with runtime: %d\n", runtime1);
        exit(0);
    }
    // second child process
    pid2 = fork();
    if(pid2 == 0){
        stride(getpid(), 12); // child 2 with stride 12
        for(i = 0; i < N; i++){}
        runtime2 = getruntime(pid2);
        printf("Child 2 finished with runtime: %d\n", runtime2);
        exit(0);
    }

    if(pid1 > 0 && pid2 > 0){ // ensures that we are in the parent process to avoid 
                              // unnecessary waiting calls by the child processes
        wait(0); // wait for child 1
        wait(0); // wait for child 2
        printf("test1() completed\n");
    }
}

void
test2(void)
{
    int pid1, pid2;
    uint64 i;
    uint runtime1, runtime2;
    
    printf("test2\n");

    // first child process
    pid1 = fork();
    if(pid1 == 0){
        stride(getpid(), 4); // child 1 with stride 4
        for(i = 0; i < N; i++){} // cpu burst
        runtime1 = getruntime(pid1);
        printf("Child 1 finished with runtime: %d\n", runtime1);
        exit(0);
    }
    // second child process
    pid2 = fork();
    if(pid2 == 0){
        stride(getpid(), 12); // child 2 with stride 12
        for(i = 0; i < N; i++){
            sleep(1); // put the process into block state
        }
        runtime2 = getruntime(pid2);
        printf("Child 2 finished with runtime: %d\n", runtime2);
        exit(0);
    }

    if(pid1 > 0 && pid2 > 0){ // ensures that we are in the parent process to avoid 
                              // unnecessary waiting calls by the child processes
        wait(0); // wait for child 1
        wait(0); // wait for child 2
        printf("test2() completed\n");
    }
}

int 
main(void) {
    // test1();
    // test2();
    // exit(0);
    return 0;
}