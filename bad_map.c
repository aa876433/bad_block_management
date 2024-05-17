//
// Created by JohnLin on 2024/5/14.
//

#include "config.h"
#include "bad_map.h"
#include "util.h"
#include "nand_geometry.h"

uint8_t *g_bad_map;
uint32_t g_size;

uint32_t bad_block_gen(void) {
    return (GEN_RANDOM() % 1000) < 6;
}

void bad_block_init(void)
{
    NAND_GEOMETRY_T *geo = nand_geometry_get();
    uint32_t die = geo->die;
    uint32_t plane = geo->plane;
    uint32_t block = geo->block;
    uint32_t interleave = geo->interleave;
    uint32_t size = ROUND_UP(block * interleave * 4, 8) / 8;
    uint32_t index = 0;
    uint32_t map_index, map_offset;

    g_bad_map = malloc(size);
    memset(g_bad_map, 0x00, size);
    g_size = size;

    for (uint32_t i = 0; i < block; i++) {
        for (uint32_t j = 0; j < die; j++) {
            for (uint32_t k = 0; k < plane; k++) {
                if (bad_block_gen()) {
                    map_index  = index >> 1;
                    map_offset = (index & 1) << 2;
                    g_bad_map[map_index] |= BAD_BLOCK_BIT << map_offset;
                    // printf("b %d, d %d, p %d\n", i, j, k);
                }
                index++;
            }
        }
    }

    // printf("\n\n");
    // uint32_t b, d, p, s;
    // for (uint32_t i = 0; i < size; i++) {
    //
    //     if (g_bad_map[i] & 0x08) {
    //         s = 2 * i;
    //         b = s / INTERLEAVE;
    //         d = (s % INTERLEAVE) / PLANE;
    //         p = s % PLANE;
    //         printf("b %d, d %d, p %d\n", b, d, p);
    //     }
    //
    //     if (g_bad_map[i] & 0x80) {
    //         s = 2 * i + 1;
    //         b = s / INTERLEAVE;
    //         d = (s % INTERLEAVE) / PLANE;
    //         p = s % PLANE;
    //         printf("b %d, d %d, p %d\n", b, d, p);
    //     }
    //
    // }
    //
    // printf("\n");
}

void* bad_block_map_get(void)
{
    return (void *)g_bad_map;
}

uint8_t is_bad_block(uint32_t block, uint32_t die, uint32_t plane) {

}