#include "types.h"
#include "riscv.h"
#include "param.h"
#include "defs.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;


  argint(0, &n);
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}


#ifdef LAB_PGTBL
int
sys_pgaccess(void)
{
  // lab pgtbl: your code here.
  uint64 va_start;
  int npages;
  uint64 addr;
  uint64 va;
  pte_t *pte;
  struct proc* p;
  unsigned int abits;

  argaddr(0, &va_start);
  argint(1, &npages);
  argaddr(2, &addr);

  p = myproc();
  va_start = PGROUNDDOWN(va_start);
  if(npages > sizeof(unsigned int) * 8)
    npages = sizeof(unsigned int) * 8;
  
  abits = 0;
  for(int i = 0; i < npages; i++) {
    va = va_start + i * PGSIZE;
    pte = walk(p->pagetable, va, 0);
    if(pte && (*pte & PTE_V) && (*pte & PTE_A)) {
      abits |= (1U << i);
      *pte &= ~PTE_A;
    }
  }
  if(copyout(p->pagetable, addr, (char *)&abits, sizeof(abits)) < 0)
    return -1;

  return 0;
}
#endif

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}
