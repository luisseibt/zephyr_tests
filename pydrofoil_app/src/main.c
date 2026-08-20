#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <stdlib.h>
#include <stdint.h>
/* --- HIER WIRD DAS ARRAY GELADEN --- */
#include "array_include.h"
/* ----------------------------------- */

#define USE_BUCKETS
#define MULTICORE_SIMDEV_CORE_DONE (*(volatile unsigned int *)(0x1C203000))
/* 32-Bit Cast Fix, um den %u Fehler zu vermeiden */
#define MULTICORE_SIM_DEV_GET_SIM_TIME ((uint32_t)(*(volatile uint64_t *)(0x1C203020)))

/*
 * Anzahl der Cores für die Parallelisierung.
 * Erlaubte Werte: 1, 2, 4, 8
 */
#define NUM_CORES 2

#if (NUM_CORES != 1) && (NUM_CORES != 2) && (NUM_CORES != 4) && (NUM_CORES != 8)
#error "NUM_CORES must be 1, 2, 4, or 8"
#endif

#if (NUM_KEYS % NUM_CORES) != 0
#error "NUM_KEYS must be divisible by NUM_CORES"
#endif

#if (NUM_BUCKETS % NUM_CORES) != 0
#error "NUM_BUCKETS must be divisible by NUM_CORES"
#endif

#define KEYS_PER_CORE    (NUM_KEYS / NUM_CORES)
#define BUCKETS_PER_CORE (NUM_BUCKETS / NUM_CORES)

int      passed_verification;

/* --- Zephyr SMP Threading Setup --- */
#define STACK_SIZE 4096
#define THREAD_PRIORITY 7

#if NUM_CORES > 1
K_THREAD_STACK_ARRAY_DEFINE(worker_stacks, NUM_CORES - 1, STACK_SIZE);
static struct k_thread worker_thread_data[NUM_CORES - 1];
static struct k_sem start_sem[NUM_CORES - 1];
static struct k_sem done_sem[NUM_CORES - 1];
#endif

/* TRICK 1: 2D Array für konfliktfreies Zählen (eine Zeile pro Core) */
INT_TYPE bucket_size[NUM_CORES][NUM_BUCKETS];
INT_TYPE global_bucket_ptrs[NUM_BUCKETS];
INT_TYPE local_bucket_ptrs[NUM_CORES][NUM_BUCKETS];

/* The large static arrays */
INT_TYPE key_buff1[MAX_KEY],    
         key_buff2[SIZE_OF_BUFFERS],
         partial_verify_vals[TEST_ARRAY_SIZE];

INT_TYPE test_index_array[TEST_ARRAY_SIZE],
         test_rank_array[TEST_ARRAY_SIZE],
         S_test_index_array[TEST_ARRAY_SIZE] = {48427,17148,23627,62548,4431},
         S_test_rank_array[TEST_ARRAY_SIZE]  = {0,18,346,64917,65463};

/* Portable random number generator */
double randlc(double *X, double *A) {
      static int KS = 0;
      static double R23, R46, T23, T46;
      double T1, T2, T3, T4, A1, A2, X1, X2, Z;
      int i, j;

      if (KS == 0) {
        R23 = 1.0; R46 = 1.0; T23 = 1.0; T46 = 1.0;
        for (i=1; i<=23; i++) { R23 = 0.50 * R23; T23 = 2.0 * T23; }
        for (i=1; i<=46; i++) { R46 = 0.50 * R46; T46 = 2.0 * T46; }
        KS = 1;
      }

      T1 = R23 * *A; j = T1; A1 = j; A2 = *A - T23 * A1;
      T1 = R23 * *X; j = T1; X1 = j; X2 = *X - T23 * X1;
      T1 = A1 * X2 + A2 * X1;
      
      j  = R23 * T1; T2 = j; Z = T1 - T23 * T2;
      T3 = T23 * Z + A2 * X2;
      j  = R46 * T3; T4 = j; *X = T3 - T46 * T4;
      return(R46 * *X);
} 

void print_hard_id(void) {
    uint32_t hard_id = arch_proc_id();
    printk("Executing on Hardware Core ID: 0x%08X\n", hard_id);
}

static void signal_workers_start(void) {
#if NUM_CORES > 1
    for (int c = 0; c < NUM_CORES - 1; c++) {
        k_sem_give(&start_sem[c]);
    }
#endif
}

static void wait_workers_done(void) {
#if NUM_CORES > 1
    for (int c = 0; c < NUM_CORES - 1; c++) {
        k_sem_take(&done_sem[c], K_FOREVER);
    }
#endif
}

/* Startindex von Core C in Bucket i (keys der Cores liegen pro Bucket hintereinander) */
static void compute_local_bucket_ptrs(int core_id) {
    INT_TYPE sum = 0;

    for (int c = 0; c < core_id; c++) {
        sum += bucket_size[c][0];
    }
    local_bucket_ptrs[core_id][0] = sum;

    for (int i = 1; i < NUM_BUCKETS; i++) {
        INT_TYPE add = 0;
        for (int c = core_id; c < NUM_CORES; c++) {
            add += bucket_size[c][i - 1];
        }
        for (int c = 0; c < core_id; c++) {
            add += bucket_size[c][i];
        }
        local_bucket_ptrs[core_id][i] = local_bucket_ptrs[core_id][i - 1] + add;
    }
}

static void compute_global_bucket_ptrs(void) {
    INT_TYPE sum = 0;

    for (int c = 0; c < NUM_CORES; c++) {
        sum += bucket_size[c][0];
    }
    global_bucket_ptrs[0] = sum;

    for (int i = 1; i < NUM_BUCKETS; i++) {
        sum = global_bucket_ptrs[i - 1];
        for (int c = 0; c < NUM_CORES; c++) {
            sum += bucket_size[c][i];
        }
        global_bucket_ptrs[i] = sum;
    }
}

static void count_keys(int core_id, int shift) {
    int key_start = core_id * KEYS_PER_CORE;
    int key_end = key_start + KEYS_PER_CORE;
    printk("DEBUG::Core %d, with hartid 0x%08X counting keys from %d to %d\n", core_id, arch_proc_id(), key_start, key_end - 1);

    for (int i = 0; i < NUM_BUCKETS; i++) {
        bucket_size[core_id][i] = 0;
    }
    for (int i = key_start; i < key_end; i++) {
        bucket_size[core_id][key_array[i] >> shift]++;
    }
}

static void scatter_keys(int core_id, int shift) {
    int key_start = core_id * KEYS_PER_CORE;
    int key_end = key_start + KEYS_PER_CORE;

    compute_local_bucket_ptrs(core_id);

    for (int i = key_start; i < key_end; i++) {
        INT_TYPE key = key_array[i];
        key_buff2[local_bucket_ptrs[core_id][key >> shift]++] = key;
    }
}

static void final_rank_buckets(int core_id, int num_bucket_keys) {
    int bucket_start = core_id * BUCKETS_PER_CORE;
    int bucket_end = bucket_start + BUCKETS_PER_CORE;

    for (int i = bucket_start; i < bucket_end; i++) {
        INT_TYPE k1 = i * num_bucket_keys;
        INT_TYPE k2 = k1 + num_bucket_keys;

        for (INT_TYPE k = k1; k < k2; k++) {
            key_buff1[k] = 0;
        }

        INT_TYPE m = (i > 0) ? global_bucket_ptrs[i - 1] : 0;
        for (INT_TYPE k = m; k < global_bucket_ptrs[i]; k++) {
            key_buff1[key_buff2[k]]++;
        }

        key_buff1[k1] += m;
        for (INT_TYPE k = k1 + 1; k < k2; k++) {
            key_buff1[k] += key_buff1[k - 1];
        }
    }
}

#if NUM_CORES > 1
/* Worker-Thread für Core 1 .. NUM_CORES-1 */
void worker_thread(void *arg1, void *arg2, void *arg3) {
    int core_id = (int)(uintptr_t)arg1;
    int sem_idx = core_id - 1;
    int shift = MAX_KEY_LOG_2 - NUM_BUCKETS_LOG_2;
    int num_bucket_keys = (1 << shift);

    printk("DEBUG::Worker core %d running on hartid: 0x%08X\n",
           core_id, arch_proc_id());

    while (1) {
        /* ================= PHASE 1: COUNTING ================= */
        k_sem_take(&start_sem[sem_idx], K_FOREVER);
        count_keys(core_id, shift);
        k_sem_give(&done_sem[sem_idx]);

        /* ================= PHASE 2/3: SCATTER ================= */
        k_sem_take(&start_sem[sem_idx], K_FOREVER);
        scatter_keys(core_id, shift);
        k_sem_give(&done_sem[sem_idx]);

        /* ================= PHASE 4: FINAL RANK ================= */
        k_sem_take(&start_sem[sem_idx], K_FOREVER);
        final_rank_buckets(core_id, num_bucket_keys);
        k_sem_give(&done_sem[sem_idx]);
    }
}
#endif

void rank(int iteration) {
    printk("DEBUG::The function rank() is executed on the core with following hartid: 0x%08X\n", arch_proc_id());
    INT_TYPE i, k;
    int shift = MAX_KEY_LOG_2 - NUM_BUCKETS_LOG_2;
    int num_bucket_keys = (1 << shift);

    key_array[iteration] = iteration;
    key_array[iteration + MAX_ITERATIONS] = MAX_KEY - iteration;

    for(i = 0; i < TEST_ARRAY_SIZE; i++){
        partial_verify_vals[i] = key_array[test_index_array[i]];
    }

    /* ================= PHASE 1: COUNTING ================= */
    signal_workers_start();
    count_keys(0, shift);
    wait_workers_done();

    /* ================= PHASE 2/3: SCATTER ================= */
    signal_workers_start();
    compute_global_bucket_ptrs();
    scatter_keys(0, shift);
    wait_workers_done();

    /* ================= PHASE 4: FINAL RANK ================= */
    signal_workers_start();
    final_rank_buckets(0, num_bucket_keys);
    wait_workers_done();

    /* ================= VERIFICATION ================= */
    for(i = 0; i < TEST_ARRAY_SIZE; i++) {   
        k = partial_verify_vals[i];          
        if(0 < k && k <= NUM_KEYS-1) {
            INT_TYPE key_rank = key_buff1[k-1];
            int failed = 0;
            if(i <= 2) {
                if(key_rank != test_rank_array[i] + iteration) failed = 1;
                else passed_verification++;
            } else {
                if(key_rank != test_rank_array[i] - iteration) failed = 1;
                else passed_verification++;
            }
        }
    }
    
    if(iteration == MAX_ITERATIONS) key_buff_ptr_global = key_buff1;
}      


void full_verify(void) {
    INT_TYPE i, j;
    for(i = 0; i < NUM_KEYS; i++) {
        key_array[--key_buff_ptr_global[key_buff2[i]]] = key_buff2[i];
    }
    
    j = 0;
    for(i = 1; i < NUM_KEYS; i++) {
        if(key_array[i-1] > key_array[i]) j++;
    }

    if(j != 0) printk("Full_verify: keys out of sort: %d\n", j);
    else passed_verification++;
}


int main(void) {
    int i, iteration;
    uint32_t start_time, end_time;
    printk("DEBUG::The function main() is executed on the core with following hartid: 0x%08X\n", arch_proc_id());

    for(i = 0; i < TEST_ARRAY_SIZE; i++) {
        test_index_array[i] = S_test_index_array[i];
        test_rank_array[i]  = S_test_rank_array[i];
    }

    printk("\n\n NAS Parallel Benchmarks (Zephyr SMP) - IS Benchmark\n");
    printk(" Size:  %d  (class %c)\n", TOTAL_KEYS, CLASS);
    printk(" Iterations:   %d\n", MAX_ITERATIONS);
    printk(" Cores:        %d\n", NUM_CORES);

#if NUM_CORES > 1
    for (int c = 0; c < NUM_CORES - 1; c++) {
        k_sem_init(&start_sem[c], 0, 1);
        k_sem_init(&done_sem[c], 0, 1);
    }

    for (int c = 1; c < NUM_CORES; c++) {
        k_thread_create(&worker_thread_data[c - 1], worker_stacks[c - 1],
                        K_THREAD_STACK_SIZEOF(worker_stacks[c - 1]),
                        worker_thread, (void *)(uintptr_t)c, NULL, NULL,
                        THREAD_PRIORITY, 0, K_NO_WAIT);
    }
#endif

    // create_seq(314159265.00, 1220703125.00);                 

    passed_verification = 0;
    
    /* Untimed initial rank */
    rank(1);  
    printk("Initialization done, counting time\n\n");
    
    start_time = MULTICORE_SIM_DEV_GET_SIM_TIME;
    
    for(iteration = 1; iteration <= MAX_ITERATIONS; iteration++) {
        printk("   Iteration %d\n", iteration);
        rank(iteration);
    }

    full_verify();

    printk("\n===================================\n");
    printk("===================================\n");
    MULTICORE_SIMDEV_CORE_DONE = 1;
    MULTICORE_SIMDEV_CORE_DONE = 0;

    printk("\n--- Komplettes Sorted Array ---\n");
    for (int idx = 0; idx < NUM_KEYS; idx++) {
        /* Kuerzere Ausgabe, um die Zeilenanzahl/Zeit zu minimieren */
        printk("%d\n", key_array[idx]);
    }
    printk("-------------------------------\n");
    
    return 0;
}
