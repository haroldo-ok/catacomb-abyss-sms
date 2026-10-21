/* cartram.h - layout of cartridge RAM half 0 (mapped in slot 2 during play) */
#ifndef CARTRAM_H
#define CARTRAM_H

#define MAPSIZE   64
#define MAPSIZEY  32

#define CR_PAD      0x8000      /* 32 x SEALTILE: column x = -1 */
#define CR_TILEMAP  0x8020      /* u8 [64][32], x major */
#define CR_SPOTVIS  0x8820      /* u8 [64][32]: == vis_stamp when seen */
#define CR_ACTORAT  0x9020      /* u8 [64][32] */
#define MAPSIZEY_MAP 32          /* plane arrays: mapwidth*mapheight <= 1280 */
#define MAP_PLANE_BYTES 1280
#define CR_PLANE0   0x9820      /* u8 [1280] */
#define CR_PLANE2   0x9D20      /* u8 [1280] high byte of plane 2 */
#define CR_OBJLIST  0xA220      /* objtype [100], 64 bytes each */
#define CR_SCRATCH  0xBB20      /* 1248 bytes of work space */
#define CR_SCRATCH_SIZE 0x04E0
#define CR_END      0xC000

#define SPOTVIS_OFS (CR_SPOTVIS - CR_TILEMAP)

#define TILEMAP(x,y) (*(u8 *)(CR_TILEMAP + ((u16)(x) << 5) + (y)))
#define SPOTVIS_RAW(x,y) (*(u8 *)(CR_SPOTVIS + ((u16)(x) << 5) + (y)))
#define ACTORAT(x,y) (*(u8 *)(CR_ACTORAT + ((u16)(x) << 5) + (y)))

#endif
