# Lab3 Summary

JOS environments are conceptually similar to Unix processes. 



## Part A: User Environments and Exception Handling

Exercise 1: Allocate and map `envs` array at `UENVS`. 

Exercise 2: Write functions to create, set up, and run environments.

Exercise 3: read about interrupt/exception/syscall mechanism.

Exercise 4: Implement basic interrupt/exception mechanism by (1) constructing vector entries, and (2) setting up IDT.



## Part B: Page Faults, Breakpoints Exceptions, and System Calls

Exercise 5: Dispatch and handle page faults.

Exercise 6: Dispatch and handle breakpoint exception.

Exercise 7: Implement system calls.

Exercise 8: Set up `thisenv` before user program starts running.

Exercise 9, 10: Add memory protection & check.