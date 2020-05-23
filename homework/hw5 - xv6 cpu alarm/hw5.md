# HW5

```diff
diff --git a/Makefile b/Makefile
index 54c3fb5..eab990c 100644
--- a/Makefile
+++ b/Makefile
@@ -182,6 +182,7 @@ UPROGS=\
 	_wc\
 	_zombie\
 	_date\
+	_alarmtest\
 
 fs.img: mkfs README $(UPROGS)
 	./mkfs fs.img README $(UPROGS)
diff --git a/proc.c b/proc.c
index be3afe8..759df58 100644
--- a/proc.c
+++ b/proc.c
@@ -119,6 +119,9 @@ found:
   memset(p->context, 0, sizeof *p->context);
   p->context->eip = (uint)forkret;
 
+  // alarm sys call
+  p->alarmhandler = 0;
+  p->prevalarmtick = 0;
   return p;
 }
 
diff --git a/proc.h b/proc.h
index 1647114..e59db84 100644
--- a/proc.h
+++ b/proc.h
@@ -49,6 +49,10 @@ struct proc {
   struct file *ofile[NOFILE];  // Open files
   struct inode *cwd;           // Current directory
   char name[16];               // Process name (debugging)
+
+  int alarmticks;              // interval between two calls to handler
+  void (*alarmhandler)();      // alarm handler. If NULL, not set up for alarm().
+  uint prevalarmtick;          // system tick when prev handler is invoked
 };
 
 // Process memory is laid out contiguously, low addresses first:
diff --git a/syscall.c b/syscall.c
index 9d63d0d..fe5dced 100644
--- a/syscall.c
+++ b/syscall.c
@@ -104,6 +104,7 @@ extern int sys_wait(void);
 extern int sys_write(void);
 extern int sys_uptime(void);
 extern int sys_date(void);
+extern int sys_alarm(void);
 
 static int (*syscalls[])(void) = {
 [SYS_fork]    sys_fork,
@@ -128,6 +129,7 @@ static int (*syscalls[])(void) = {
 [SYS_link]    sys_link,
 [SYS_mkdir]   sys_mkdir,
 [SYS_close]   sys_close,
+[SYS_alarm]   sys_alarm,
 };
 
 void
diff --git a/syscall.h b/syscall.h
index 1a620b9..c32611f 100644
--- a/syscall.h
+++ b/syscall.h
@@ -21,3 +21,4 @@
 #define SYS_mkdir  20
 #define SYS_close  21
 #define SYS_date   22
+#define SYS_alarm  23
diff --git a/sysproc.c b/sysproc.c
index 266fd8c..054378e 100644
--- a/sysproc.c
+++ b/sysproc.c
@@ -113,3 +113,18 @@ sys_date(void)
   cmostime(p);
   return 0;
 }
+
+int
+sys_alarm(void)
+{
+  int ticks;         // interval that handler should be called
+  void (*handler)(); // handler function
+
+  if(argint(0, &ticks) < 0)
+    return -1;
+  if(argptr(1, (char**)&handler, 1) < 0)
+    return -1;
+  myproc()->alarmticks = ticks;
+  myproc()->alarmhandler = handler;
+  return 0;
+}
diff --git a/trap.c b/trap.c
index 463d876..bf29ea0 100644
--- a/trap.c
+++ b/trap.c
@@ -21,6 +21,11 @@ tvinit(void)
 
   for(i = 0; i < 256; i++)
     SETGATE(idt[i], 0, SEG_KCODE<<3, vectors[i], 0);
+  
+  // The x86 treats syscall differently. (1) The 2nd argment is 1, not clearing
+  // IF flag when trapping into the kernel, allowing other interrupts to
+  // happen during the meantime. (2) The last argument is DPL_USER, allowing
+  // user to invoke using `int`.
   SETGATE(idt[T_SYSCALL], 1, SEG_KCODE<<3, vectors[T_SYSCALL], DPL_USER);
 
   initlock(&tickslock, "time");
@@ -51,6 +56,32 @@ trap(struct trapframe *tf)
     if(cpuid() == 0){
       acquire(&tickslock);
       ticks++;
+
+      // alarm syscall
+      struct proc *p = myproc();
+      if (p != 0 && (tf->cs & 3) == 3) {
+        if (p->alarmhandler) {
+          if (ticks - p->prevalarmtick >= p->alarmticks) {
+            p->prevalarmtick = ticks;
+
+            // Cannot do `p->alarmhandler()`. We're in kernel space and 
+            // using kernel stack, but the user function should be
+            // called in user space and on user stack.
+
+            //`tf->eip` and `p->alarmhandler` matches alarmtest.asm.
+            // cprintf("tf->eip: 0x%x\n", tf->eip);
+            // cprintf("handler: 0x%x\n", (uint) p->alarmhandler);
+            
+            // Set return address (next instruction when interrupt happens) 
+            // after the handler returns
+            tf->esp -= 4;
+            * ((uint *)tf->esp) = tf->eip;
+            
+            // Set return address (handler) after return to user space
+            tf->eip = (uint) p->alarmhandler;
+          }
+        }
+      }
       wakeup(&ticks);
       release(&tickslock);
     }
diff --git a/user.h b/user.h
index 3e791dd..7724bbc 100644
--- a/user.h
+++ b/user.h
@@ -24,6 +24,7 @@ char* sbrk(int);
 int sleep(int);
 int uptime(void);
 int date(struct rtcdate*);
+int alarm(int ticks, void (*handler)());
 
 // ulib.c
 int stat(const char*, struct stat*);
diff --git a/usys.S b/usys.S
index 93359ed..53cb35b 100644
--- a/usys.S
+++ b/usys.S
@@ -40,3 +40,4 @@ SYSCALL(sbrk)
 SYSCALL(sleep)
 SYSCALL(uptime)
 SYSCALL(date)
+SYSCALL(alarm)
```

Most of the files are easy, just follow the code for other syscalls. The idea is clear that the handler of timer interrupt should do most of the work. Thus, let's focus on `trap.c`.

### The wrong attempt

```c
case T_IRQ0 + IRQ_TIMER:
    if(cpuid() == 0){
      acquire(&tickslock);
      ticks++;

      // alarm syscall
      struct proc *p = myproc();
      if (p != 0 && (tf->cs & 3) == 3) {
        if (p->alarmhandler) {
          if (ticks - p->prevalarmtick >= p->alarmticks) {
            p->prevalarmtick = ticks;
			p->alarmhandler();
          }
        }
      }
      wakeup(&ticks);
      release(&tickslock);
    }
    lapiceoi();
    break;
```

The problem with this approach is that it tries to execute a user function in kernel mode. We're in kernel space and using kernel stack, but the user function should be called in user space and on user stack. So this clearly wouldn't work. Surprisingly, no exception is thrown at execution, but just silently fails. 

### The correct way

With the understanding that we cannot execute the handler in the kernel mode, it's clear that we should set up the stack in some way such that the handler is executed when returnning back to user mode. We know %eip is pushed onto stack by the processor as part of the stackframe when the interrupt happens, so we do:

```c
tf->eip = (uint) p->alarmhandler;
```

However, this's not enough. This makes the handler run when returning back to the user mode, but doesn't control what runs after the handler finishes. We want to make sure the program executes the next instruction when the interrupt is generated. But how to do this?

Refer back to xv6 book, it detailedly explains how the user stack and kernel stack is used when a syscall is made: (1) %eip is pushed onto **user stack**, (2) hardware switches to **kernel stack**, (3) push registers to form the trapframe. Note that two %eip values are pushed to two different stacks.

**Important**: For interrupt, the only difference from syscall is that nothing is pushed onto the user stack. In other words, (2) and (3) are executed but not (1), as the user code doesn't know when the interrupt would happen. 

So how can we control what's executed after the alarm handler returns? The key thing to notice is that the alarm hander is a function and the return statement would try to pop the return address from the **user stack**! So push the return address originally in `tf->eip` to the user stack, and the program executes where it left off when the interrupt happens. You can manipulate the user stack through `tf->esp`. 

```c
case T_IRQ0 + IRQ_TIMER:
    if(cpuid() == 0){
      acquire(&tickslock);
      ticks++;

      // alarm syscall
      struct proc *p = myproc();
      if (p != 0 && (tf->cs & 3) == 3) {
        if (p->alarmhandler) {
          if (ticks - p->prevalarmtick >= p->alarmticks) {
            p->prevalarmtick = ticks;

            // Set return address (next instruction when interrupt happens) 
            // after the handler returns
            tf->esp -= 4;
            * ((uint *)tf->esp) = tf->eip;
            
            // Set return address (handler) after return to user space
            tf->eip = (uint) p->alarmhandler;
          }
        }
      }
      wakeup(&ticks);
      release(&tickslock);
    }
    lapiceoi();
    break;
```

This's a very good exercise and requires solid understanding of the syscall & interrupt mechanism, and the similarities and differences.

PS: Lecture 8 notes also discuess this homework. 