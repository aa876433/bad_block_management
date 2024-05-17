//
// Created by JohnLin on 2024/5/14.
//

#include "config.h"
#include "bbm.h"
#include "bad_map.h"
#include "nand_geometry.h"
#include "util.h"
#include <windows.h>

#define BBM_SIGNATURE 0xBBA5BEEF
#define BBM_PATCH_MASK (32768)

typedef struct remap_group_t {
    uint16_t wPtr;
    uint16_t block[1];
} REMAP_GROUP_T;

typedef struct remap_node_t {
    uint16_t block: 13;
    uint16_t used: 1;
    uint16_t rsvd: 2;
} REMAP_NODE_T;

typedef struct bb_mgr {
    uint32_t signature;
    uint32_t buf_size;
    uint16_t remap_offset;
    uint16_t remap_index;
    uint16_t *remap_phy_block;
    uint16_t *remap_vir_block;
    REMAP_NODE_T *remap_table;
} BB_MGR;

BB_MGR *g_bbm;

void bad_block_mgr_init(void) {
    NAND_GEOMETRY_T *geo = nand_geometry_get();
    uint32_t buf_size = sizeof(struct bb_mgr);
    uint32_t reamp_block_size = ROUND_UP((geo->block - RESERVED_BLOCK) * sizeof(uint16_t), 4);
    uint32_t remap_table_size = ROUND_UP(geo->interleave * REMAP_TABLE_MAX * sizeof(REMAP_NODE_T), 4);
    buf_size += (reamp_block_size << 1) + remap_table_size;
    g_bbm = (BB_MGR *) malloc(buf_size);
    memset(g_bbm, 0x00, buf_size);
    g_bbm->signature = BBM_SIGNATURE;
    g_bbm->buf_size = buf_size;
    g_bbm->remap_offset = geo->interleave * sizeof(REMAP_NODE_T);
    g_bbm->remap_phy_block = PTR_ADD_OFFSET(g_bbm, sizeof(struct bb_mgr));
    g_bbm->remap_vir_block = PTR_ADD_OFFSET(g_bbm->remap_phy_block, reamp_block_size);
    g_bbm->remap_table = PTR_ADD_OFFSET(g_bbm->remap_vir_block, reamp_block_size);
}

void bad_block_mgr_build(void) {
    ASSERT(g_bbm);
    NAND_GEOMETRY_T *geo = nand_geometry_get();
    uint8_t *bad_map = bad_block_map_get();
    uint32_t is_good_block, map_index, map_offset;
    uint32_t index = RESERVED_BLOCK * geo->interleave;
    uint32_t temp;
    REMAP_GROUP_T *remap_group;
    uint32_t grp_size = ROUND_UP(sizeof(uint16_t) * (REMAP_TABLE_MAX + 1), 4);
    void *remap_grp_base = malloc(grp_size * geo->interleave);
    memset(remap_grp_base, 0x00, grp_size * geo->interleave);

    for (uint32_t block = RESERVED_BLOCK; block < geo->block; block++) {
        is_good_block = 1;
        temp = index;
        for (uint32_t j = 0; j < geo->interleave; j++, index++) {
            map_index = index >> 1;
            map_offset = (index & 1) << 2;
            if (bad_map[map_index] & (BAD_BLOCK_BIT << map_offset)) {
                is_good_block = 0;
                goto COLLECT;
            }
        }

    COLLECT:
        if (is_good_block) {
            g_bbm->remap_phy_block[g_bbm->remap_index++] = block;
        } else {
            index = temp;
            remap_group = remap_grp_base;
            for (uint32_t j = 0; j < geo->interleave; j++, index++) {
                if (remap_group->wPtr >= REMAP_TABLE_MAX) {
                    continue;
                }
                map_index = index >> 1;
                map_offset = (index & 1) << 2;
                if (!(bad_map[map_index] & (BAD_BLOCK_BIT << map_offset))) {
                    remap_group->block[remap_group->wPtr++] = block;
                }
                remap_group = PTR_ADD_OFFSET(remap_group, grp_size);
            }
        }
    }

    remap_group = remap_grp_base;
    uint16_t min_group_count = 0xFFFF;
    for (uint32_t i = 0; i < geo->interleave; i++) {
        if (remap_group->wPtr < min_group_count) {
            min_group_count = remap_group->wPtr;
        }
        remap_group = PTR_ADD_OFFSET(remap_group, grp_size);
    }

    printf("remap_index %d\n", g_bbm->remap_index);
    REMAP_NODE_T *remap_node = g_bbm->remap_table;
    uint16_t block;
    for (uint32_t i = 0; i < min_group_count; i++) {
        remap_group = remap_grp_base;
        ASSERT(!remap_node->used);
        for (uint32_t j = 0; j < geo->interleave; j++) {
            ASSERT(remap_group->wPtr > 0);
            block = remap_group->block[--remap_group->wPtr];
            index = block * geo->interleave + j;
            map_index = index >> 1;
            map_offset = (index & 1) << 2;
            ASSERT(block < BLOCK);
            ASSERT(!(bad_map[map_index] & (BAD_BLOCK_BIT << map_offset)));
            bad_map[map_index] |= (PATCH_BLOCK_BIT << map_offset);
            remap_node[j].used = 1;
            remap_node[j].block = block;
            remap_group = PTR_ADD_OFFSET(remap_group, grp_size);
        }
        g_bbm->remap_phy_block[g_bbm->remap_index++] = BBM_PATCH_MASK | i;
        remap_node = PTR_ADD_OFFSET(remap_node, g_bbm->remap_offset);
    }

    for (uint32_t i = 0; i < (geo->block - RESERVED_BLOCK); i++) {
        g_bbm->remap_vir_block[i] = i;
    }

    printf("remap_index %d\n", g_bbm->remap_index);
    free(remap_grp_base);
}

void bad_block_find_remap(uint16_t log_block, uint16_t log_die, uint16_t *remap_block) {
    NAND_GEOMETRY_T *geo = nand_geometry_get();
    uint16_t vir = g_bbm->remap_vir_block[log_block];
    uint16_t pb = g_bbm->remap_phy_block[vir];
    uint16_t block, map_index, map_offset;
    uint32_t index;
    uint32_t r_size;
    uint8_t *bad_map = bad_block_map_get();

    if (pb & BBM_PATCH_MASK) {
        uint32_t table_index = pb & (~BBM_PATCH_MASK);
        ASSERT(table_index < REMAP_TABLE_MAX);
        REMAP_NODE_T *remap_node = PTR_ADD_OFFSET(g_bbm->remap_table, table_index * g_bbm->remap_offset);
        ASSERT(remap_node->used);
        uint32_t start = log_die * geo->plane;
        for (uint32_t i = 0; i < geo->plane; i++) {
            block = remap_node[start + i].block;
            index = block * geo->interleave + start + i;
            map_index = index >> 1;
            map_offset = (index & 1) << 2;
            ASSERT(bad_map[map_index] & (PATCH_BLOCK_BIT << map_offset));
            remap_block[i] = remap_node[start + i].block;
            ASSERT(remap_block[i] < geo->block);
        }
    } else {
        for (uint32_t i = 0; i < geo->plane; i++) {
            remap_block[i] = pb;
        }
    }
}
