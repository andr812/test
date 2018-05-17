#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

extern struct {
  struct spinlock lock;
  struct proc proc[NPROC];
}ptable;

// s_cand is stride candidate
// Stride scheduler round this array
// for searching min-pass
extern struct stride s_cand[NPROC];
// ---------thread_create----------

// 1. Allocate process (fork())
// 2. child's pgdir copied from parent's pgdir
// 3. Allocate stack (allocuvm())
// 4. store return value & arg in child's stack
// 5. set esp register to starting point, eip to start_routine
// 6. set child's state to RUNNABLE
// 7. insert this LWP process to feedback Queue

int
thread_create(thread_t* thread, void* (*start_routine)(void*), void* arg)
{
  int i;
  uint base, size;
  char *sp;
  struct proc *np;
  struct proc *curproc = myproc();

  // 1. Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // 2. child's pgdir copied from parent's pgdir
  np->pid = curproc->pid;
  np->parent = curproc;
  *np->tf = *curproc->tf;
  np->sz = curproc->sz;
  np->pgdir = curproc->pgdir;
  np->is_LWP = 1;
  np->child_num++;

  // 3. Allocate stack
  for(i = 0; i < 64; i++){
    if(curproc->stackEmpty[i] == 0){
      np->thread_id = i;
      curproc->stackEmpty[i] = 1;
      break;
    }
  }

  base = curproc->stack_size + (np->thread_id) * PGSIZE;

  if((size = allocuvm(np->pgdir, base, base + PGSIZE)) == 0){
    return -1;
  }

  np->stack_base = base + PGSIZE;
  np->sz = size;
  sp = (char*)size;

  // 4. store return value & arg in child's stack
  sp -= 4;
  *(uint*)sp = (uint)arg;
  sp -= 4;
  *(uint*)sp = 0xffffffff;

  *thread = np->thread_id;

  // 5. set esp register to starting point, eip to start_routine
  np->tf->eax = 0;
  np->tf->esp =(uint)sp;
  np->tf->eip =(uint)start_routine;

  // copying file descriptor from function caller(curproc)
  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  // 6. set child's state to RUNNABLE
  acquire(&ptable.lock);

  np->state = RUNNABLE;

  release(&ptable.lock);

  return 0;
  // 7. insert this LWP process to feedback Queue
}

int
thread_join(thread_t thread, void** retval)
{
  struct proc *p;
  int havekids;
  struct proc *curproc = myproc();
  //cprintf("Init\n");

  acquire(&ptable.lock);
  for(;;){
    havekids = 0;
    // Scan through table and look for ZOMBIE and caller's child
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if((p->parent != curproc) || (p->thread_id != thread)){
        continue;
      }
      havekids = 1;
      if(p->state == ZOMBIE && p->is_LWP){
        // deallocate vm from pgdir from stack_base to stack_base - PGSIZE
        //cprintf("Inside\n");
        *retval = p->parent->retValue[thread];

        //cprintf("thread_id:%d retval: %d\n",thread, (int)*retval);
        deallocuvm(p->pgdir, p->stack_base, p->stack_base - PGSIZE);
        kfree(p->kstack);
        p->kstack = 0;
        p->pid = 0;
        p->parent = 0;
        p->is_LWP = 0;
        curproc->stackEmpty[thread] = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;

        //cprintf("Return\n");
        release(&ptable.lock);
        //cprintf("2!!thread_id:%d retval: %d\n",thread, (int)*retval);
        return 0;
      }
    }
    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;

    }

  // Wait for children to exit.  (See wakeup1 call in proc_exit.)
  sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
  return 0;
}

void
thread_exit(void* retval)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;
  struct proc *curparent = curproc->parent;

  // Close all open files of its own
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }
  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  //cprintf("thread_exit--------- retval:%d\n",(int)retval);
  // curparent might wait for thread to end(Not yet anyof them finished)
  wakeup1(curparent);
  // test
  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if((p->parent != curproc) || (p->is_LWP != 1)){
      continue;
    }
    if(p->parent == curparent){
    cprintf("Found here thread:%d retval:%d\n",curproc->thread_id, (int)retval);
  }
}
  // To change process state
  //acquire(&ptable.lock);

  if(curproc->myst == s_cand){
    pop_MLFQ(curproc);
  }else{
    // If it is run on Stride scheduler, reset Stride share
    curproc->myst->valid = 0;
    s_cand[0].share += curproc->myst->share;
    s_cand[0].stride = 10000000 / s_cand[0].share;
  }
  // save return value
  curparent->retValue[curproc->thread_id] = retval;
  //cprintf("thread_exit--------- retval:%d   curparent->retValue:%d  curproc->thread_id:%d\n",(int)retval, (int)curparent->retValue[curproc->thread_id], (int)curproc->thread_id);
  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}
