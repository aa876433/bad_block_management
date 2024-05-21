//
// Created by JohnLin on 2024/5/14.
//

#include "config.h"
#include "bbm.h"
#include "bad_map.h"
#include "nand_geometry.h"
#include "util.h"

#define BBM_SIGNATURE 0xBBA5BEEF
#define BBM_REMAP_MASK (32768)
#define BBM_IS_REMAP(pb) (pb & BBM_REMAP_MASK)
#define BBM_REAMP_INDEX(pb) (pb & ~BBM_REMAP_MASK)

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
    uint16_t remap_phy_index;
    uint16_t remap_table_index;
    uint16_t rsvd;
    uint16_t *remap_phy_block;
    uint16_t *remap_vir_block;
    REMAP_NODE_T *remap_table;
    REMAP_GROUP_T *remap_group;
} BB_MGR;

BB_MGR *g_bbm;

void bad_block_remap_table_build(void *remap_grp_base, uint32_t grp_size);

void bad_block_mgr_init(void) {
    NAND_GEOMETRY_T *geo = nand_geometry_get();
    uint32_t buf_size = sizeof(struct bb_mgr);
    uint32_t reamp_block_size = ROUND_UP((geo->block - RESERVED_BLOCK) * sizeof(uint16_t), 4);
    uint32_t remap_table_size = ROUND_UP(geo->interleave * REMAP_TABLE_MAX * sizeof(REMAP_NODE_T), 4);
    uint32_t remap_grp_size = ROUND_UP(geo->interleave * sizeof(uint16_t) * (REMAP_GROUP_MAX + 1), 4);
    buf_size += (reamp_block_size << 1) + remap_table_size + remap_grp_size;
    g_bbm = (BB_MGR *) malloc(buf_size);
    memset(g_bbm, 0x00, buf_size);
    g_bbm->signature = BBM_SIGNATURE;
    g_bbm->buf_size = buf_size;
    g_bbm->remap_offset = geo->interleave * sizeof(REMAP_NODE_T);
    g_bbm->remap_phy_block = PTR_ADD_OFFSET(g_bbm, sizeof(struct bb_mgr));
    g_bbm->remap_vir_block = PTR_ADD_OFFSET(g_bbm->remap_phy_block, reamp_block_size);
    g_bbm->remap_table = PTR_ADD_OFFSET(g_bbm->remap_vir_block, reamp_block_size);
    g_bbm->remap_group = PTR_ADD_OFFSET(g_bbm->remap_table, remap_table_size);
}

void bad_block_mgr_build(void) {
    ASSERT(g_bbm);
    NAND_GEOMETRY_T *geo = nand_geometry_get();
    uint8_t *bad_map = bad_block_map_get();
    uint32_t is_good_block, map_index, map_offset;
    uint32_t index = RESERVED_BLOCK * geo->interleave;
    REMAP_GROUP_T *remap_group;
    uint32_t temp;

    uint32_t grp_size = ROUND_UP(sizeof(uint16_t) * (REMAP_TABLE_MAX + 1), 4);
    void *remap_grp_base = malloc(grp_size * geo->interleave);
    memset(remap_grp_base, 0x00, grp_size * geo->interleave);

    for (uint32_t block = RESERVED_BLOCK; block < geo->block; block++) {
        is_good_block = 1;
        temp = index;
        ASSERT(index % geo->interleave == 0);
        for (uint32_t j = 0; j < geo->interleave; j++, index++) {
            map_index = BAD_MAP_INDEX(index);
            map_offset = BAD_MAP_OFFSET(index);
            if (bad_map[map_index] & (BAD_BLOCK_BIT << map_offset)) {
                is_good_block = 0;
                goto COLLECT;
            }
        }

    COLLECT:
        if (is_good_block) {
            g_bbm->remap_phy_block[g_bbm->remap_phy_index++] = block;
            index = temp;
            for (uint32_t j = 0; j < geo->interleave; j++, index++) {
                map_index = BAD_MAP_INDEX(index);
                map_offset = BAD_MAP_OFFSET(index);
                bad_map[map_index] |= PERFECT_BLOCK_BIT << map_offset;
            }
        } else {
            index = temp;
            remap_group = remap_grp_base;
            for (uint32_t j = 0; j < geo->interleave; j++, index++) {
                if (remap_group->wPtr >= REMAP_TABLE_MAX) {
                    remap_group = PTR_ADD_OFFSET(remap_group, grp_size);
                    continue;
                }
                map_index = BAD_MAP_INDEX(index);
                map_offset = BAD_MAP_OFFSET(index);
                if (!(bad_map[map_index] & (BAD_BLOCK_BIT << map_offset))) {
                    remap_group->block[remap_group->wPtr++] = block;
                }
                remap_group = PTR_ADD_OFFSET(remap_group, grp_size);
            }
        }
    }

    bad_block_remap_table_build(remap_grp_base, grp_size);

    for (uint32_t i = 0; i < (geo->block - RESERVED_BLOCK); i++) {
        g_bbm->remap_vir_block[i] = i;
    }

    free(remap_grp_base);
}

extern int min_plane;
void bad_block_remap_table_build(void *remap_grp_base, uint32_t grp_size) {
    NAND_GEOMETRY_T *geo = nand_geometry_get();
    REMAP_GROUP_T *remap_group = remap_grp_base;
    uint8_t *bad_map = bad_block_map_get();
    uint32_t index, map_index, map_offset;
    uint16_t min_group_count = 0xFFFF;

    for (uint32_t i = 0; i < geo->interleave; i++) {
        if (remap_group->wPtr < min_group_count) {
            min_group_count = remap_group->wPtr;
            min_plane = i;
        }
        remap_group = PTR_ADD_OFFSET(remap_group, grp_size);
    }

    REMAP_NODE_T *remap_node = g_bbm->remap_table;
    uint16_t block;
    for (uint32_t i = 0; i < min_group_count; i++) {
        remap_group = remap_grp_base;
        ASSERT(!remap_node->used);
        for (uint32_t j = 0; j < geo->interleave; j++) {
            ASSERT(remap_group->wPtr > 0);
            block = remap_group->block[--remap_group->wPtr];
            index = block * geo->interleave + j;
            map_index = BAD_MAP_INDEX(index);
            map_offset = BAD_MAP_OFFSET(index);
            ASSERT(block < geo->block);
            ASSERT(!BAD_MAP_GET(bad_map[map_index], map_offset));
            bad_map[map_index] |= (REMAP_BLOCK_BIT << map_offset);
            remap_node[j].used = 1;
            remap_node[j].block = block;
            remap_group = PTR_ADD_OFFSET(remap_group, grp_size);
        }
        g_bbm->remap_phy_block[g_bbm->remap_phy_index++] = BBM_REMAP_MASK | i;
        remap_node = PTR_ADD_OFFSET(remap_node, g_bbm->remap_offset);
    }

    g_bbm->remap_table_index = min_group_count;
    remap_group = remap_grp_base;
    REMAP_GROUP_T *remap_dst = g_bbm->remap_group;
    uint32_t dst_grp_size = sizeof(uint16_t) * (REMAP_GROUP_MAX + 1);
    for (uint32_t i = 0; i < geo->interleave; i++) {
        uint32_t count = MIN(remap_group->wPtr, REMAP_GROUP_MAX);
        for (uint32_t j = 0; j < count; j++) {
            remap_dst->block[remap_dst->wPtr++] = remap_group->block[--remap_group->wPtr];
        }
        remap_group = PTR_ADD_OFFSET(remap_group, grp_size);
        remap_dst = PTR_ADD_OFFSET(remap_dst, dst_grp_size);
    }
}

void bad_block_find_remap(uint16_t log_block, uint16_t log_die, uint16_t *remap_block) {
    NAND_GEOMETRY_T *geo = nand_geometry_get();
    uint16_t vir = g_bbm->remap_vir_block[log_block];
    uint16_t pb = g_bbm->remap_phy_block[vir];
    uint16_t block, map_index, map_offset;
    uint32_t index;
    uint8_t *bad_map = bad_block_map_get();

    if (BBM_IS_REMAP(pb)) {
        uint16_t table_index = BBM_REAMP_INDEX(pb);
        ASSERT(table_index < REMAP_TABLE_MAX);
        REMAP_NODE_T *remap_node = PTR_ADD_OFFSET(g_bbm->remap_table, table_index * g_bbm->remap_offset);
        ASSERT(remap_node->used);
        uint32_t start = log_die * geo->plane;
        for (uint32_t i = 0; i < geo->plane; i++) {
            block = remap_node[start + i].block;
            index = block * geo->interleave + start + i;
            map_index = BAD_MAP_INDEX(index);
            map_offset = BAD_MAP_OFFSET(index);
            ASSERT(IS_ONLY_BIT_SET(BAD_MAP_GET(bad_map[map_index], map_offset), REMAP_BLOCK_BIT));
            remap_block[i] = remap_node[start + i].block;
            ASSERT(remap_block[i] < geo->block);
        }
    } else {
        index = pb * geo->interleave + log_die * geo->plane;
        ASSERT(pb < geo->block);
        for (uint32_t i = 0; i < geo->plane; i++) {
            remap_block[i] = pb;
            map_index = BAD_MAP_INDEX(index);
            map_offset = BAD_MAP_OFFSET(index);
            ASSERT(IS_ONLY_BIT_SET(BAD_MAP_GET(bad_map[map_index], map_offset), PERFECT_BLOCK_BIT));
            index++;
        }
    }
}

uint32_t bad_block_run_replace(uint16_t log_block, uint8_t *error_plane) {
    NAND_GEOMETRY_T *geo = nand_geometry_get();
    REMAP_GROUP_T *remap_group;
    REMAP_NODE_T *remap_node = NULL;
    uint8_t *bad_map = bad_block_map_get();
    uint32_t grp_size = sizeof(uint16_t) * (REMAP_GROUP_MAX + 1);
    uint32_t index, map_index, map_offset;
    uint16_t vir = g_bbm->remap_vir_block[log_block];
    uint16_t pb = g_bbm->remap_phy_block[vir];
    uint16_t table_index = (pb & BBM_REMAP_MASK) ? pb & ~BBM_REMAP_MASK : 0xFFFF;
    uint16_t block = pb & 0x7FFF;
    uint32_t can_replace = TRUE;
    uint16_t remap_block[geo->interleave];
    ASSERT(block < geo->block);

    if (table_index != 0xFFFF) {
        remap_node = PTR_ADD_OFFSET(g_bbm->remap_table, table_index * g_bbm->remap_offset);
    }

    for (uint32_t i = 0; i < geo->interleave; i++) {
        block = (remap_node == NULL) ? pb : remap_node[i].block;
        index = block * geo->interleave + i;
        map_index = BAD_MAP_INDEX(index);
        map_offset = BAD_MAP_OFFSET(index);
        ASSERT(!(bad_map[map_index] & (BAD_BLOCK_BIT << map_offset)));
        if (remap_node) {
            ASSERT(IS_ONLY_BIT_SET(BAD_MAP_GET(bad_map[map_index], map_offset), REMAP_BLOCK_BIT));
            ASSERT(remap_node[i].used);
            bad_map[map_index] &= ~(REMAP_BLOCK_BIT << map_offset);
            remap_node[i].used = 0;
        } else {
            ASSERT(IS_ONLY_BIT_SET(BAD_MAP_GET(bad_map[map_index], map_offset), PERFECT_BLOCK_BIT));
            bad_map[map_index] &= ~(PERFECT_BLOCK_BIT << map_offset);
        }
        if (error_plane[i]) {
            bad_map[map_index] |= (BAD_BLOCK_BIT << map_offset);
        }
        remap_block[i] = block;
    }

    remap_group = g_bbm->remap_group;
    for (uint32_t i = 0; i < geo->interleave; i++) {
        if (error_plane[i]) {
            if (remap_group->wPtr > 0) {
                block = remap_group->block[--remap_group->wPtr];
                index = block * geo->interleave + i;
                map_index = BAD_MAP_INDEX(index);
                map_offset = BAD_MAP_OFFSET(index);
                ASSERT(block < geo->block);
                ASSERT(!(bad_map[map_index] & ((BAD_BLOCK_BIT | REMAP_BLOCK_BIT) << map_offset)));
                remap_block[i] = block;
            } else {
                block = 0xFFFF;
                index = RESERVED_BLOCK * geo->interleave + i;
                for (uint32_t j = RESERVED_BLOCK; j < geo->block; j++) {
                    map_index = BAD_MAP_INDEX(index);
                    map_offset = BAD_MAP_OFFSET(index);
                    if (BAD_MAP_GET(bad_map[map_index], map_offset) == 0) {
                        block = j;
                        break;
                    }
                    index += geo->interleave;
                }
                remap_block[i] = block;
            }
        }
        remap_group = PTR_ADD_OFFSET(remap_group, grp_size);
    }

    for (uint32_t i = 0; i < geo->interleave; i++) {
        if (remap_block[i] == 0xFFFF) {
            can_replace = FALSE;
            break;
        }
    }

    if (!can_replace) {
        remap_group = g_bbm->remap_group;
        for (uint32_t i = 0; i < geo->interleave; i++) {
            if (remap_block[i] != 0xFFFF && remap_group->wPtr < REMAP_GROUP_MAX) {
                remap_group->block[remap_group->wPtr++] = remap_block[i];
                remap_group = PTR_ADD_OFFSET(remap_group, grp_size);
                index = remap_block[i] * geo->interleave + i;
                map_index = BAD_MAP_INDEX(index);
                map_offset = BAD_MAP_OFFSET(index);
                ASSERT(BAD_MAP_GET(bad_map[map_index], map_offset) == 0);
            }
        }
        g_bbm->remap_phy_block[vir] = ~BBM_REMAP_MASK & 0xFFFF;
        return FALSE;
    }

    if (table_index == 0xFFFF) {
        uint32_t start = g_bbm->remap_table_index % REMAP_TABLE_MAX;
        for (uint32_t i = 0; i < REMAP_TABLE_MAX; i++) {
            remap_node = PTR_ADD_OFFSET(g_bbm->remap_table, start * g_bbm->remap_offset);
            if (!remap_node->used) {
                table_index = start;
                for (uint32_t j = 0; j < geo->interleave; j++) {
                    ASSERT(!remap_node[j].used);
                }
                break;
            }
            start = (start + 1) % REMAP_TABLE_MAX;
        }
    }

    if (table_index == 0xFFFF) {
        g_bbm->remap_phy_block[vir] = ~BBM_REMAP_MASK & 0xFFFF;
        return FALSE;
    }

    ASSERT(table_index < REMAP_TABLE_MAX);
    remap_node = PTR_ADD_OFFSET(g_bbm->remap_table, table_index * g_bbm->remap_offset);
    for (uint32_t i = 0; i < geo->interleave; i++) {
        ASSERT(!remap_node[i].used);
        ASSERT(remap_block[i] < geo->block);
        remap_node[i].used = 1;
        remap_node[i].block = remap_block[i];
        index = remap_block[i] * geo->interleave + i;
        map_index = BAD_MAP_INDEX(index);
        map_offset = BAD_MAP_OFFSET(index);
        ASSERT(BAD_MAP_GET(bad_map[map_index], map_offset) == 0);
        bad_map[map_index] |= (REMAP_BLOCK_BIT << map_offset);
    }
    g_bbm->remap_phy_block[vir] = BBM_REMAP_MASK | table_index;
    return TRUE;
}


uint16_t bad_block_get_max(void) {
    return g_bbm->remap_phy_index;
}