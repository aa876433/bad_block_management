//
// Created by JohnLin on 2024/5/14.
//

#ifndef NAND_GEOMETRY_H
#define NAND_GEOMETRY_H

#include <stdint.h>

typedef struct {
    uint32_t plane      : 3;
    uint32_t die        : 6;
    uint32_t block      : 12;
    uint32_t wl         : 11;
    uint32_t interleave : 8;
    uint32_t rsvd       : 24;
} NAND_GEOMETRY_T;

void nand_geometry_init(void);
void *nand_geometry_get(void);

#endif //NAND_GEOMETRY_H
