# HW4

## Part One: Eliminate allocation from sbrk()

```c
/* Without lazy page allocation */
// int
// sys_sbrk(void)
// {
//   int addr;
//   int n;

//   if(argint(0, &n) < 0)
//     return -1;
//   addr = myproc()->sz;
//   if(growproc(n) < 0)
//     return -1;
//   return addr;
// }

/* With lazy page allocation */
int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  myproc()->sz += n;
  return addr;
}
```

```
init: starting sh
$ echo hi
pid 3 sh: trap 14 err 6 on cpu 0 eip 0x12f1 addr 0x4004--kill proc
```

Actually, no matter what command you enter, you get the same output. The reason is that, `sh.c:execcmd` calls `malloc`, which calls `sbrk`. `Sbrk` returns successfully, but page fault is raised when later accessing the memory. 



## Part two: Lazy allocation

```c
case T_PGFLT:
    /* Lazy page allocation, without error checking/handling */
    ; // https://stackoverflow.com/a/18496437/9057530
    uint faultAddr = rcr2();
    uint pageBoundary = PGROUNDDOWN(faultAddr);
    char* mem = kalloc();
    memset(mem, 0, PGSIZE);
    
    int mappages(pde_t *pgdir, void *va, uint size, uint pa, int perm);
    mappages(myproc()->pgdir, (char *)pageBoundary, PGSIZE, V2P(mem), PTE_W|PTE_U);
    break;
```



