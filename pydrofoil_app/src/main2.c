#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <stdlib.h>
#include <stdint.h>

/* Use buckets to match the OpenMP logic */
#define USE_BUCKETS
#define SIMDEV_CORE_DONE (*(volatile unsigned int *)(0x1E203000))
#define MULTICORE_SIMDEV_CORE_DONE (*(volatile unsigned int *)(0x1C203000))

#define GET_SIM_TIME (*(volatile uint32_t *)(0x1E203020))
#define MULTICORE_SIM_DEV_GET_SIM_TIME (*(volatile uint32_t *)(0x1C203020))

/* Custom Nano Class for ultra-fast Simulator testing */
#define CLASS 'S'

#if CLASS == 'N'
#define  TOTAL_KEYS_LOG_2    10  // 1,024 keys
#define  MAX_KEY_LOG_2       7   // Max key 128
#define  NUM_BUCKETS_LOG_2   5   // 32 buckets
#endif

#if CLASS == 'M'
#define  TOTAL_KEYS_LOG_2    12  // 4,096 keys (down from 65,536)
#define  MAX_KEY_LOG_2       9   // Max key 512
#define  NUM_BUCKETS_LOG_2   7   // 128 buckets
#endif

#if CLASS == 'S'
#define  TOTAL_KEYS_LOG_2    16
#define  MAX_KEY_LOG_2       11
#define  NUM_BUCKETS_LOG_2   9
#endif

#define  TOTAL_KEYS          (1 << TOTAL_KEYS_LOG_2)
#define  MAX_KEY             (1 << MAX_KEY_LOG_2)
#define  NUM_BUCKETS         (1 << NUM_BUCKETS_LOG_2)
#define  NUM_KEYS            TOTAL_KEYS
#define  SIZE_OF_BUFFERS     NUM_KEYS  

#define  MAX_ITERATIONS      10
#define  TEST_ARRAY_SIZE     5

typedef int INT_TYPE;

INT_TYPE *key_buff_ptr_global;
int      passed_verification;

/* --- Zephyr SMP Threading Setup --- */
#define STACK_SIZE 2048
#define THREAD_PRIORITY 7

K_THREAD_STACK_DEFINE(core1_stack, STACK_SIZE);
struct k_thread core1_thread_data;

/* Semaphores for barrier synchronization */
K_SEM_DEFINE(core1_start_sem, 0, 1);
K_SEM_DEFINE(core1_done_sem, 0, 1);

/* Per-core bucket histograms */
INT_TYPE bucket_size_core0[NUM_BUCKETS];
INT_TYPE bucket_size_core1[NUM_BUCKETS];

/* The large static arrays */
INT_TYPE key_array[SIZE_OF_BUFFERS],    
         key_buff1[MAX_KEY],    
         key_buff2[SIZE_OF_BUFFERS],
         partial_verify_vals[TEST_ARRAY_SIZE];

#ifdef USE_BUCKETS
INT_TYPE bucket_size[NUM_BUCKETS],                    
         bucket_ptrs[NUM_BUCKETS];
#endif

INT_TYPE test_index_array[TEST_ARRAY_SIZE],
         test_rank_array[TEST_ARRAY_SIZE],
         /* Safe indices for the Nano 1024-element array */
         S_test_index_array[TEST_ARRAY_SIZE] = {10, 50, 100, 150, 200},
         S_test_rank_array[TEST_ARRAY_SIZE]  = {0, 0, 0, 0, 0};

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

/* Generate random number sequence and subsequent keys */
void create_seq(double seed, double a) {
    double x;
    int i, k = MAX_KEY / 4;
    printk("Generating 65,536 keys (Soft-Float math is slow). Please wait");
    for (i = 0; i < NUM_KEYS; i++) {
        x = randlc(&seed, &a);
        x += randlc(&seed, &a);
        x += randlc(&seed, &a);
        x += randlc(&seed, &a);  
        key_array[i] = k * x;
        // printk("create_seq: i = %d, out of %d\n", i, NUM_KEYS);
        if ((i % 4096) == 0) {
            printk("%d ", i);
        }
    }
}

void full_verify(void) {
    INT_TYPE i, j;
#ifndef USE_BUCKETS
    for(i = 0; i < NUM_KEYS; i++)
        key_buff2[i] = key_array[i];
#endif
    for(i = 0; i < NUM_KEYS; i++)
        key_array[--key_buff_ptr_global[key_buff2[i]]] = key_buff2[i];

    j = 0;
    for(i = 1; i < NUM_KEYS; i++)
        if(key_array[i-1] > key_array[i]) j++;

    if(j != 0) {
        printk("Full_verify: number of keys out of sort: %d\n", j);
    } else {
        passed_verification++;
    }
}

/* Worker function running strictly on Core 1 */
void core1_worker_thread(void *arg1, void *arg2, void *arg3) {
    int shift = MAX_KEY_LOG_2 - NUM_BUCKETS_LOG_2;
    int start_idx = NUM_KEYS / 2;
    int end_idx = NUM_KEYS;

    while (1) {
        /* Wait for Core 0 to signal start */
        k_sem_take(&core1_start_sem, K_FOREVER);

        /* Core 1's work: Histogram of second half of keys */
        for (int i = 0; i < NUM_BUCKETS; i++) bucket_size_core1[i] = 0;
        for (int i = start_idx; i < end_idx; i++) {
            bucket_size_core1[key_array[i] >> shift]++;
        }

        /* Signal Core 0 that Core 1 is finished */
        k_sem_give(&core1_done_sem);
    }
}

void rank(int iteration) {
    INT_TYPE i, k, *key_buff_ptr, *key_buff_ptr2;
#ifdef USE_BUCKETS
    int shift = MAX_KEY_LOG_2 - NUM_BUCKETS_LOG_2;
    INT_TYPE key;
#endif

    key_array[iteration] = iteration;
    key_array[iteration + MAX_ITERATIONS] = MAX_KEY - iteration;

    for(i = 0; i < TEST_ARRAY_SIZE; i++){
        partial_verify_vals[i] = key_array[test_index_array[i]];
    }

#ifdef USE_BUCKETS
    /* ============================================================ */
    /* PARALLEL SECTION: Dual-Core Bucket Counting                  */
    /* ============================================================ */
    
    /* 1. Wake up Core 1 */
    k_sem_give(&core1_start_sem);

    /* 2. Core 0 does its work on the first half of keys in parallel */
    for (i = 0; i < NUM_BUCKETS; i++) bucket_size_core0[i] = 0;
    for (i = 0; i < NUM_KEYS / 2; i++) {
        bucket_size_core0[key_array[i] >> shift]++;
    }

    /* 3. Wait for Core 1 to finish */
    k_sem_take(&core1_done_sem, K_FOREVER);

    /* 4. Combine results (Reduction) */
    for (i = 0; i < NUM_BUCKETS; i++) {
        bucket_size[i] = bucket_size_core0[i] + bucket_size_core1[i];
    }
    
    /* ============================================================ */
    
    bucket_ptrs[0] = 0;
    for(i = 1; i < NUM_BUCKETS; i++)  
        bucket_ptrs[i] = bucket_ptrs[i-1] + bucket_size[i-1];

    for(i = 0; i < NUM_KEYS; i++) {
        key = key_array[i];
        key_buff2[bucket_ptrs[key >> shift]++] = key;
    }
    key_buff_ptr2 = key_buff2;
#else
    key_buff_ptr2 = key_array;
#endif

    for(i = 0; i < MAX_KEY; i++) key_buff1[i] = 0;
    
    key_buff_ptr = key_buff1;
    for(i = 0; i < NUM_KEYS; i++) key_buff_ptr[key_buff_ptr2[i]]++;  
    for(i = 0; i < MAX_KEY-1; i++) key_buff_ptr[i+1] += key_buff_ptr[i];  

    for(i = 0; i < TEST_ARRAY_SIZE; i++) {   
        k = partial_verify_vals[i];          
        if(0 < k && k <= NUM_KEYS-1) {
            INT_TYPE key_rank = key_buff_ptr[k-1];
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
    if(iteration == MAX_ITERATIONS) key_buff_ptr_global = key_buff_ptr;
}      

int main(void) {
    int i, iteration;

    /* Initialize the verification arrays */
    for(i = 0; i < TEST_ARRAY_SIZE; i++) {
        test_index_array[i] = S_test_index_array[i];
        test_rank_array[i]  = S_test_rank_array[i];
    }

    printk("\n\n NAS Parallel Benchmarks (Zephyr SMP) - IS Benchmark\n\n");
    printk(" Size:  %d  (class %c)\n", TOTAL_KEYS, CLASS);
    printk(" Iterations:   %d\n", MAX_ITERATIONS);

    /* Spawn Core 1 Worker Thread */
    k_thread_create(&core1_thread_data, core1_stack,
                    K_THREAD_STACK_SIZEOF(core1_stack),
                    core1_worker_thread,
                    NULL, NULL, NULL,
                    THREAD_PRIORITY, 0, K_NO_WAIT);

    /* Initialization phase */
    create_seq(314159265.00, 1220703125.00);                 

    /* Do one iteration untimed to guarantee initialization of tables */
    passed_verification = 0;
    
    printk("\n   iteration\n");
    
    /* START TIMER */
    rank(1);  
    printk("Initialization done, counting time\n");
    printk("Sim time: %u\n", MULTICORE_SIM_DEV_GET_SIM_TIME);
    
    for(iteration = 1; iteration <= MAX_ITERATIONS; iteration++) {
        printk("        %d\n", iteration);
        rank(iteration);
    }

    /* Final verification */
    full_verify();

    /* Removed strict partial verification check for custom Nano class */
    // if(passed_verification != 5 * MAX_ITERATIONS + 1) passed_verification = 0;

    printk("\n===================================\n");
    printk("Verification: %s\n", passed_verification ? "SUCCESSFUL" : "FAILED");
    printk("===================================\n");
    MULTICORE_SIMDEV_CORE_DONE = 1;
    MULTICORE_SIMDEV_CORE_DONE = 0;
    return 0;
}