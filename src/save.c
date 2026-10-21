/* save.c - saved games in the second 16 KiB of cartridge RAM (banked).
 *
 * Three slots of 5456 bytes.  A slot is a header followed by a run length
 * coded stream: FreezeTime, gamestate, EASYMODEON, BGFLAGS, the two map
 * planes XORed with the level as stored in ROM (so mostly zeros), the
 * object count and the objects (state pointers as state numbers).
 * The game data is in cart RAM half 0 and the slots in half 1, which share
 * slot 2: bytes go through small buffers in main RAM.
 */
#include "game/def.h"

#define SLOT_SIZE   5456
#define SLOT_BASE(s) (0x8000 + (u16)(s) * SLOT_SIZE)
#define SAVE_MAGIC  0x4D533133ul        /* 'MS13' */
#define OBJ_SAVE_BYTES ((u16)__builtin_offsetof(objtype, next))

typedef struct
{
    u32 magic;
    u16 used;
    u16 mapon;
    u16 length;         /* stream bytes */
    u16 sum;            /* stream checksum */
    char desc[32];
} SaveHeader;

static u8  iob[64];
static u8  ion, iopos;
static u16 ioaddr, ioend, iosum, iolen;
static u8  ioerr;

/* ----------------------------------------------------------- RLE out
 * control byte c < 0x80: c+1 literal bytes follow
 *              c >= 0x80: next byte repeated c-0x80+3 times (3..130) */
static u8  lit[128];
static u8  nlit;
static u8  runb, runn;

static void out_flush(void)
{
    if (!ion)
        return;
    if (ioaddr + ion > ioend)
        ioerr = 1;
    else
    {
        slot2_map(SLOT2_CARTRAM1);
        memcpy((u8 *)ioaddr, iob, ion);
        slot2_map(SLOT2_CARTRAM);
        ioaddr += ion;
    }
    ion = 0;
}

static void out_raw(u8 b)
{
    iosum += b;
    iolen++;
    iob[ion++] = b;
    if (ion == sizeof(iob))
        out_flush();
}

static void lit_flush(void)
{
    u8 i;
    if (!nlit)
        return;
    out_raw(nlit - 1);
    for (i = 0; i < nlit; i++)
        out_raw(lit[i]);
    nlit = 0;
}

static void run_flush(void)
{
    if (runn >= 3)
    {
        lit_flush();
        out_raw(0x80 + runn - 3);
        out_raw(runb);
    }
    else
        while (runn)
        {
            lit[nlit++] = runb;
            if (nlit == 128)
                lit_flush();
            runn--;
        }
    runn = 0;
}

static void put(u8 b)
{
    if (runn && b == runb && runn < 130)
    {
        runn++;
        return;
    }
    run_flush();
    runb = b;
    runn = 1;
}

static void put_mem(const void *p, u16 n)
{
    const u8 *s = p;
    while (n--)
        put(*s++);
}

static void put_end(void)
{
    run_flush();
    lit_flush();
    out_flush();
}

/* ------------------------------------------------------------ RLE in */
static u8 in_lit, in_run, in_b;

static u8 in_raw(void)
{
    if (iopos == ion)
    {
        u16 n = ioend - ioaddr;
        if (!n)
        {
            ioerr = 1;
            return 0;
        }
        if (n > sizeof(iob)) n = sizeof(iob);
        slot2_map(SLOT2_CARTRAM1);
        memcpy(iob, (const u8 *)ioaddr, n);
        slot2_map(SLOT2_CARTRAM);
        ioaddr += n;
        ion = n;
        iopos = 0;
    }
    return iob[iopos++];
}

static u8 get(void)
{
    if (in_run)
    {
        in_run--;
        return in_b;
    }
    if (in_lit)
    {
        in_lit--;
        return in_raw();
    }
    {
        u8 c = in_raw();
        if (c < 0x80)
        {
            in_lit = c;
            return in_raw();
        }
        in_b = in_raw();
        in_run = c - 0x80 + 2;
        return in_b;
    }
}

static void get_mem(void *p, u16 n)
{
    u8 *d = p;
    while (n--)
        *d++ = get();
}

/* ---------------------------------------------------------- headers */

static void header_read(u8 slot, SaveHeader *h)
{
    slot2_map(SLOT2_CARTRAM1);
    memcpy(h, (const u8 *)SLOT_BASE(slot), sizeof(*h));
    slot2_map(SLOT2_CARTRAM);
}

static void header_write(u8 slot, const SaveHeader *h)
{
    slot2_map(SLOT2_CARTRAM1);
    memcpy((u8 *)SLOT_BASE(slot), h, sizeof(*h));
    slot2_map(SLOT2_CARTRAM);
}

void save_init(void) B
{
}

/* level name of a used slot (desc: 32 bytes), 0 when empty */
u8 save_slot_info(u8 slot, char *desc) B
{
    SaveHeader h;
    u8 old = slot2_cur;
    slot2_map(SLOT2_CARTRAM);
    header_read(slot, &h);
    slot2_map(old);
    if (h.magic != SAVE_MAGIC || h.used != 1 || h.length > SLOT_SIZE - sizeof(h))
        return 0;
    memcpy(desc, h.desc, 31);
    desc[31] = 0;
    return 1;
}

/* the ROM planes, n bytes from offset ofs of plane p (0, 2 = high bytes) */
static void plane_xor(u8 p, u8 *dst, u16 ofs, u16 n)
{
    u8 rom[32];
    u16 base = p ? 2 + 2 * mapwidth * mapheight : 2;
    u8 i;
    while (n)
    {
        u8 k = n > 32 ? 32 : n;
        map_read(gamestate.mapon, base + ofs, rom, k);
        for (i = 0; i < k; i++)
            dst[i] ^= rom[i];
        dst += k;
        ofs += k;
        n -= k;
    }
}

static void put_plane(u8 p, const u8 *plane)
{
    u16 n = mapwidth * mapheight, ofs = 0;
    u8 buf[32];
    while (ofs < n)
    {
        u8 k = n - ofs > 32 ? 32 : n - ofs;
        memcpy(buf, plane + ofs, k);
        plane_xor(p, buf, ofs, k);
        put_mem(buf, k);
        ofs += k;
    }
}

static void get_plane(u8 p, u8 *plane)
{
    u16 n = mapwidth * mapheight, ofs = 0;
    u8 buf[32];
    while (ofs < n)
    {
        u8 k = n - ofs > 32 ? 32 : n - ofs;
        get_mem(buf, k);
        plane_xor(p, buf, ofs, k);
        memcpy(plane + ofs, buf, k);
        ofs += k;
    }
}

boolean SaveTheGame(u16 slot) B
{
    SaveHeader h;
    objtype *o;
    u16 n = 0;

    memset(&h, 0, sizeof(h));
    header_write(slot, &h);         /* invalid while writing */

    ion = 0;
    ioaddr = SLOT_BASE(slot) + sizeof(h);
    ioend = SLOT_BASE(slot) + SLOT_SIZE;
    iosum = iolen = 0;
    ioerr = 0;
    nlit = runn = 0;

    put_mem(&FreezeTime, sizeof(FreezeTime));
    put_mem(&gamestate, sizeof(gamestate));
    put_mem(&EASYMODEON, sizeof(EASYMODEON));
    put_mem(&BGFLAGS, sizeof(BGFLAGS));
    put_plane(0, mapplane0);
    put_plane(1, mapplane2);
    for (o = player; o; o = o->next) n++;
    put_mem(&n, sizeof(n));
    for (o = player; o; o = o->next)
    {
        objtype tmp;
        memcpy(&tmp, o, OBJ_SAVE_BYTES);
        tmp.state = (const statetype *)STATE_INDEX(o->state);
        put_mem(&tmp, OBJ_SAVE_BYTES);
    }
    put_end();
    if (ioerr)
        return false;

    h.magic = SAVE_MAGIC;
    h.used = 1;
    h.mapon = gamestate.mapon;
    h.length = iolen;
    h.sum = iosum;
    strncpy(h.desc, LevelName(gamestate.mapon), 31);
    header_write(slot, &h);
    return true;
}

static boolean stream_ok(u8 slot, const SaveHeader *h)
{
    u16 i, sum = 0, a = SLOT_BASE(slot) + sizeof(*h);
    u8 b[32];
    for (i = 0; i < h->length; )
    {
        u8 k = h->length - i > 32 ? 32 : h->length - i, j;
        slot2_map(SLOT2_CARTRAM1);
        memcpy(b, (const u8 *)(a + i), k);
        slot2_map(SLOT2_CARTRAM);
        for (j = 0; j < k; j++)
            sum += b[j];
        i += k;
    }
    return sum == h->sum;
}

boolean LoadTheGame(u16 slot) B
{
    SaveHeader h;
    u16 i,x,y,n;
    objtype *prev,*next;

    header_read(slot, &h);
    if (h.magic != SAVE_MAGIC || h.used != 1 || h.length > SLOT_SIZE - sizeof(h)
        || !stream_ok(slot, &h))
        return false;

    ion = iopos = 0;
    ioaddr = SLOT_BASE(slot) + sizeof(h);
    ioend = ioaddr + h.length;
    ioerr = 0;
    in_lit = in_run = 0;

    playstate = ex_loadedgame;
    get_mem(&FreezeTime, sizeof(FreezeTime));
    get_mem(&gamestate, sizeof(gamestate));
    get_mem(&EASYMODEON, sizeof(EASYMODEON));
    get_mem(&BGFLAGS, sizeof(BGFLAGS));

    SetupGameLevel ();

    get_plane(0, mapplane0);
    get_plane(1, mapplane2);

    memset (tilemap,0,sizeof(tilemap));
    memset (actorat,0,sizeof(actorat));
    for (y=0;y<mapheight;y++)
        for (x=0;x<mapwidth;x++)
        {
            u16 tile = MAP0(x,y);
            if (tile<NUMFLOORS)
            {
                tilemap[x][y] = tile;
                if (tile>0)
                    actorat[x][y] = tile;
            }
        }

    R_SealMap();
    get_mem(&n, sizeof(n));
    InitObjList ();
    new = player;
    for (i = 0; i < n; i++)
    {
        u16 si;
        prev = new->prev;
        next = new->next;
        get_mem(new, OBJ_SAVE_BYTES);
        si = (u16)new->state;
        new->state = &states[si < NUMSTATES ? si : 0];
        new->prev = prev;
        new->next = next;
        actorat[new->tilex][new->tiley] = ACTOROBJ(new);
        if (i + 1 < n)
            GetNewObj (false);
    }
    /* the player marker isn't kept in actorat */
    actorat[player->tilex][player->tiley] = 0;

    return !ioerr;
}
