This project implements a custom scheduling system for the XV6 operating system, featuring both stride scheduling and round-robin (RR) scheduling. I implemented a FIFO queue for the schedulers.


Files modified:
    syscall.c
    syscall.h
    sysproc.c
    trap.c
    proc.c
    proc.h
    param.h
    defs.h
    main.c
    schedtestc.c
    Makefile
    usys.pl
    user.h

Screenshot of the runtime with different stride values:
![alt text](runtime-stride.png)

I have a lot of tests using printf() in the files. All of them worked. You are welcome to uncomment them and test if you want. You can search "printf" to see my tests.

I also implemented a test_enqueue(void) that prints the queue table and the proc[]. This was very helpful.
