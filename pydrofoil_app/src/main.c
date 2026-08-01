#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#define STACK_SIZE 1024
#define THREAD_PRIORITY 7

/* 1. We must manually allocate memory for the new thread's stack */
K_THREAD_STACK_DEFINE(my_stack_area, STACK_SIZE);
struct k_thread my_thread_data;

/* 2. The function our second thread will run */
void secondary_thread_function(void *arg1, void *arg2, void *arg3)
{
    for(int i = 0; i < 10; i++) {
        printk("1\n");
        k_busy_wait(100);
    }
}

/* 3. The Main function (Core 0) */
int main(void)
{
    printk("Starting Multicore SMP Test...\n");

    /* THIS IS WHAT YOU EXPECTED: 
     * Explicitly telling the OS to create and start the second thread! 
     */
    k_thread_create(&my_thread_data, my_stack_area,
                    K_THREAD_STACK_SIZEOF(my_stack_area),
                    secondary_thread_function,
                    NULL, NULL, NULL,
                    THREAD_PRIORITY, 0, K_NO_WAIT);

    /* Main thread loop */
    for(int i = 0; i < 10; i++) {
        printk("0\n");
        k_busy_wait(100);
    }
    return 0;
}








