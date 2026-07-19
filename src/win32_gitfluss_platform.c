#include "gitfluss.h"

#include <stdio.h>
#include <time.h>
#include <stdlib.h>

int64_t gfQueryTime
(
    void
){
    return time(0);
}
