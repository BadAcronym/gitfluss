#include "gitfluss.h"

#include <time.h>

int64_t gfQueryTime
(
    void
){
    return time(0);
}
