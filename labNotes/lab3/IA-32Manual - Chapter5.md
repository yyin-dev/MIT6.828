# Chapter 5 Interrupt and Exception Handling

## 5.1 Interrupt and exception overview

Hardware devices can generate interrupts, and software can also generate interrupts using `INT n` instruction. Exceptions occur when the processor detects an error condition at execution. When an interrupt or an exception happens, the current procedure is suspended and the interrupt/exception handler is executed. When the handler finishes, the interrupted procedure is resumed.



## 5.2 Exception and interrupt vectors

Each exception/interrupt is assigned a unique identifier, called a *vector*. The processor uses the vector as an index into the Interrupt Descriptor Table (IDT). The table provides the entry point to the handler.

Table 5-1 shows exception type and whether an error code is pushed for 0-31.



## 5.3 Sources of interrupts

External (hardware generated); Software-generated.



