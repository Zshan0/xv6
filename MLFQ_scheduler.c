#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"

extern struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

// Interrupt descriptor table (shared by all CPUs).
extern struct gatedesc idt[256];
extern uint vectors[];  // in vectors.S: array of 256 entry pointers
extern struct spinlock tickslock;
extern uint ticks;





extern int nextpid;
extern void forkret(void);
extern void trapret(void);



//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.

void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
  for(;;){
    /* Enable interrupts on this processor. */
    sti();

    /* Loop over process table looking for process to run. */
    acquire(&ptable.lock);
    
    /* Iterating over the four priority queues. */
    int priority_level = 0;
    while(priority_level <= 4) {
      // cprintf("Looking in Queue: %d\n", priority_level);    
      int min_entry = 1e9;
      p = 0;

      /* finding the process with miniumum entry time in the current level.*/
      for(struct proc *min_entry_proc = ptable.proc; \
            min_entry_proc < &ptable.proc[NPROC]; \
            min_entry_proc++) {
        if(min_entry_proc->state == RUNNABLE && min_entry_proc->entry < min_entry) {
          // cprintf("Found one process in queue %d", min_entry_proc->cur_q);        
          p = min_entry_proc;
        }
      }

      if (p == 0) {
        priority_level += 1;
        continue;
      }
      /* Switching process to the found process */
      c->proc = p;
      switchuvm(p);
      p->time_slice = (int) (1u << (unsigned int) priority_level); // assign ticks to process based on priority level
      p->state = RUNNING;
      p->ran_times += 1;
      cprintf("MLFQ: Process switching to %d in queue %d for %d ticks\n", p->pid, p->cur_q, p->time_slice);
      swtch(&(c->scheduler), p->context);
      //cprintf("MLFQ: Back to scd!\n");
      switchkvm();
      // if the process uses entire time slice put move it down in prio
      if (p->time_slice == 0) {
       if(p->state == ZOMBIE){
        p->cur_q = -1;
       } else if (p->cur_q < 4) {
        p->cur_q += 1;
        cprintf("MLFQ: Process %d demoted to %d queue\n", p->pid, p->cur_q);
       }
      }

      /* 
        Checking for aging, if conditions satisfy we shift the process
        to the level above it.
      */
      for(struct proc *min_entry_proc = ptable.proc; min_entry_proc < &ptable.proc[NPROC]; min_entry_proc++) {
        if(min_entry_proc->state == RUNNABLE) {
          int time_ran = 1 << (priority_level + SHIFT_BYTE);
          if(ticks - min_entry_proc->entry > time_ran) {
            /* 
              the new entry time of the process is the current time
              since it just entered the queue.
            */
            min_entry_proc->entry = ticks;
            if(min_entry_proc->cur_q == 0)
              continue;
            min_entry_proc->cur_q -= 1;
          }
        }
      }

      /* Going back to the 1st queue and repeating the procedure */
      priority_level = 0; 
    } 
    release(&ptable.lock);

  }
}

//PAGEBREAK: 41
void
trap(struct trapframe *tf)
{
  if(tf->trapno == T_SYSCALL){
    if(myproc()->killed)
      exit();
    myproc()->tf = tf;
    syscall();
    if(myproc()->killed)
      exit();
    return;
  }

  switch(tf->trapno){
  case T_IRQ0 + IRQ_TIMER:
    if(cpuid() == 0){
      acquire(&tickslock);
      ticks++;
      // Updating the run time of the process running currently
      if(myproc() != 0 && myproc()->state == RUNNING) {
        myproc()->rtime += 1;
        /* 1 more tick spent in the current queue */
        myproc()->time_in_q[myproc()->cur_q] += 1;
        /* One tick of the process got used up */
        myproc()->time_slice -= 1;
      }
      wakeup(&ticks);
      release(&tickslock);
    }
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE:
    ideintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE+1:
    // Bochs generates spurious IDE1 interrupts.
    break;
  case T_IRQ0 + IRQ_KBD:
    kbdintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_COM1:
    uartintr();
    lapiceoi();
    break;
  case T_IRQ0 + 7:
  case T_IRQ0 + IRQ_SPURIOUS:
    cprintf("cpu%d: spurious interrupt at %x:%x\n",
            cpuid(), tf->cs, tf->eip);
    lapiceoi();
    break;

  //PAGEBREAK: 13
  default:
    if(myproc() == 0 || (tf->cs&3) == 0){
      // In kernel, it must be our mistake.
      cprintf("unexpected trap %d from cpu %d eip %x (cr2=0x%x)\n",
              tf->trapno, cpuid(), tf->eip, rcr2());
      panic("trap");
    }
    // In user space, assume process misbehaved.
    cprintf("pid %d %s: trap %d err %d on cpu %d "
            "eip 0x%x addr 0x%x--kill proc\n",
            myproc()->pid, myproc()->name, tf->trapno,
            tf->err, cpuid(), tf->eip, rcr2());
    myproc()->killed = 1;
  }

  // Force process exit if it has been killed and is in user space.
  // (If it is still executing in the kernel, let it keep running
  // until it gets to the regular system call return.)
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit();


  if(myproc() && myproc()->state == RUNNING &&
     tf->trapno == T_IRQ0+IRQ_TIMER && myproc()->time_slice <= 0)
    yield();

  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit();
}
