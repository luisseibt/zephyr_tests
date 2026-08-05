#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <stdlib.h>
#include <stdint.h>

#define USE_BUCKETS
#define MULTICORE_SIMDEV_CORE_DONE (*(volatile unsigned int *)(0x1C203000))
/* 32-Bit Cast Fix, um den %u Fehler zu vermeiden */
#define MULTICORE_SIM_DEV_GET_SIM_TIME ((uint32_t)(*(volatile uint64_t *)(0x1C203020)))

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

/* --- Zephyr SMP Threading Setup --- */
#define STACK_SIZE 4096
#define THREAD_PRIORITY 7

K_THREAD_STACK_DEFINE(core1_stack, STACK_SIZE);
struct k_thread core1_thread_data;

/* Lockstep Synchronisation */
K_SEM_DEFINE(core1_start_sem, 0, 1);
K_SEM_DEFINE(core1_done_sem, 0, 1);

/* TRICK 1: 2D Array für konfliktfreies Zählen (Core 0 = [0], Core 1 = [1]) */
INT_TYPE bucket_size[2][NUM_BUCKETS];
INT_TYPE global_bucket_ptrs[NUM_BUCKETS];

/* The large static arrays */
INT_TYPE key_array[SIZE_OF_BUFFERS],    
         key_buff1[MAX_KEY],    
         key_buff2[SIZE_OF_BUFFERS];

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

void create_seq(double seed, double a) {
    double x;
    int i, k = MAX_KEY / 4;
    for (i = 0; i < NUM_KEYS; i++) {
        x = randlc(&seed, &a);
        x += randlc(&seed, &a);
        x += randlc(&seed, &a);
        x += randlc(&seed, &a);  
        key_array[i] = k * x;
        if ((i % 4096) == 0) printk(".");
    }
    printk(" Init Done!\n");
}

/* Worker function running strictly on Core 1 */
void core1_worker_thread(void *arg1, void *arg2, void *arg3) {
    printk("DEBUG::The worker thread is executed on the core with following hartid: 0x%08X\n", arch_proc_id());
    int shift = MAX_KEY_LOG_2 - NUM_BUCKETS_LOG_2;
    int num_bucket_keys = (1 << shift);
    
    /* TRICK 2: Thread-Privater Pointer Array für den Scatter */
    INT_TYPE local_bucket_ptrs[NUM_BUCKETS];

    while (1) {
        /* ================= PHASE 1: COUNTING ================= */
        k_sem_take(&core1_start_sem, K_FOREVER);

        for (int i = 0; i < NUM_BUCKETS; i++) bucket_size[1][i] = 0;
        for (int i = NUM_KEYS / 2; i < NUM_KEYS; i++) {
            bucket_size[1][key_array[i] >> shift]++;
        }
        
        k_sem_give(&core1_done_sem);


        /* ================= PHASE 2/3: SCATTER ================= */
        k_sem_take(&core1_start_sem, K_FOREVER);

        /* Berechnet Core 1's exakte Start-Indizes */
        local_bucket_ptrs[0] = bucket_size[0][0];
        for (int i = 1; i < NUM_BUCKETS; i++) {
            local_bucket_ptrs[i] = local_bucket_ptrs[i-1] + bucket_size[1][i-1] + bucket_size[0][i];
        }

        for (int i = NUM_KEYS / 2; i < NUM_KEYS; i++) {
            INT_TYPE key = key_array[i];
            key_buff2[local_bucket_ptrs[key >> shift]++] = key;
        }

        k_sem_give(&core1_done_sem);


        /* ================= PHASE 4: FINAL RANK ================= */
        k_sem_take(&core1_start_sem, K_FOREVER);

        /* TRICK 3: Core 1 übernimmt die zweite Hälfte der BUCKETS (256 bis 511) */
        for (int i = NUM_BUCKETS / 2; i < NUM_BUCKETS; i++) {
            INT_TYPE k1 = i * num_bucket_keys;
            INT_TYPE k2 = k1 + num_bucket_keys;

            for (INT_TYPE k = k1; k < k2; k++) key_buff1[k] = 0;

            INT_TYPE m = (i > 0) ? global_bucket_ptrs[i-1] : 0;
            for (INT_TYPE k = m; k < global_bucket_ptrs[i]; k++) {
                key_buff1[key_buff2[k]]++;
            }

            key_buff1[k1] += m;
            for (INT_TYPE k = k1 + 1; k < k2; k++) {
                key_buff1[k] += key_buff1[k-1];
            }
        }

        k_sem_give(&core1_done_sem);
    }
}

void rank(int iteration) {
    printk("DEBUG::The function rank() is executed on the core with following hartid: 0x%08X\n", arch_proc_id());
    INT_TYPE i;
    int shift = MAX_KEY_LOG_2 - NUM_BUCKETS_LOG_2;
    int num_bucket_keys = (1 << shift);
    
    /* Thread-Privater Pointer für Core 0 */
    INT_TYPE local_bucket_ptrs[NUM_BUCKETS];

    key_array[iteration] = iteration;
    key_array[iteration + MAX_ITERATIONS] = MAX_KEY - iteration;

    /* ================= PHASE 1: COUNTING ================= */
    k_sem_give(&core1_start_sem); /* Wecke Core 1 */

    for (i = 0; i < NUM_BUCKETS; i++) bucket_size[0][i] = 0;
    for (i = 0; i < NUM_KEYS / 2; i++) {
        bucket_size[0][key_array[i] >> shift]++;
    }

    k_sem_take(&core1_done_sem, K_FOREVER); /* Warte auf Core 1 */


    /* ================= PHASE 2/3: SCATTER ================= */
    k_sem_give(&core1_start_sem);

    /* Core 0 Prefix Sum */
    local_bucket_ptrs[0] = 0;
    for (i = 1; i < NUM_BUCKETS; i++) {
        local_bucket_ptrs[i] = local_bucket_ptrs[i-1] + bucket_size[0][i-1] + bucket_size[1][i-1];
    }
    
    /* Globale Startpunkte für Phase 4 vorbereiten */
    global_bucket_ptrs[0] = bucket_size[0][0] + bucket_size[1][0];
    for (i = 1; i < NUM_BUCKETS; i++) {
        global_bucket_ptrs[i] = global_bucket_ptrs[i-1] + bucket_size[0][i] + bucket_size[1][i];
    }

    /* Scatter Core 0 */
    for (i = 0; i < NUM_KEYS / 2; i++) {
        INT_TYPE key = key_array[i];
        key_buff2[local_bucket_ptrs[key >> shift]++] = key;
    }

    k_sem_take(&core1_done_sem, K_FOREVER);


    /* ================= PHASE 4: FINAL RANK ================= */
    k_sem_give(&core1_start_sem);

    /* TRICK 3: Core 0 übernimmt die erste Hälfte der BUCKETS (0 bis 255) */
    for (i = 0; i < NUM_BUCKETS / 2; i++) {
        INT_TYPE k1 = i * num_bucket_keys;
        INT_TYPE k2 = k1 + num_bucket_keys;

        for (INT_TYPE k_idx = k1; k_idx < k2; k_idx++) key_buff1[k_idx] = 0;

        INT_TYPE m = (i > 0) ? global_bucket_ptrs[i-1] : 0;
        for (INT_TYPE k_idx = m; k_idx < global_bucket_ptrs[i]; k_idx++) {
            key_buff1[key_buff2[k_idx]]++;
        }

        key_buff1[k1] += m;
        for (INT_TYPE k_idx = k1 + 1; k_idx < k2; k_idx++) {
            key_buff1[k_idx] += key_buff1[k_idx-1];
        }
    }

    k_sem_take(&core1_done_sem, K_FOREVER);
    
    if(iteration == MAX_ITERATIONS) key_buff_ptr_global = key_buff1;
}

int main(void) {
    int iteration;
    printk("DEBUG::The function main() is executed on the core with following hartid: 0x%08X\n", arch_proc_id());

    printk("\n\n NAS Parallel Benchmarks (Zephyr SMP) - IS Benchmark\n");
    printk(" Size:  %d  (class %c)\n", TOTAL_KEYS, CLASS);
    printk(" Iterations:   %d\n", MAX_ITERATIONS);

    /* Core 1 Thread starten (läuft ab jetzt unendlich in der while-Schleife) */
    k_thread_create(&core1_thread_data, core1_stack,
                    K_THREAD_STACK_SIZEOF(core1_stack),
                    core1_worker_thread, NULL, NULL, NULL,
                    THREAD_PRIORITY, 0, K_NO_WAIT);

    create_seq(314159265.00, 1220703125.00);                 

    
    /* Untimed initial rank */
    rank(1);  
    printk("Initialization done, counting time simdev has time %u\n", MULTICORE_SIM_DEV_GET_SIM_TIME);
    
    
    for(iteration = 1; iteration <= MAX_ITERATIONS; iteration++) {
        printk("   Iteration %d\n", iteration);
        rank(iteration);
    }

    // printk("\n--- Komplettes Sorted Array ---\n");
    // for (int idx = 0; idx < NUM_KEYS; idx++) {
    //     /* Kuerzere Ausgabe, um die Zeilenanzahl/Zeit zu minimieren */
    //     printk("%d\n", key_array[idx]);
    // }
    // printk("-------------------------------\n");
    
    MULTICORE_SIMDEV_CORE_DONE = 1;
    MULTICORE_SIMDEV_CORE_DONE = 0;
    return 0;
}