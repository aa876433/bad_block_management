//
// Created by JohnLin on 2024/5/14.
//

#ifndef BBM_H
#define BBM_H

#include <stdint.h>

void bad_block_mgr_init(void);
void bad_block_mgr_build(void);
void bad_block_find_remap(uint16_t log_block, uint16_t log_die, uint16_t *remap_block);
uint32_t bad_block_run_replace(uint16_t log_block, uint8_t *error_plane);
uint16_t bad_block_get_max(void);

#endif //BBM_H
