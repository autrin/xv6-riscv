#include "user/user.h"

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
    struct proc *p;
    printf("fork_stride_test\n");

    pid1 = fork();
    pid2 = fork();
    stride(pid1, 4);
    stride(pid2, 12);
    for(i = 0; i < N; i++){
        printf("i is: %d/n", i);
    }

    printf("The runtime of proccess %d is %d", pid1, getruntime(pid1));
    printf("The runtime of proccess %d is %d", pid2, getruntime(pid2));
    
    printf("Exiting the processes %d and %d", pid1, pid2);
    exit(pid1);
    exit(pid2);
}