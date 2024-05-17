#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "nand_geometry.h"
#include "bad_map.h"
#include "bbm.h"

int main(void) {
    srand(time(NULL));
    nand_geometry_init();
    bad_block_init();
    bad_block_mgr_init();
    bad_block_mgr_build();
    uint16_t remap_block[16];

    bad_block_find_remap(1000, 2, remap_block);
    for (uint32_t i = 0; i < 4; i++) {
        printf("%d ", remap_block[i]);
    }
    printf("\n");
    bad_block_find_remap(997, 0, remap_block);
    for (uint32_t i = 0; i < 4; i++) {
        printf("%d ", remap_block[i]);
    }
    printf("\n");
    bad_block_find_remap(956, 0, remap_block);
    for (uint32_t i = 0; i < 4; i++) {
        printf("%d ", remap_block[i]);
    }
    printf("\n");
    bad_block_find_remap(986, 0, remap_block);
    for (uint32_t i = 0; i < 4; i++) {
        printf("%d ", remap_block[i]);
    }
    printf("\n");

    return 0;
}
