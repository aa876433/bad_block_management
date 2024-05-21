#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "nand_geometry.h"
#include "bad_map.h"
#include "bbm.h"
#include "config.h"

int min_plane = 0;

int main(void) {
    srand(time(NULL));
    nand_geometry_init();
    bad_block_init();
    bad_block_mgr_init();
    bad_block_mgr_build();
    uint16_t remap_block[16];
    uint8_t err_plane[INTERLEAVE] = {0};

    printf("min_plane %d\n", min_plane);
    err_plane[min_plane] = 1;
    uint32_t success = bad_block_run_replace(50, err_plane);
    printf("success %d\n", success);

    uint16_t phy_max = bad_block_get_max();
    for (uint32_t block = 0; block < phy_max; block++) {
        for (uint32_t die = 0; die < DIE; die++) {
            bad_block_find_remap(block, die, remap_block);
        }
    }
    return 0;
}
