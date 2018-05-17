#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"

int
sys_fork(void)
{
  return fork();
}

int
sys_yield(void)
{
  if(ticks % 2 == 0){
    MLFQ_tick_adder();
  } // For prevent gaming the scheduler - project 2
  yield();
  return 0;
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return myproc()->pid;
}

int
sys_getppid(void)
{
    return myproc()->parent->pid;
}

int
sys_getlev(void)
{
  return myproc()->prior;
}

int
sys_set_cpu_share(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  return set_cpu_share(n);
}

int
sys_thread_create(void)
{
  thread_t* thread;
  void* (*start_routine)(void*);
  void* arg;
  int n;
  //argument 0
  if(argint(0, &n) < 0)
    return -1;
  thread = (thread_t*)n;
  //argument 1
  if(argint(1, &n) < 0)
    return -1;
  start_routine = (void*)n;
  //argument 2
  if(argint(2, &n) < 0)
    return -1;
  arg = (void*)n;

  return thread_create(thread, start_routine, arg);
}

int
sys_thread_exit(void)
{
  void* retval;
  int n;

  //argument 0
  if(argint(0, &n) < 0)
    return -1;
  retval = (void*)n;

  thread_exit(retval);
  return 0;
}

int
sys_thread_join(void)
{
  thread_t thread;
  void** retval;
  int n;

  //argument 0
  if(argint(0, &n) < 0)
    return -1;
  thread = n;

  //argument 1
  if(argint(0, &n) < 0)
    return -1;
  retval = (void**)n;

  return thread_join(thread, retval);
}

int
sys_sbrk(void)
{
  struct proc *curproc = myproc();
  //int addr;
  //int paddr;
  int n;
  //cprintf("Fine?");
  if(argint(0, &n) < 0)
    return -1;
  //addr = myproc()->sz;
  //addr = curproc->sz;
  //paddr = curproc->parent->sz;
  if(growproc(n) < 0)
    return -1;

  // if myproc is LWP grow parent proc
  if(curproc->is_LWP)
    return (curproc->parent->sz + PGSIZE * 64 - n);
  // if myproc is Non_LWP
  else if(curproc->child_num == 0)
    return (curproc->sz - n);
    //return addr;
  else
    return (curproc->sz + PGSIZE * 64 - n);

  //return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}
