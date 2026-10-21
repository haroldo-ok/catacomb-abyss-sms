/* c4_const.c - constant tables shared by game files in different banks
 * (bank 0: banked code can only read its own bank's constants) */
#include "def.h"

const dirtype opposite[9] =
    {south,west,north,east,southwest,northwest,northeast,southeast,nodir};

const dirtype dirtable[9] = {northwest,north,northeast,west,nodir,east,
    southwest,south,southeast};

const s16 other_x[4] = {0,39,39,0};
const s16 other_y[4] = {0,0,27,27};

const u16 sky_daytonight[5] = {0x0909,0x0101,0x0808,0x0000,0xFFFF};
const u16 sky_lightning[6] = {0x0101,0x0909,0x0f0f,0x0808,0x0000,0xFFFF};

const u16 sky_colors[NUMLEVELS]={0x0000,0x0000,0x0000,0x0000,0x0808,
                                 0x0404,0x0000,0x0000,0x0000,0x0000,
                                 0x0000,0x0000,0x0000,0x0000,0x0606,
                                 0x0000,0x0000,0x0000,0x0000,0x0000,
                                 0x0000};
const u16 gnd_colors[NUMLEVELS]={0x0202,0x0202,0x0606,0x0202,0x0707,
                                 0x0505,0x0808,0x0606,0x0101,0x0808,
                                 0x0606,0x0404,0x0808,0x0c0c,0x0e0e,
                                 0x0808,0x0808,0x0c0c,0x0000,0x0707,
                                 0x0808};

/* the cartridge RAM arrays (absolute symbols for the linker) */
__at(CR_TILEMAP) u8 tilemap[MAPSIZE][MAPSIZEY];
__at(CR_ACTORAT) u8 actorat[MAPSIZE][MAPSIZEY];
__at(CR_PLANE0) byte mapplane0[MAP_PLANE_BYTES];
__at(CR_PLANE2) byte mapplane2[MAP_PLANE_BYTES];
__at(CR_OBJLIST) objtype objlist[MAXACTORS];
