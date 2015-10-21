#ifndef __H_60FEB5EA1C574093BF47109791645AE0__
#define __H_60FEB5EA1C574093BF47109791645AE0__

#if defined(DEBUG) && DEBUG > 0
        #include <stdio.h>
        #define DEBUG_PRINT(fmt, args...) \
                fprintf(stderr, "DEBUG: %s:%d:%s: " fmt, \
                     __FILE__, __LINE__, __func__, ##args)
#else
        #define DEBUG_PRINT(fmt, args...)
        #define NDEBUG
#endif

#include <assert.h>

#endif
