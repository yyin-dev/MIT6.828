#include <errno.h>
#include <math.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <unistd.h>

static size_t page_size;

// align_down - rounds a value down to an alignment
// @x: the value
// @a: the alignment (must be power of 2)
//
// Returns an aligned value.
#define align_down(x, a) ((x) & ~((typeof(x))(a)-1))

#define AS_LIMIT (1 << 25)   // Maximum limit on virtual memory bytes (AS: address space)
#define MAX_SQRTS (1 << 27)  // Maximum limit on sqrt table entries
static double *sqrts;

////////////////////////// Added global variables /////////////////////////
#define DEBUG
static int count = 0;

static void *prev_mapped = NULL;
///////////////////////////////////////////////////////////////////////////

// Use this helper function as an oracle for square root values.
static void
calculate_sqrts(double *sqrt_pos, int start, int nr) {
    int i;
    for (i = 0; i < nr; i++) {
        sqrt_pos[i] = sqrt((double)(start + i));
    }
}

static void
handle_sigsegv(int sig, siginfo_t *si, void *ctx) {
#ifdef DEBUG
    printf("Handler called\n");
#endif

    // Your code here.
    uintptr_t fault_addr = (uintptr_t)si->si_addr;
    uintptr_t aligned = align_down(fault_addr, page_size);
    uintptr_t start_index = (aligned - (uintptr_t)sqrts) / sizeof(double);
    void *mapped;

    if (prev_mapped) {
        if (munmap(prev_mapped, page_size) < 0) {
            printf("munmap fails\n");
            exit(EXIT_FAILURE);
        }
    }

    if ((mapped = mmap((void *)aligned, page_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0)) < 0) {
        printf("mmap fails\n");
        exit(EXIT_FAILURE);
    }

    prev_mapped = mapped;

#ifdef DEBUG
    ++count;
    printf("count: %d\n", count);

    printf("fault_addr: %p, try to map: %p, mapped: %p\n", fault_addr, aligned, mapped);
    printf("Print value at allocated location: *(%p) = %ld\n", mapped, *(uintptr_t *)mapped);
#endif

    calculate_sqrts(mapped, start_index, page_size / sizeof(double));
}

static void
setup_sqrt_region(void) {
    struct rlimit lim = {AS_LIMIT, AS_LIMIT};
    struct sigaction act;

    // Only mapping to find a safe location for the table.
    sqrts = mmap(NULL, MAX_SQRTS * sizeof(double) + AS_LIMIT, PROT_NONE,
                 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (sqrts == MAP_FAILED) {
        fprintf(stderr, "Couldn't mmap() region for sqrt table; %s\n",
                strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Now release the virtual memory to remain under the rlimit.
    if (munmap(sqrts, MAX_SQRTS * sizeof(double) + AS_LIMIT) == -1) {
        fprintf(stderr, "Couldn't munmap() region for sqrt table; %s\n",
                strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Set a soft rlimit on virtual address-space bytes.
    if (setrlimit(RLIMIT_AS, &lim) == -1) {
        fprintf(stderr, "Couldn't set rlimit on RLIMIT_AS; %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Register a signal handler to capture SIGSEGV.
    act.sa_sigaction = handle_sigsegv;
    act.sa_flags = SA_SIGINFO;
    sigemptyset(&act.sa_mask);
    if (sigaction(SIGSEGV, &act, NULL) == -1) {
        fprintf(stderr, "Couldn't set up SIGSEGV handler;, %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
}

static void
test_sqrt_region(void) {
    int i, pos = rand() % (MAX_SQRTS - 1);
    double correct_sqrt;

    printf("Validating square root table contents...\n");
    srand(0xDEADBEEF);

    for (i = 0; i < 500000; i++) {
        if (i % 2 == 0)
            pos = rand() % (MAX_SQRTS - 1);
        else
            pos += 1;
        calculate_sqrts(&correct_sqrt, pos, 1);
        if (sqrts[pos] != correct_sqrt) {
            fprintf(stderr, "Square root is incorrect. Expected %f, got %f.\n",
                    correct_sqrt, sqrts[pos]);
            exit(EXIT_FAILURE);
        }
    }

    printf("All tests passed!\n");
#ifdef DEBUG
    printf("MAX page allowed: %ld\n", AS_LIMIT / page_size);
    printf("Number of pages neede: %ld\n", MAX_SQRTS * sizeof(double) / page_size);
#endif DEBUG
}

int main(int argc, char *argv[]) {
    page_size = sysconf(_SC_PAGESIZE);
    printf("page_size is %ld\n", page_size);
    setup_sqrt_region();
    test_sqrt_region();
    return 0;
}