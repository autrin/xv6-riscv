#include "user/user.h"
#include "kernel/types.h"

#define N  100
/*
Use fork to create two new child processes and set their stride values to 4 and 12. Both processes
then enter long CPU-bursts that lasts several ticks, for example, spinning in a long loop.
We would expect the child with stride of 4 to get about 3 times as much runtime. Call the
getruntime() system call for each process and print the results to verify this.
 */
void
fork_stride_test(void)
{
    int pid1, pid2, i;
    int runtime1, runtime2;
    
    printf("fork_stride_test\n");

    // first child process
    pid1 = fork();
    if(pid1 == 0){
        stride(getpid(), 4); // child 1 with stride 4
        for(i = 0; i < N; i++){} // cpu burst
        runtime1 = getruntime(pid1);
        printf("Child 1 finished with runtime: %d", runtime1);
        exit(0);
    }
    // second child process
    pid2 = fork();
    if(pid2 == 0){
        stride(getpid(), 12); // child 2 with stride 12
        for(i = 0; i < N; i++){}
        runtime2 = getruntime(pid2);
        printf("Child 2 finished with runtime: %d", runtime2);
        exit(0);
    }

    if(pid1 > 0 && pid2 > 0){ // ensures that we are in the parent process to avoid 
                              // unnecessary waiting calls by the child processes
        wait(0); // wait for child 1
        wait(0); // wait for child 2
        printf("fork_stride_test() completed\n");
    }
}