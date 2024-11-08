#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

#define MAX_UINT64 (-1)
#define EMPTY MAX_UINT64
#define default_ticket_count 100
#define large_constant 10000

// a node of the linked list
struct qentry{
  uint64 pass; // used by the stride scheduler to keep the list sorted
  uint64 prev; // index of previous qentry in list
  uint64 next; // index of next qentry in list
};

struct cpu cpus[NCPU];

struct proc proc[NPROC];

// A fixed size table where the index of a process in proc[] is the same in qtable_stride[]
// Index of head is stored at 64 and tail at 65
struct qentry qtable_stride[NPROC+2];
struct qentry qtable_rr[NPROC+2];

struct proc *initproc;

int nextpid = 1;
struct spinlock pid_lock;

extern void forkret(void);
static void freeproc(struct proc *p);

extern char trampoline[]; // trampoline.S

// helps ensure that wakeups of wait()ing
// parents are not lost. helps obey the
// memory model when using p->parent.
// must be acquired before any p->lock.
struct spinlock wait_lock;

void 
init_queue(void)
{
  // qtabe_stride initialization
  for(int i = 0; i < NPROC; i++){
    qtable_stride[i].pass = MAX_UINT64; // indicating an empty entry
    qtable_stride[i].prev = MAX_UINT64;
    qtable_stride[i].next = MAX_UINT64;
  }

  qtable_stride[NPROC].next = NPROC+1;
  qtable_stride[NPROC].prev = MAX_UINT64; // No previous entry for the head
  qtable_stride[NPROC+1].prev = NPROC;
  qtable_stride[NPROC+1].next = MAX_UINT64; // No next entry for the tail

  // qtable_rr initialization
  for(int i = 0; i < NPROC; i++){
    qtable_rr[i].pass = MAX_UINT64;
    qtable_rr[i].prev = MAX_UINT64;
    qtable_rr[i].next = MAX_UINT64;
  }

  qtable_rr[NPROC].next = NPROC+1;
  qtable_rr[NPROC].prev = MAX_UINT64;
  qtable_rr[NPROC+1].prev = NPROC;
  qtable_rr[NPROC+1].next = MAX_UINT64;
}

// Enqueue a process in qtable
void 
enqueue(int index, uint64 pass) {
  // Check if the process state is RUNNABLE before enqueueing
  if (proc[index].state != RUNNABLE) {
    printf("Enqueue Error: Process %d is not RUNNABLE, state:%d\n", proc[index].pid, proc[index].state);
    return;  // Exit function if process is not RUNNABLE
  }

  if (SCHEDULER == 2) {  // Round-robin queue
    int tail = qtable_rr[NPROC + 1].prev;

    qtable_rr[index].next = NPROC + 1;
    qtable_rr[index].prev = tail;
    qtable_rr[index].pass = pass;

    if (tail != NPROC + 1) {
      qtable_rr[tail].next = index;
    } else {
      qtable_rr[NPROC].next = index;
    }
    qtable_rr[NPROC + 1].prev = index;

    // printf("RR Enqueue: Process %d with state:%d enqueued\n", proc[index].pid, proc[index].state);
  }
  else if (SCHEDULER == 3) {  // Stride queue with sorted insertion based on pass value
    qtable_stride[index].pass = pass;  // Assign the pass value

    int current = NPROC;  // Start at the head of the queue
    int next = qtable_stride[NPROC].next;

    // Traverse the queue to find the insertion point for the new process
    while (next != NPROC + 1 && qtable_stride[next].pass <= pass) {
      current = next;
      next = qtable_stride[next].next;
    }

    // Insert the new process between 'current' and 'next'
    qtable_stride[index].next = next;
    qtable_stride[index].prev = current;
    qtable_stride[current].next = index;

    if (next != NPROC + 1) {
      qtable_stride[next].prev = index;
    } else {
      // If it's the last entry, update the tail pointer
      qtable_stride[NPROC + 1].prev = index;
    }
    // printf("Stride Enqueue: Process %d with state:%d enqueued\n", proc[index].pid, proc[index].state);
  }
  else {
    printf("Error: Unsupported scheduler type of %d in enqueue().\n", SCHEDULER);
  }
}

// Dequeue a process from qtable
int 
dequeue(void)
{
  if (SCHEDULER == 2) {  // Round-Robin queue
    int index = qtable_rr[NPROC].next;

    while (index != NPROC + 1) {
      if (proc[index].state == RUNNABLE) {
        if (qtable_rr[NPROC].next == index) {
          qtable_rr[NPROC].next = qtable_rr[index].next;
        }

        qtable_rr[qtable_rr[index].prev].next = qtable_rr[index].next;
        if (qtable_rr[index].next != NPROC + 1) {
          qtable_rr[qtable_rr[index].next].prev = qtable_rr[index].prev;
        } else {
          qtable_rr[NPROC + 1].prev = qtable_rr[index].prev;
        }

        qtable_rr[index].next = MAX_UINT64;
        qtable_rr[index].prev = MAX_UINT64;

        // printf("RR Dequeue: Process %d dequeued\n", proc[index].pid);
        return index;
      }
      index = qtable_rr[index].next;
    }
    return -1;
  }
  else if (SCHEDULER == 3) {  // Stride queue
    int index = qtable_stride[NPROC].next;

    while (index != NPROC + 1) {
      if (proc[index].state == RUNNABLE) {
        if (qtable_stride[NPROC].next == index) {
          qtable_stride[NPROC].next = qtable_stride[index].next;
        }

        qtable_stride[qtable_stride[index].prev].next = qtable_stride[index].next;
        if (qtable_stride[index].next != NPROC + 1) {
          qtable_stride[qtable_stride[index].next].prev = qtable_stride[index].prev;
        } else {
          qtable_stride[NPROC + 1].prev = qtable_stride[index].prev;
        }

        qtable_stride[index].next = MAX_UINT64;
        qtable_stride[index].prev = MAX_UINT64;

        // printf("Stride Dequeue: Process %d dequeued\n", proc[index].pid);
        return index;
      }
      index = qtable_stride[index].next;
    }
    return -1;
  }
  printf("Error: Unsupported scheduler type of %d in dequeue().\n", SCHEDULER);
  return -1;
}

void 
test_enqueue(void){
  if(SCHEDULER == 2){
    printf("Round Robin Queue state after enqueue:\n");
    for (int i = 0; i < NPROC + 2; i++) {
      printf("Index %d: pass=%lu, prev=%lu, next=%lu\n", 
              i, qtable_rr[i].pass, qtable_rr[i].prev, qtable_rr[i].next);
    }
    printf("Printing the proc[]\n");
    for (int i = 0; i < NPROC; i++) {
      if(proc[i].state == RUNNABLE){
        printf("Index %d: pid=%d\n", 
                i, proc[i].pid);
      }
    }
  }
  else if(SCHEDULER == 3){
    printf("Stride Queue state after enqueue:\n");
    for (int i = 0; i < NPROC + 2; i++) {
      printf("Index %d: pass=%lu, prev=%lu, next=%lu\n", 
              i, qtable_stride[i].pass, qtable_stride[i].prev, qtable_stride[i].next);
    }
    printf("Printing the proc[]\n");
    for (int i = 0; i < NPROC; i++) {
      if(proc[i].state == RUNNABLE){
        printf("Index %d: pid=%d\n", 
                i, proc[i].pid);
      }
    }
  }
}

// A round robin scheduler with time quanta of 2 and a stride scheduler
void scheduler_rr_stride()
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
  
  for (;;)
  {
    intr_on(); // Enable interrupts to avoid deadlocks
    // printf("We are in scheduler_rr_stride()'s for loop right after the call to intr_on()\n");
    int dequeued = dequeue(); // Dequeue the next process
    if (dequeued == -1)
    {
      // printf("dequeued was -1 in the for loop of scheduler_rr_stride()\n");
      // No process in the queue
      asm volatile("wfi"); // Wait for interrupt
      continue; // Go back to the start of the loop and wait for an interrupt
    }

    p = &proc[dequeued];
    acquire(&p->lock);

    if (p->state == RUNNABLE) {
      // Set process to RUNNING state
      p->state = RUNNING;
      c->proc = p;
      // printf("Scheduler: Switching to process %d\n", p->pid);
      swtch(&c->context, &p->context); // Context switch into the process
      // printf("The state of the dequeued process should be Running and it is: %d\n States are UNUSED, USED, SLEEPING, RUNNABLE, RUNNING, ZOMBIE\n\n", p->state);

      if (SCHEDULER == 3) {
        qtable_stride[p - proc].pass += p->stride; // Increment pass for stride
      }
      if(SCHEDULER == 2 && p->state == RUNNABLE) {
        enqueue(p-proc, 0);  // Re-enqueue if still runnable
      }
    }
    else {
      printf("Warning: Process %d dequeued but not RUNNABLE.\n", p->pid);
    }
    c->proc = 0; // Reset the CPU's proc
    release(&p->lock);
  }
}

// Find the lowest pass for the process. Used for the stride scheduler
uint64
find_lowest_pass(void){
  uint64 lowest_pass = MAX_UINT64;
  for(int i = 0; i < NPROC; i++){
    if(qtable_stride[i].pass != MAX_UINT64 && qtable_stride[i].pass < lowest_pass){
      lowest_pass = qtable_stride[i].pass;
    }
  }
  return lowest_pass;
}

// Set the stride value
uint64
stride(int pid, int stride_value)
{
    struct proc *p;
    for(p = proc; p < &proc[NPROC]; p++){
        if(p->pid == pid){
            acquire(&p->lock); // Lock the process before modifying
            p->stride = stride_value; // Assign the correct stride value
            release(&p->lock); // Release the lock
            return 0;
        }
    }
    printf("Process with pid %d not found in stride()\n", pid); // TODO: comment out
    return -1; // Return -1 if process is not found
}

// Allocate a page for each process's kernel stack.
// Map it high in memory, followed by an invalid
// guard page.
void
proc_mapstacks(pagetable_t kpgtbl)
{
  struct proc *p;
  
  for(p = proc; p < &proc[NPROC]; p++) {
    char *pa = kalloc();
    if(pa == 0)
      panic("kalloc");
    uint64 va = KSTACK((int) (p - proc));
    kvmmap(kpgtbl, va, (uint64)pa, PGSIZE, PTE_R | PTE_W);
  }
}

// initialize the proc table.
void
procinit(void)
{
  struct proc *p;
  
  initlock(&pid_lock, "nextpid");
  initlock(&wait_lock, "wait_lock");
  for(p = proc; p < &proc[NPROC]; p++) {
      initlock(&p->lock, "proc");
      p->state = UNUSED;
      p->kstack = KSTACK((int) (p - proc));
      p->runtime = 0; // initialize runtime to 0
      p->ticks_used = 0;
      // printf("The process with pid %d is at index %lu in proc[]\n", p->pid, p - proc);
  }
}

// Must be called with interrupts disabled,
// to prevent race with process being moved
// to a different CPU.
int
cpuid()
{
  int id = r_tp();
  return id;
}

// Return this CPU's cpu struct.
// Interrupts must be disabled.
struct cpu*
mycpu(void)
{
  int id = cpuid();
  struct cpu *c = &cpus[id];
  return c;
}

// Return the current struct proc *, or zero if none.
struct proc*
myproc(void)
{
  push_off();
  struct cpu *c = mycpu();
  struct proc *p = c->proc;
  pop_off();
  return p;
}

int
allocpid()
{
  int pid;
  
  acquire(&pid_lock);
  pid = nextpid;
  nextpid = nextpid + 1;
  release(&pid_lock);

  return pid;
}

// Look in the process table for an UNUSED proc.
// If found, initialize state required to run in the kernel,
// and return with p->lock held.
// If there are no free procs, or a memory allocation fails, return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if(p->state == UNUSED) {
      goto found;
    } else {
      release(&p->lock);
    }
  }
  return 0;

found:
  p->pid = allocpid();
  p->state = USED;

  // Allocate a trapframe page.
  if((p->trapframe = (struct trapframe *)kalloc()) == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  // An empty user page table.
  p->pagetable = proc_pagetable(p);
  if(p->pagetable == 0){
    freeproc(p); 
    release(&p->lock);
    return 0;
  }

  // Set up new context to start executing at forkret,
  // which returns to user space.
  memset(&p->context, 0, sizeof(p->context));
  p->context.ra = (uint64)forkret;
  p->context.sp = p->kstack + PGSIZE;

  if(SCHEDULER == 3) { // Only set stride if in Stride Scheduler mode
    // acquire(&p->lock);
    uint64 lowest_pass = find_lowest_pass();  // Find the lowest pass value to maintain queue order
    p->tickets = default_ticket_count;        // Initialize tickets
    p->stride = !p->stride ? (large_constant / p->tickets) : p->stride; // Calculate stride if unset
    // printf("The stride of processes with pid %d is: %d in allocproc()\n",p->pid, p->stride);
    qtable_stride[p - proc].pass = lowest_pass + p->stride;

    // release(&p->lock);
  }
  // printf("The pid of the process returning in allocproc() is %d\n", p->pid);
  // printf("Printing the proc[]\n");
  for (int i = 0; i < NPROC; i++) {
    if(proc[i].state == RUNNABLE){ // Testing
      // printf("Index %d: pid=%d\n", 
      //         i, proc[i].pid);
    }
  }
  return p;
}

// free a proc structure and the data hanging from it,
// including user pages.
// p->lock must be held.
static void
freeproc(struct proc *p)
{
  if(p->trapframe)
    kfree((void*)p->trapframe);
  p->trapframe = 0;
  if(p->pagetable)
    proc_freepagetable(p->pagetable, p->sz);
  p->pagetable = 0;
  p->sz = 0;
  p->pid = 0;
  p->parent = 0;
  p->name[0] = 0;
  p->chan = 0;
  p->killed = 0;
  p->xstate = 0;
  p->state = UNUSED;
  p->stride = 1;
  p->runtime = 0;
  p->ticks_used = 0;
  p->tickets = 0;
}

// Create a user page table for a given process, with no user memory,
// but with trampoline and trapframe pages.
pagetable_t
proc_pagetable(struct proc *p)
{
  pagetable_t pagetable;

  // An empty page table.
  pagetable = uvmcreate();
  if(pagetable == 0)
    return 0;

  // map the trampoline code (for system call return)
  // at the highest user virtual address.
  // only the supervisor uses it, on the way
  // to/from user space, so not PTE_U.
  if(mappages(pagetable, TRAMPOLINE, PGSIZE,
              (uint64)trampoline, PTE_R | PTE_X) < 0){
    uvmfree(pagetable, 0);
    return 0;
  }

  // map the trapframe page just below the trampoline page, for
  // trampoline.S.
  if(mappages(pagetable, TRAPFRAME, PGSIZE,
              (uint64)(p->trapframe), PTE_R | PTE_W) < 0){
    uvmunmap(pagetable, TRAMPOLINE, 1, 0);
    uvmfree(pagetable, 0);
    return 0;
  }

  return pagetable;
}

// Free a process's page table, and free the
// physical memory it refers to.
void
proc_freepagetable(pagetable_t pagetable, uint64 sz)
{
  uvmunmap(pagetable, TRAMPOLINE, 1, 0);
  uvmunmap(pagetable, TRAPFRAME, 1, 0);
  uvmfree(pagetable, sz);
}

// a user program that calls exec("/init")
// assembled from ../user/initcode.S
// od -t xC ../user/initcode
uchar initcode[] = {
  0x17, 0x05, 0x00, 0x00, 0x13, 0x05, 0x45, 0x02,
  0x97, 0x05, 0x00, 0x00, 0x93, 0x85, 0x35, 0x02,
  0x93, 0x08, 0x70, 0x00, 0x73, 0x00, 0x00, 0x00,
  0x93, 0x08, 0x20, 0x00, 0x73, 0x00, 0x00, 0x00,
  0xef, 0xf0, 0x9f, 0xff, 0x2f, 0x69, 0x6e, 0x69,
  0x74, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00
};

// Set up first user process.
void
userinit(void)
{
  printf("Creating init process\n");

  struct proc *p;

  p = allocproc();
  initproc = p;
  // printf("Returned from allocproc to userinit()\n");
  
  // allocate one user page and copy initcode's instructions
  // and data into it.
  uvmfirst(p->pagetable, initcode, sizeof(initcode));
  p->sz = PGSIZE;

  // prepare for the very first "return" from kernel to user.
  p->trapframe->epc = 0;      // user program counter
  p->trapframe->sp = PGSIZE;  // user stack pointer

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  p->state = RUNNABLE;
  if(SCHEDULER == 2 || SCHEDULER == 3){
    enqueue(p-proc, (SCHEDULER == 3) ? qtable_stride[p-proc].pass : 0); // Use pass only for stride scheduler
    // printf("Enqueued init process (pid=%d) in userinit\n", p->pid);
  }
  release(&p->lock);
}

// Grow or shrink user memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint64 sz;
  struct proc *p = myproc();

  sz = p->sz;
  if(n > 0){
    if((sz = uvmalloc(p->pagetable, sz, sz + n, PTE_W)) == 0) {
      return -1;
    }
  } else if(n < 0){
    sz = uvmdealloc(p->pagetable, sz, sz + n);
  }
  p->sz = sz;
  return 0;
}

// Create a new process, copying the parent.
// Sets up child kernel stack to return as if from fork() system call.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *p = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy user memory from parent to child.
  if(uvmcopy(p->pagetable, np->pagetable, p->sz) < 0){
    freeproc(np);
    release(&np->lock);
    return -1;
  }
  np->sz = p->sz;

  // copy saved user registers.
  *(np->trapframe) = *(p->trapframe);

  // Cause fork to return 0 in the child.
  np->trapframe->a0 = 0;

  // increment reference counts on open file descriptors.
  for(i = 0; i < NOFILE; i++)
    if(p->ofile[i])
      np->ofile[i] = filedup(p->ofile[i]);
  np->cwd = idup(p->cwd);

  safestrcpy(np->name, p->name, sizeof(p->name));

  pid = np->pid;

  release(&np->lock);

  acquire(&wait_lock);
  np->parent = p;
  release(&wait_lock);

  acquire(&np->lock);
  np->state = RUNNABLE;
  if(SCHEDULER == 2 || SCHEDULER == 3){
    enqueue(np-proc, (SCHEDULER == 3) ? qtable_stride[np-proc].pass : 0);
    // printf("Enqueued child process (pid=%d) from parent (pid=%d)\n", np->pid, p->pid);
  }
  release(&np->lock);

  return pid;
}

// Pass p's abandoned children to init.
// Caller must hold wait_lock.
void
reparent(struct proc *p)
{
  struct proc *pp;

  for(pp = proc; pp < &proc[NPROC]; pp++){
    if(pp->parent == p){
      pp->parent = initproc;
      wakeup(initproc);
    }
  }
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait().
void
exit(int status)
{
  struct proc *p = myproc();

  if(p == initproc)
    panic("init exiting");

  // Close all open files.
  for(int fd = 0; fd < NOFILE; fd++){
    if(p->ofile[fd]){
      struct file *f = p->ofile[fd];
      fileclose(f);
      p->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(p->cwd);
  end_op();
  p->cwd = 0;

  acquire(&wait_lock);

  // Give any children to init.
  reparent(p);

  // Parent might be sleeping in wait().
  wakeup(p->parent);
  
  acquire(&p->lock);

  p->xstate = status;
  p->state = ZOMBIE;

  release(&wait_lock);

  // Jump into the scheduler, never to return.
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(uint64 addr)
{
  struct proc *pp;
  int havekids, pid;
  struct proc *p = myproc();

  acquire(&wait_lock);

  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(pp = proc; pp < &proc[NPROC]; pp++){
      if(pp->parent == p){
        // make sure the child isn't still in exit() or swtch().
        acquire(&pp->lock);

        havekids = 1;
        if(pp->state == ZOMBIE){
          // printf("pp was zombie!!!!! in wait()");
          // Found one.
          pid = pp->pid;
          if(addr != 0 && copyout(p->pagetable, addr, (char *)&pp->xstate,
                                  sizeof(pp->xstate)) < 0) {
            release(&pp->lock);
            release(&wait_lock);
            return -1;
          }
          freeproc(pp);
          release(&pp->lock);
          release(&wait_lock);
          return pid;
        }
        release(&pp->lock);
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || killed(p)){
      release(&wait_lock);
      return -1;
    }
    
    // Wait for a child to exit.
    sleep(p, &wait_lock);  //DOC: wait-sleep
  }
}

// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run.
//  - swtch to start running that process.
//  - eventually that process transfers control
//    via swtch back to the scheduler.
void
scheduler(void)
{
  // if(SCHEDULER == 2 || SCHEDULER == 3){
  //     printf("We went inside of the if statment in the beginning of scheduler()\n");
  //     scheduler_rr_stride();
  //     printf("We are after the scheduler_rr_stride() call\n");
  // }
  struct proc *p;
  struct cpu *c = mycpu();

  c->proc = 0;
  for(;;){
    // The most recent process to run may have had interrupts
    // turned off; enable them to avoid a deadlock if all
    // processes are waiting.
    intr_on();

    int found = 0;
    for(p = proc; p < &proc[NPROC]; p++) {
      acquire(&p->lock);
      if(p->state == RUNNABLE) {
        // Switch to chosen process.  It is the process's job
        // to release its lock and then reacquire it
        // before jumping back to us.
        p->state = RUNNING;
        c->proc = p;
        // printf("Switching to process %d\n", p->pid);
        swtch(&c->context, &p->context);

        // Process is done running for now.
        // It should have changed its p->state before coming back.
        c->proc = 0;
        found = 1;
      }
      release(&p->lock);
    }
    if(found == 0) {
      // nothing to run; stop running on this core until an interrupt.
      intr_on();
      asm volatile("wfi");
    }
  }
}

// Switch to scheduler.  Must hold only p->lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->noff, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&p->lock))
    panic("sched p->lock");
  if(mycpu()->noff != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(intr_get())
    panic("sched interruptible");

  intena = mycpu()->intena;
  swtch(&p->context, &mycpu()->context);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  struct proc *p = myproc();
  acquire(&p->lock);
  p->state = RUNNABLE;
  if(SCHEDULER == 2) // For Round Robin scheduler
    p->ticks_used = 0; // Reset ticks before re-enqueuing the process
  if(SCHEDULER == 2 || SCHEDULER == 3){
    // Add the process back to the queue based on the current scheduler
    enqueue(p-proc, (SCHEDULER == 3) ? qtable_stride[p-proc].pass : 0); // Use pass only for stride scheduler
  }
  sched(); // Call the scheduler to pick the next process
  release(&p->lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch to forkret.
void
forkret(void)
{
  static int first = 1;

  // Still holding p->lock from scheduler.
  release(&myproc()->lock);

  if (first) {
    // File system initialization must be run in the context of a
    // regular process (e.g., because it calls sleep), and thus cannot
    // be run from main().
    fsinit(ROOTDEV);

    first = 0;
    // ensure other cores see first=0.
    __sync_synchronize();
  }

  usertrapret();
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  // Must acquire p->lock in order to
  // change p->state and then call sched.
  // Once we hold p->lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup locks p->lock),
  // so it's okay to release lk.

  acquire(&p->lock);  //DOC: sleeplock1
  release(lk);

  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  release(&p->lock);
  acquire(lk);
}

// Wake up all processes sleeping on chan.
// Must be called without any p->lock.
void
wakeup(void *chan)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    if(p != myproc()){
      acquire(&p->lock);
      if(p->state == SLEEPING && p->chan == chan) {
        p->state = RUNNABLE;
        if(SCHEDULER == 2 || SCHEDULER == 3){
          enqueue(p-proc, (SCHEDULER == 3) ? qtable_stride[p-proc].pass : 0); // Use pass only for stride scheduler
        }
      }
      release(&p->lock);
    }
  }
}

// Kill the process with the given pid.
// The victim won't exit until it tries to return
// to user space (see usertrap() in trap.c).
int
kill(int pid)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++){
    acquire(&p->lock);
    if(p->pid == pid){
      p->killed = 1;
      if(p->state == SLEEPING){
        // Wake process from sleep().
        p->state = RUNNABLE;
        if(SCHEDULER == 2 || SCHEDULER == 3){
          uint64 pindex = p - proc;
          enqueue(p-proc, (SCHEDULER == 3) ? qtable_stride[pindex].pass : 0); // Use pass only for stride scheduler
        }
      }
      release(&p->lock);
      return 0;
    }
    release(&p->lock);
  }
  return -1;
}

void
setkilled(struct proc *p)
{
  acquire(&p->lock);
  p->killed = 1;
  release(&p->lock);
}

int
killed(struct proc *p)
{
  int k;
  
  acquire(&p->lock);
  k = p->killed;
  release(&p->lock);
  return k;
}

// Copy to either a user address, or kernel address,
// depending on usr_dst.
// Returns 0 on success, -1 on error.
int
either_copyout(int user_dst, uint64 dst, void *src, uint64 len)
{
  struct proc *p = myproc();
  if(user_dst){
    return copyout(p->pagetable, dst, src, len);
  } else {
    memmove((char *)dst, src, len);
    return 0;
  }
}

// Copy from either a user address, or kernel address,
// depending on usr_src.
// Returns 0 on success, -1 on error.
int
either_copyin(void *dst, int user_src, uint64 src, uint64 len)
{
  struct proc *p = myproc();
  if(user_src){
    return copyin(p->pagetable, dst, src, len);
  } else {
    memmove(dst, (char*)src, len);
    return 0;
  }
}

// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [USED]      "used",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  struct proc *p;
  char *state;

  printf("\n");
  for(p = proc; p < &proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    printf("%d %s %s", p->pid, state, p->name);
    printf("\n");
  }
}
