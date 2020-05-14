# Lec2

- Assembly language `move`

  ```
  movl %eax, %edx			edx = eax
  movl $0x123, %edx		edx = 0x123
  movl 0x123, %edx		edx = * (int32_t*)0x123
  movl (%ebx), %edx 		edx = * (int32_t*)ebx
  movl 4(%ebx), %edx		edx = * (int32_t*)(ebx + 4)
  ```

- Stack operation

  Note that %esp points to the top of the stack, not the next free stack space.

  ```
  pushl %eax 		== 		subl $4, %esp
  						movl %eax, (%esp)
  popl %eax		== 		movl %eax, (%esp)
  						addl $4, %esp
  call 0x12345	== 		pushl %eip
  						movl $0x12345, %eip
  ret 			== 		popl %eip
  ```

- More memory

  - 8086: 16 16-bit registers, 20-bit addresses. The extra 4 bits are from *segment registers*: CS, SS, DS, ES. CS: code segment, SS: stack segment, DS and ES: data segment. 

    Virtual to physical translation: `phyAddr = virAddr + seg * 16`.

    Memory segmentation is tricky to use. 

  - 80386: 32-bit registers, 32-bit addresses. To achieve backwards compatibility, boots in 16-bit mode, `boot.S` switches to protected mode with 32-bit address. Prefix `0x66` to a 16-bit address gives 32-bit address and `.code32` in `bott.S` tells assembler to insert `0x66`. 80386 changed from segmented memory to paged memory.

- I/O Device Management

  - Special instruction I/O

    Special CPU instruction for accessing I/O device, used by 8086. Only 1024 I/O ports.

  - Memory-mapped I/O

    Use normal address to access I/O devices, no special instructions. No 1024 limit. I/O devices are accessed like memory. Used by 80386.

- x86 physical memory

  ```
  +------------------+  <- 0xFFFFFFFF (4GB)
  |      32-bit      |
  |  memory mapped   |
  |     devices      |
  |                  |
  /\/\/\/\/\/\/\/\/\/\
  
  /\/\/\/\/\/\/\/\/\/\
  |      Unused      |
  +------------------+  <- depends on amount of RAM
  |                  |
  | Extended Memory  |
  |                  |
  +------------------+  <- 0x00100000 (1MB)
  |     BIOS ROM     |
  +------------------+  <- 0x000F0000 (960KB)
  |  16-bit devices, |
  |  expansion ROMs  |
  +------------------+  <- 0x000C0000 (768KB)
  |   VGA Display    |
  +------------------+  <- 0x000A0000 (640KB)
  |    Low Memory    |
  +------------------+  <- 0x00000000
  ```

  Write to VGA display appears on the screen.

- x86 instruction set

  Intel syntax: `opcode dst src`, AT&T syntax: `opcode src dst` .

  In this class, we use AY&Y syntax.

- gcc calling convetion for JOS

  - Note that stack grows from higher address to lower address.
  - Caller-saved and callee-saved registers.
    - Caller-saved: caller saves temporary values in its frame before making the call. Include: %eax, %ecx, %edx.
    - Callee-saved: callee saves the temporary values in its frame before using the register. Callee restores them before returning to the caller. Include: %ebp, %ebx, %esi, %edi.
  - At entry
    - `%eip` (instruction pointer) points to the first instruction of the function.
    - `%esp + 4` points to the first argument to the function.
    - `%esp` points at the return address.
  - After `ret` instruction
    - caller-saved registers may be trashed (and the caller should restore it from stack), while callee-saved registers should contain the content before the call (the callee should restore the original value before return).
    - %eax (and %edx, if return type is 64-bit), and %ecx could be trashed/modified.
    - The arguments to the called function could have been trashed/modified.
    - %esp points at arguments pushed by caller.
    - %eip points at the return address.

  - %ebp points at saved from previous function, used to walk stack. Function prologue:

    ```
    pushl %ebp			// save control link
    movl %esp, %ebp		// update %ebp
    ```

    Before return, %eip can be found easily

    ```
    movl %ebp, %esp
    popl %ebp
    ```

    ```
    	       +------------+   |
    	       | arg 2      |   \
    	       +------------+    >- previous function's stack frame
    	       | arg 1      |   /
    	       +------------+   |
    	       | ret %eip   |   /
    	       +============+   
    	       | saved %ebp |   \
    	%ebp-> +------------+   |
    	       |            |   |
    	       |   local    |   \
    	       | variables, |    >- current function's stack frame
    	       |    etc.    |   /
    	       |            |   |
    	       |            |   |
    	%esp-> +------------+   /
    ```

    ESP is the current stack pointer, which points to the top of the stack and will change any time anything is pushed or popped onto/off off the stack. EBP is a more convenient way for the compiler to keep track of a function's parameters and local variables than using the ESP directly, as ESP is varying but EBP is the same throughout one function call.

    Generally (this may vary for different compilers), all arguments to a function being called are pushed onto the stack by the calling function (usually in the reverse order that they're declared in the function prototype, which is case in our diagram: arg2 is pushed before arg1). Then the function is called, which pushes the return address, which is the address of the next instruction stored in EIP, onto the stack.

    Upon entry to the function, the old EBP value is pushed onto the stack and EBP is set to the value of ESP. Then the ESP is decremented to allocate space for the local variables and temporaries. From that point on, during the execution of the function, arguments to the function are located on the stack at positive offsets from EBP (because they were pushed prior to the function call), and local variables are located at negative offsets from EBP. 

    Upon exit, all the function has to do is set ESP to the value of EBP (which deallocates local variables and exposes the EBP of the calling function on top of the stack), then pop the old EBP value from the stack into EBP, and then the function returns (popping the return address into EIP).

    Upon returning back to the calling function, it can then increment ESP in order to remove the function arguments it pushed onto the stack before calling the other function. At this point, the stack is back in the same state it was before invoking the called function

    In short:

    ```
    // When calling the function
    push arguments;
    call function;  // push %eip to stack, set new %eip
    
    // Entry to the function: function prologue
    push %ebp;
    %ebp = %esp;
    
    // Function body
    Allocate space on stack for local variables
    
    // Return from function: function epilogue
    %esp = %ebp;  // free space + make %ebp of caller on stack top
    pop %ebp;     // pop the previous %ebp into %ebp
    ret;          // pop return address into %eip
    
    // Back at caller
    increment %esp;  // Free space for arguments
    ```

- Preprocessing, compiling, assembling, linking + loading

  - Preprocessor takes C source code (ASCII text), expands `#include` and other macros, produces C source code, `.c`
  - Compiler takes C source code (ASCII text), produces assembly language, `.asm`
  - Assembler takes assembly language (ASCII text), produces `.o` file (binary, machine-readable!)
  - Linker takes multiple `.o`'s, produces a single program image `.out`
  - Loader loads the program image into memory at run-time and starts it executing

- PC emulation

  The QEMU emulator works by

  - doing exactly what a real PC would do,
  - only implemented in software rather than hardware!

  