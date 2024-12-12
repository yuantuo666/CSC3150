#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "fcntl.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"

struct spinlock tickslock;
uint ticks;

extern char trampoline[], uservec[], userret[];

// in kernelvec.S, calls kerneltrap().
void kernelvec();

extern int devintr();

void
trapinit(void)
{
  initlock(&tickslock, "time");
}

// set up to take exceptions and traps while in the kernel.
void
trapinithart(void)
{
  w_stvec((uint64)kernelvec);
}

//
// handle an interrupt, exception, or system call from user space.
// called from trampoline.S
//
void
usertrap(void)
{
  int which_dev = 0;

  if((r_sstatus() & SSTATUS_SPP) != 0)
    panic("usertrap: not from user mode");

  // send interrupts and exceptions to kerneltrap(),
  // since we're now in the kernel.
  w_stvec((uint64)kernelvec);

  struct proc *p = myproc();
  
  // save user program counter.
  p->trapframe->epc = r_sepc();
  
  if(r_scause() == 8){
    // system call

    if(killed(p))
      exit(-1);

    // sepc points to the ecall instruction,
    // but we want to return to the next instruction.
    p->trapframe->epc += 4;

    // an interrupt will change sepc, scause, and sstatus,
    // so enable only now that we're done with those registers.
    intr_on();

    syscall();
  } else if((which_dev = devintr()) != 0){
    // ok
  }
  // TODO: page fault handling
  // - r_stval() provides trap value. (i.e. the address causing the exception)
  // - The swapping mechanism is not supported in the xv6 system. If the physical memory is filled,
  //   you are expected to kill the process. (You shall learn to use kalloc() and setkilled()
  //   functions)
  // - If there is spare space in physical memory, map one page of the file with the corresponding
  //   vma. ( mapfile() and mappages() )
  else if(r_scause() == 13 || r_scause() == 15) {
    // page fault, need to assign a page to the faulting address
    // Add code to cause a page-fault in a mmap-ed region to allocate a page of physical memory.

    uint64 addr = r_stval(); // get trap value

    struct proc *p = myproc();
    
    // 1. Find VMA for faulting address
    struct vma *v = 0;
    for(int i = 0; i < VMASIZE; i++) {
      if(p->vma[i].used && p->vma[i].addr <= addr && 
         addr < p->vma[i].addr + p->vma[i].length) {
        v = &p->vma[i];
        break;
      }
    }

    if(v == 0) {
      // no matching VMA, panic
      panic("page fault trap: no matching VMA");
      goto err;
    }

    // 2. Allocate page
    // Find corresponding valid vma by fault address.
    void *usable_addr = kalloc();
    if(usable_addr == 0) {
      // allocation failed, panic
      printf("page fault trap: allocation failed\n");
      // kill the process
      goto err;
    }
    
    memset(usable_addr, 0, PGSIZE); // init zero

    // 3. Read from file
    // - Read 4096 bytes of the relevant file onto that page, and map it into the user address space.
    // - Read the file with readi, which takes an offset argument at which to read in the file (but you
    // will have to lock/unlock the inode passed to readi).
    ilock(v->file->ip);
    int offset = v->offset + (addr - v->addr);
    if(readi(v->file->ip, 0, (uint64)usable_addr, offset, PGSIZE) < 0) {
      iunlock(v->file->ip);
      kfree(usable_addr);
      goto err;
    }
    iunlock(v->file->ip);

    // 4. Update page table
    // Set the permissions correctly on the page. Run mmaptest; it should get to the first munmap.
    int perm = PTE_U;
    if(v->prot & PROT_READ)
      perm |= PTE_R;
    if(v->prot & PROT_WRITE)
      perm |= PTE_W;
    if(v->prot & PROT_EXEC)
      perm |= PTE_X;

    if(mappages(p->pagetable, addr, PGSIZE, (uint64)usable_addr, perm) != 0) {
      kfree(usable_addr);
      printf("page fault trap: mapping failed\n");
      goto err;
    }
    
  }
  else {
  err:
    printf("usertrap(): unexpected scause %p pid=%d\n", r_scause(), p->pid);
    printf("            sepc=%p stval=%p\n", r_sepc(), r_stval());
    setkilled(p);
  }

  if(killed(p))
    exit(-1);

  // give up the CPU if this is a timer interrupt.
  if(which_dev == 2)
    yield();

  usertrapret();
}

//
// return to user space
//
void
usertrapret(void)
{
  struct proc *p = myproc();

  // we're about to switch the destination of traps from
  // kerneltrap() to usertrap(), so turn off interrupts until
  // we're back in user space, where usertrap() is correct.
  intr_off();

  // send syscalls, interrupts, and exceptions to uservec in trampoline.S
  uint64 trampoline_uservec = TRAMPOLINE + (uservec - trampoline);
  w_stvec(trampoline_uservec);

  // set up trapframe values that uservec will need when
  // the process next traps into the kernel.
  p->trapframe->kernel_satp = r_satp();         // kernel page table
  p->trapframe->kernel_sp = p->kstack + PGSIZE; // process's kernel stack
  p->trapframe->kernel_trap = (uint64)usertrap;
  p->trapframe->kernel_hartid = r_tp();         // hartid for cpuid()

  // set up the registers that trampoline.S's sret will use
  // to get to user space.
  
  // set S Previous Privilege mode to User.
  unsigned long x = r_sstatus();
  x &= ~SSTATUS_SPP; // clear SPP to 0 for user mode
  x |= SSTATUS_SPIE; // enable interrupts in user mode
  w_sstatus(x);

  // set S Exception Program Counter to the saved user pc.
  w_sepc(p->trapframe->epc);

  // tell trampoline.S the user page table to switch to.
  uint64 satp = MAKE_SATP(p->pagetable);

  // jump to userret in trampoline.S at the top of memory, which 
  // switches to the user page table, restores user registers,
  // and switches to user mode with sret.
  uint64 trampoline_userret = TRAMPOLINE + (userret - trampoline);
  ((void (*)(uint64))trampoline_userret)(satp);
}

// interrupts and exceptions from kernel code go here via kernelvec,
// on whatever the current kernel stack is.
void 
kerneltrap()
{
  int which_dev = 0;
  uint64 sepc = r_sepc();
  uint64 sstatus = r_sstatus();
  uint64 scause = r_scause();
  
  if((sstatus & SSTATUS_SPP) == 0)
    panic("kerneltrap: not from supervisor mode");
  if(intr_get() != 0)
    panic("kerneltrap: interrupts enabled");

  if((which_dev = devintr()) == 0){
    printf("scause %p\n", scause);
    printf("sepc=%p stval=%p\n", r_sepc(), r_stval());
    panic("kerneltrap");
  }

  // give up the CPU if this is a timer interrupt.
  if(which_dev == 2 && myproc() != 0 && myproc()->state == RUNNING)
    yield();

  // the yield() may have caused some traps to occur,
  // so restore trap registers for use by kernelvec.S's sepc instruction.
  w_sepc(sepc);
  w_sstatus(sstatus);
}

void
clockintr()
{
  acquire(&tickslock);
  ticks++;
  wakeup(&ticks);
  release(&tickslock);
}

// check if it's an external interrupt or software interrupt,
// and handle it.
// returns 2 if timer interrupt,
// 1 if other device,
// 0 if not recognized.
int
devintr()
{
  uint64 scause = r_scause();

  if((scause & 0x8000000000000000L) &&
     (scause & 0xff) == 9){
    // this is a supervisor external interrupt, via PLIC.

    // irq indicates which device interrupted.
    int irq = plic_claim();

    if(irq == UART0_IRQ){
      uartintr();
    } else if(irq == VIRTIO0_IRQ){
      virtio_disk_intr();
    } else if(irq){
      printf("unexpected interrupt irq=%d\n", irq);
    }

    // the PLIC allows each device to raise at most one
    // interrupt at a time; tell the PLIC the device is
    // now allowed to interrupt again.
    if(irq)
      plic_complete(irq);

    return 1;
  } else if(scause == 0x8000000000000001L){
    // software interrupt from a machine-mode timer interrupt,
    // forwarded by timervec in kernelvec.S.

    if(cpuid() == 0){
      clockintr();
    }
    
    // acknowledge the software interrupt by clearing
    // the SSIP bit in sip.
    w_sip(r_sip() & ~2);

    return 2;
  } else {
    return 0;
  }
}

