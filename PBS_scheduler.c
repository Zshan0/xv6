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
    // Enable interrupts on this processor.
    int priority = 101;
    int found = 0;
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
      if(p->state == RUNNABLE && priority > p->priority) {
        priority = p->priority;
        found = 1;
      }
    }

    struct proc *same_priority;
    if(found == 1) {

      for(same_priority = ptable.proc; \
          same_priority < &ptable.proc[NPROC]; same_priority++) {
        if(same_priority->state == RUNNABLE && same_priority->priority == priority) {
          c->proc = same_priority;
          switchuvm(same_priority);
          same_priority->state = RUNNING;
          p->ran_times += 1;
          // cprintf("switching to process pid: %d %d\n",
          //       same_priority->pid, same_priority->priority);
          swtch(&(c->scheduler), same_priority->context);
          switchkvm();

          // Process is done running for now.
          // It should have changed its p->state before coming back.
          c->proc = 0;
          // Updating the the lowest priority after each switch.
          for(p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
            if(p->state == RUNNABLE && priority > p->priority) {
              priority = p->priority;
              found = 1;
            }
          }
        } 
      }
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
      wakeup(&ticks);
      release(&tickslock);
      for(struct proc *p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
        if(p != 0 && p->state == RUNNING) {
          p->rtime += 1;
        }
      }
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
     tf->trapno == T_IRQ0+IRQ_TIMER)
    yield();

  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit();
}
