//
// Created by JohnLin on 2024/5/14.
//

#ifndef UTIL_H
#define UTIL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>

#define ASSERT assert
#define MIN(x, y) ((x) < (y)) ? (x) : (y)
#define MAX(x, y) ((x) > (y)) ? (x) : (y)

#define ROUND_UP(x, y) ((((x) + (y) - 1) / (y)) * (y))
#define ROUND_DOWN(x, y) (((x) / (y)) * (y))
#define ROUND_UP_DIV(x, y) (((x) + (y) - 1) / (y))

#define PTR_ADD_OFFSET(ptr, offset) ((void*)((char*)(ptr) + (offset)))
#define GEN_RANDOM() ((rand() & 0xFFFF) | (rand() << 16))


#endif //UTIL_H
