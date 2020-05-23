# HW6

Without lock, key can be missing. Reason: concurrent write.

```c
static void insert(int key, int value, struct entry **p, struct entry *n) {
  struct entry *e = malloc(sizeof(struct entry));
  e->key = key;
  e->value = value;
  e->next = n;
  *p = e;
}

static void put(int key, int value) {
  int i = key % NBUCKET;
  insert(key, value, &table[i], table[i]);
}
```

Suppose two threads call `insert` at the same time, and both reached `e->next = n`. But only one would be the new head of the bucket after `*p = e`.

```
+---+
|	|-----------+
+---+			|
				V
			 +---+     +---+     +---+     +---+
			 |   |---> |   |---> |   |---> |   |---> ...
			 +---+     +---+     +---+     +---+
				^
+---+			|
|	|-----------+
+---+
```

Fix: insert locking to avoid concurrent write. Very easy.

## Performance Statistics

Benchmark: Single thread output.

```
$ gcc -g -O2 ph.c -pthread && ./a.out 1
0: put time = 0.004969
0: get time = 9.500714
0: 0 keys missing
completion time = 9.505846
```

Two-thead, lock around `get` and `put`.

```
$ gcc -g -O2 ph.c -pthread && ./a.out 2
0: put time = 0.024895
1: put time = 0.026029
1: get time = 23.600087
1: 0 keys missing
0: get time = 23.600087
0: 0 keys missing
completion time = 23.626235
```

Two-thead, lock around `put`:

```
$ gcc -g -O2 ph.c -pthread && ./a.out 2
1: put time = 0.026590
0: put time = 0.027234
0: get time = 11.169760
0: 0 keys missing
1: get time = 11.173100
1: 0 keys missing
completion time = 11.200438
```

Two-thead, per-bucket lock around `put`:

```
$ gcc -g -O2 ph.c -pthread && ./a.out 2
0: put time = 0.016501
1: put time = 0.016562
1: get time = 9.434039
1: 0 keys missing
0: get time = 9.435603
0: 0 keys missing
completion time = 9.452317
```



