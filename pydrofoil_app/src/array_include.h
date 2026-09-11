#ifndef ARRAY_INCLUDE_H
#define ARRAY_INCLUDE_H
typedef int INT_TYPE;

#define CLASS 'S'



#if CLASS == 'M'
    #include "array_class_m.h"
#elif CLASS == 'S'
    #include "array_class_s.h"
#elif CLASS == 'A'
    #include "array_class_a.h"
#elif CLASS == 'P'
    #include "array_class_p.h"
#elif CLASS == 'W'
    #include "array_class_w.h"
#else
    #error "Invalid or undefined CLASS"
#endif


#define  TOTAL_KEYS          (1 << TOTAL_KEYS_LOG_2)
#define  MAX_KEY             (1 << MAX_KEY_LOG_2)
#define  NUM_BUCKETS         (1 << NUM_BUCKETS_LOG_2)
#define  NUM_KEYS            TOTAL_KEYS
#define  SIZE_OF_BUFFERS     NUM_KEYS  

#define  MAX_ITERATIONS      1
#define  TEST_ARRAY_SIZE     5

extern INT_TYPE *key_buff_ptr_global;
#endif
