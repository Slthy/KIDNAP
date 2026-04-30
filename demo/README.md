# KIDNAP Demo

## Idea

Different inputs do not just change program behavior.  
They trigger different kernel behavior.

This demo shows that we can observe this and use it as feedback.

---

## Demo Flow

### 1. Reset

```bash
rm -rf out in seen.txt trace.txt current.txt tmp
mkdir in
printf "A" > in/seed
```

### 2. Compile
```bash
../AFLplusplus/afl-clang-fast -o mini mini_kernel_workload.c
```

### 3. Run AFL++
```bash
AFL_SKIP_CPUFREQ=1 ../AFLplusplus/afl-fuzz -i in -o out -- ./mini
```
Let it run for ~20–30 seconds, then stop it.

### 4. Show generated inputs
```bash
ls out/default/queue
```

### 5. Show input content
```bash
head -c 16 out/default/queue/id:00000*
```

### 6. Show kernel behavior
```bash
./demo_show
```

### Observation

Different inputs produce different syscalls.

Examples:

```c++
getpid
open, write, close
pipe
mkdir
```

### Point
AFL++ uses user-space coverage as feedback.

This demo shows we can use kernel-visible behavior as an additional signal.

### Takeaway

We are not just fuzzing the program.
We are exploring the kernel behavior induced by the program.