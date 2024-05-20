//
// Created by JohnLin on 2024/5/14.
//

#ifndef BAD_MAP_H
#define BAD_MAP_H

#include <stdint.h>

#define BAD_BLOCK_BIT (0x8)
#define REMAP_BLOCK_BIT (0x4)
#define BAD_MAP_INDEX(x) ((x) >> 1)
#define BAD_MAP_OFFSET(x) (((x) & 1) << 2)

void bad_block_init(void);
void* bad_block_map_get(void);

#endif //BAD_MAP_H
