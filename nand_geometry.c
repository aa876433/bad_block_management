//
// Created by JohnLin on 2024/5/14.
//

#include "config.h"
#include "nand_geometry.h"

NAND_GEOMETRY_T g_geo;

void nand_geometry_init(void)
{
    g_geo.wl         = WORD_LINE;
    g_geo.block      = BLOCK;
    g_geo.plane      = PLANE;
    g_geo.die        = DIE;
    g_geo.interleave = PLANE * DIE;
}

void *nand_geometry_get(void)
{
    return (void *)&g_geo;
}