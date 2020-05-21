# HW3

```diff
yy0125@yy0125:~/Desktop/xv6-public$ git diff
diff --git a/Makefile b/Makefile
index 09d790c..54c3fb5 100644
--- a/Makefile
+++ b/Makefile
@@ -181,6 +181,7 @@ UPROGS=\
        _usertests\
        _wc\
        _zombie\
+       _date\
 
 fs.img: mkfs README $(UPROGS)
        ./mkfs fs.img README $(UPROGS)
diff --git a/syscall.c b/syscall.c
index ee85261..9d63d0d 100644
--- a/syscall.c
+++ b/syscall.c
@@ -103,6 +103,7 @@ extern int sys_unlink(void);
 extern int sys_wait(void);
 extern int sys_write(void);
 extern int sys_uptime(void);
+extern int sys_date(void);
 
 static int (*syscalls[])(void) = {
 [SYS_fork]    sys_fork,
@@ -119,6 +120,7 @@ static int (*syscalls[])(void) = {
 [SYS_sbrk]    sys_sbrk,
 [SYS_sleep]   sys_sleep,
 [SYS_uptime]  sys_uptime,
+[SYS_date]    sys_date,
 [SYS_open]    sys_open,
 [SYS_write]   sys_write,
 [SYS_mknod]   sys_mknod,
diff --git a/syscall.h b/syscall.h
index bc5f356..1a620b9 100644
--- a/syscall.h
+++ b/syscall.h
@@ -20,3 +20,4 @@
 #define SYS_link   19
 #define SYS_mkdir  20
 #define SYS_close  21
+#define SYS_date   22
diff --git a/sysproc.c b/sysproc.c
index 0686d29..9b0075d 100644
--- a/sysproc.c
+++ b/sysproc.c
@@ -89,3 +89,12 @@ sys_uptime(void)
   release(&tickslock);
   return xticks;
 }
+
+int
+sys_date(void)
+{
+  struct rtcdate *p;
+  argptr(0, (char **)&p, sizeof(struct rtcdate));
+  cmostime(p);
+  return 0;
+}
diff --git a/user.h b/user.h
index 4f99c52..3e791dd 100644
--- a/user.h
+++ b/user.h
@@ -23,6 +23,7 @@ int getpid(void);
 char* sbrk(int);
 int sleep(int);
 int uptime(void);
+int date(struct rtcdate*);
 
 // ulib.c
 int stat(const char*, struct stat*);
diff --git a/usys.S b/usys.S
index 8bfd8a1..93359ed 100644
--- a/usys.S
+++ b/usys.S
@@ -29,3 +39,4 @@ SYSCALL(getpid)
 SYSCALL(sbrk)
 SYSCALL(sleep)
 SYSCALL(uptime)
+SYSCALL(date)
```

`syscall.c`: add the system call function `sys_date` to the vector;

`syscall.h`: assign the syscall number;

`sysproc.c`: actual syscall procedure;

`user.h`: expose the interface;

`usys.S`: the `date` definition.