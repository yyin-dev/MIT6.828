# Chapter 9 Exceptions and Interrupts

Interrupts are asynchronous, while exceptions are synchronous.

Source for interrupts: (1) Maskable (external) interrupts, signalled via INTR pin; (2) Nonmaskable interrupts, signalled via NMI (Non-Maskable Interrupt) pin.

Source for exceptions: (1) Processor detected. Further classfied as faults, traps and aborts; (2) Programmed.

## 9.1 Identifying Interrupts

The processor associates an identifying number with each type of interrupt or exception.

Exceptions are classified as faults, traps, or aborts, depending on the way they are reported and whether restart of the instruction that caused the exception is supported.

## 9.2 Enabling and Disabling interrupts

When an NMI handler is executing, the processor ignores further interrupt signals at the NMI pin until the next IRET instruction is executed.

The IF (interrupt-enable flag) controls the acceptance of external interrupts signalled via the INTR pin. When IF=0, INTR interrupts are inhibited; when IF=1, INTR interrupts are enabled. CLI and STI alter the setting the IF. These instructions may be executed only if CPL <= IOPL. A protection exception occurs if they are executed when CPL > IOPL.

Software that needs to change stack segments often uses a pair of instructions, like:

```
mov %ax, %ss
mov StackTop, %esp
```

The two instructions should be atomic. So 80386 inhibits interrupts and excpetions after a `mov` to %ss, or a `pop` from %ss.

## 9.3 Priority Among Simultaneous Interrupts and Exceptions

If more than one interrupts/exceptions are pending, the processor first services a pending interrupt or exception with the highest priority, transferring control to the interrupt handler. Lower priority exceptions are discarded; lower priority interrupts are held pending.

## 9.4 Interrupt Descriptor Table

IDT associates each interrupt/excepion identifier with a descriptor of the handler. Like GDT/LDT, IDT is an array of 8-byte descriptors. `IDT address = interrupt/exception identifier * 8`. 

IDT can be anywhere in the physical memory. The 6-byte IDT regsiter (IDTR) stores the linear address of the base address and limit of IDT, and can be accesses using `LIDT` and `SIDT`. `LIDT` loads into IDTR, and `SIDT` copies IDTR. `LIDT` can only be executed in privilege mode. 

![img](https://pdos.csail.mit.edu/6.828/2018/readings/i386/fig9-1.gif)

## 9.5 IDT Descriptors

The IDT may contain any of 3 kinds of descriptors:

- Task gate (discussed in Chapter 7)
- Interrupt gates
- Trap gates

![img](https://pdos.csail.mit.edu/6.828/2018/readings/i386/fig9-3.gif)

Each interrupt/trap descriptor contains a selector into the descritptor and an offset.

## 9.6 Interrupt Tasks and Interrupt Procedures

When indexing into the IDT, if getting an interrupt gate or trap gate, the handler is invoked.

An interrupt/trap gate points **indirectly** to a procedure. The selector of the gate points to an executable-segment descriptor in GDT/current LDT. The offset of the gate points to the beginning of the handler.

![img](https://pdos.csail.mit.edu/6.828/2018/readings/i386/fig9-4.gif)

The control transfer to a handler uses the kernel stack, indicated by TSS, to store information needed for future resumption, because the user stack might have already been corrupted. With privilege transition, old %ss, %esp are pushed onto the stack. Then %eflags, %cs, %eip. Some also pushes the error code.

![img](https://pdos.csail.mit.edu/6.828/2018/readings/i386/fig9-5.gif)

`iret` is used to return from interrupt handler. 

The difference between an interrupte gate and a trap gate is in the effect on IF. An interrupt that vectors through an interrupt gate resets IF, preventing other interrupts from intefering. A subsequent `iret` restores IF. An interrupt through the trap gate doesn't change IF (so interrupts can happen during system calls). 

The processor doesn't permit an interrupt to transfer control to a procedure of less privilege.

## 9.7 Error Code

The processor pushes an error code for some exceptions.

## 9.8 Exception Condition

Faults: The CS and EIP values saved when a fault is reported point to the instruction causing the fault. So **the same instruction is re-executed** when handler finishes. 

Traps: The CS and EIP values stored when the trap is reported point to the instruction **dynamically** after the instruction causing the trap. If a trap is detected during an instruction that alters program flow, the reported values of CS and EIP reflect the alteration of program flow. So **the next instruction is executed** when handler finishes.

Aborts: An abort is an exception that permits neither precise location of the instruction causing the exception nor restart of the program that caused the exception. Aborts are used to report severe errors.

The behavior of each exception is documented.

## 9.9 Exception Summary

Summarizes behavior of exceptions recognized by 386.