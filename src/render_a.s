;; render_a.s - Z80 inner loops of the 3-D view (bank 0)
;;
;; colbuf: 80 rows of one tile column, left ray in page 0xDA, right ray in
;; page 0xDB.  Texel bytes are 0x80 + 4 * colour: stage 2 reads the plane
;; byte b of a tile row at ((L + b) << 8) | R in the plane table bank
;; (slot 2), so no nibble merging is ever needed.

        .module render_a
        .include "gen/assets.inc"
        .area _DATA
dr_ws:  .ds 1
cr_q:   .ds 1
cr_cneg: .ds 1
cr_ic:  .ds 2
cr_is:  .ds 2
cr_c2:  .ds 2
cr_w:   .ds 1
cr_s:   .ds 1
cr_u:   .ds 1
dr_we:  .ds 1
dr_page: .ds 1
dr_tb:  .ds 1
dr_tex: .ds 2
spr_basehi: .ds 1
spr_page: .ds 1
nu_tile: .ds 2
rc_tr0: .ds 1
rc_tr1: .ds 1
rc_hs:  .ds 1
ca_c:   .ds 1
re_chg: .ds 1
pt_src: .ds 1
pt_dst: .ds 1
pt_s:   .ds 1
pt_u:   .ds 1
ip_sm:  .ds 1
ip_um:  .ds 1
ip_v:   .ds 2
ip_w:   .ds 2
ipq_a:  .ds 1
ipq_b:  .ds 1
ap_side: .ds 1
s2_hvar: .ds 1
ip_k:   .ds 1
ip_adj: .ds 1
lp_n:   .ds 1
lp_as:  .ds 2
lp_aq:  .ds 2
lp_ds:  .ds 2
lp_dq:  .ds 2
lp_end: .ds 1
lp_bb:  .ds 1
lp_tbb: .ds 1
;; ray_s and ray_tbank must stay adjacent (rd_sig reads tbank at 56(iy))
_ray_s::     .ds 56
_ray_tbank:: .ds 56
        .area _CODE

COLBUF   = 0xDA

        .globl _s2_vaddr, _s2_y, _s2_n, _s2_hand
        .globl _s1_rdbank

;; 8x8 multiply with quarter squares (render data bank in slot 1):
;; in b = x, c = y ; out de = x*y ; clobbers a, hl
QSQ = RD_QSQ_LO / 256
mul8:
        ld      a,b
        add     a,c
        ld      l,a
        ld      a,#QSQ
        adc     a,#0
        ld      h,a
        ld      e,(hl)
        inc     h
        inc     h
        ld      d,(hl)
        ld      a,b
        sub     c
        jr      nc,00001$
        neg
00001$:
        ld      l,a
        ld      h,#QSQ
        ld      a,e
        sub     (hl)
        ld      e,a
        inc     h
        inc     h
        ld      a,d
        sbc     a,(hl)
        ld      d,a
        ret

;; u16 mulhi16(u16 a, u16 b) -- hl = a, de = b -> de ~ (a*b) >> 16
;; (the al*bl term is dropped)
_mulhi16::
        ld      (_mq_a),hl
        ld      (_mq_b),de
        ld      b,h
        ld      c,d
        call    mul8
        push    de              ; ah*bh
        ld      a,(_mq_a+1)
        ld      b,a
        ld      a,(_mq_b)
        ld      c,a
        call    mul8
        push    de              ; ah*bl
        ld      a,(_mq_a)
        ld      b,a
        ld      a,(_mq_b+1)
        ld      c,a
        call    mul8            ; al*bh
        pop     hl
        add     hl,de
        ld      a,#0
        rla
        ld      e,h
        ld      d,a
        pop     hl
        add     hl,de
        ex      de,hl
        ret

;; u16 mulfrac8(u16 d, u16 f) -- hl = d, de = f (0..255) -> de = (d*f) >> 8
_mulfrac8::
        ld      c,e
        ld      b,h
        push    hl
        call    mul8            ; dh*f
        pop     hl
        push    de
        ld      b,l
        call    mul8            ; dl*f
        ld      l,d
        ld      h,#0
        pop     de
        add     hl,de
        ex      de,hl
        ret

;; u8 dda(void) -- the ray walk.  Main set: HL = tile pointer, DE = x step
;; (+-32), B = step budget, C = vis stamp.  Alternate set: BC = sideX,
;; DE = sideY.  Empty tiles get the stamp in spotvis (tile + 0x800).
;; Returns 1 (hit: cr_p, cr_dist, cr_side) or 0.
        .globl  _cr_p, _cr_sx, _cr_sX, _cr_sY, _cr_dX, _cr_dY, _cr_lim
        .globl  _cr_dist, _cr_side, _cr_ydir, _vis_stamp, _mq_a, _mq_b
_dda::
        push    iy
        ld      a,(_vis_stamp)
        ld      c,a
        ld      b,#96
        ld      hl,(_cr_p)
        ld      de,(_cr_sx)
        exx
        ld      hl,(_cr_sX)
        ld      bc,(_cr_dX)
        ld      iy,(_cr_sY)
        ld      de,(_cr_dY)
        exx
        ld      a,(_cr_ydir)
        or      a
        jp      nz,dda_s

dda_n:
        exx
        ld      a,l
        .db     0xFD,0x95       ; sub iyl
        ld      a,h
        .db     0xFD,0x9C       ; sbc a,iyh
        jr      nc,dda_n_y      ; sX >= sY: y step
        add     hl,bc           ; sX += dX
        jr      c,00021$
00022$: exx
        add     hl,de
        ld      a,(hl)
        or      a
        jr      nz,dda_n_xs
        set     3,h
        ld      (hl),c
        res     3,h
        djnz    dda_n
        jp      dda_nohit
00021$: ld      hl,#0xFFFF
        jr      00022$
dda_n_y:
        add     iy,de           ; sY += dY
        jr      c,00023$
00024$: exx
        dec     hl
        ld      a,(hl)
        or      a
        jr      nz,dda_n_ys
        set     3,h
        ld      (hl),c
        res     3,h
        djnz    dda_n
        jp      dda_nohit
00023$: ld      iy,#0xFFFF
        jr      00024$
dda_n_xs:
        ld      a,#1
        jr      dda_n_sd
dda_n_ys:
        xor     a
dda_n_sd:
        ld      (_cr_side),a
        call    dda_far
        jr      nc,dda_hit
        djnz    dda_n
        jp      dda_nohit

dda_s:
        exx
        ld      a,l
        .db     0xFD,0x95       ; sub iyl
        ld      a,h
        .db     0xFD,0x9C       ; sbc a,iyh
        jr      nc,dda_s_y      ; sX >= sY: y step
        add     hl,bc           ; sX += dX
        jr      c,00021$
00022$: exx
        add     hl,de
        ld      a,(hl)
        or      a
        jr      nz,dda_s_xs
        set     3,h
        ld      (hl),c
        res     3,h
        djnz    dda_s
        jp      dda_nohit
00021$: ld      hl,#0xFFFF
        jr      00022$
dda_s_y:
        add     iy,de           ; sY += dY
        jr      c,00023$
00024$: exx
        inc     hl
        ld      a,(hl)
        or      a
        jr      nz,dda_s_ys
        set     3,h
        ld      (hl),c
        res     3,h
        djnz    dda_s
        jp      dda_nohit
00023$: ld      iy,#0xFFFF
        jr      00024$
dda_s_xs:
        ld      a,#1
        jr      dda_s_sd
dda_s_ys:
        xor     a
dda_s_sd:
        ld      (_cr_side),a
        call    dda_far
        jr      nc,dda_hit
        djnz    dda_s
        jp      dda_nohit

;; dda_far: solid cell: NC if the ray leaves it beyond cr_lim (min of the
;; next crossings >= cr_lim)
dda_far:
        exx
        push    de
        ld      de,(_cr_lim)
        or      a
        sbc     hl,de
        add     hl,de           ; C: sX < lim
        jr      c,00002$
        push    hl
        push    iy
        pop     hl
        or      a
        sbc     hl,de           ; C: sY < lim
        pop     hl
00002$: pop     de
        exx
        ret

dda_hit:
        ld      (_cr_p),hl
        exx
        ld      a,(_cr_side)
        or      a
        jr      z,00001$
        or      a
        sbc     hl,bc           ; dist = sX - dX
        jr      00002$
00001$: push    iy
        pop     hl
        sbc     hl,de           ; (carry clear) dist = sY - dY
00002$: ld      (_cr_dist),hl
        exx
        pop     iy
        ld      a,#1
        ret
dda_nohit:
        pop     iy
        xor     a
        ret

;; void draw_ray(void) -- one ray of the current tile column into colbuf
;; page COLBUF + colright: sky rows [dr_y0, ws), the wall [ws, we) through
;; the compiled scaler and ground rows [we, dr_y1).
        .globl  _dr_c, _dr_y0, _dr_y1, _dr_skyv, _dr_gndv, _colright
        .globl  _ray_s, _ray_tbank, _ray_tex
        .globl  _scaler_bank0, _scaler_addr0
_draw_ray::
        ; c = ray, b = colbuf page
        ld      l,c
        ld      h,#0
        ld      de,#_ray_s
        add     hl,de
        ld      a,(hl)          ; s
        ld      de,#56
        add     hl,de           ; ray_tbank (= ray_s + 56)
        ld      d,(hl)
        ld      e,a
        push    de              ; tbank : s
        ld      l,c
        ld      h,#0
        add     hl,hl
        ld      de,#_ray_tex
        add     hl,de
        ld      e,(hl)
        inc     hl
        ld      d,(hl)
        push    de              ; tex
        ld      c,a             ; c = s
        ld      a,#40
        sub     c
        jr      nc,00001$
        xor     a
00001$: ld      e,a             ; e = ws = wall top
        ld      a,#40
        add     a,c
        cp      #81
        jr      c,00002$
        ld      a,#80
00002$: ld      d,a             ; d = we = wall end
        ; sky rows [y0, min(ws, y1))
        ld      a,(_dr_y1)
        cp      e
        jr      c,00003$
        ld      a,e
00003$: ld      hl,#_dr_y0
        sub     (hl)
        jr      z,00004$
        jr      c,00004$
        push    de
        ld      l,(hl)
        ld      h,b
        ld      d,a
        ld      a,(_dr_skyv)
        call    fillrun
        pop     de
00004$: ; ground rows [max(we, y0), y1)
        ld      a,(_dr_y0)
        cp      d
        jr      nc,00005$
        ld      a,d
00005$: ld      l,a
        ld      a,(_dr_y1)
        sub     l
        jr      z,00006$
        jr      c,00006$
        push    de
        ld      h,b
        ld      d,a
        ld      a,(_dr_gndv)
        call    fillrun
        pop     de
00006$: ld      d,b             ; de = page:ws
        pop     hl              ; tex
        pop     bc              ; b = tbank, c = s
        ld      a,c
        or      a
        ret     z
        ld      a,b
        ld      (#0xFFFF),a     ; slot 2 = texture bank
        ld      b,#0
        push    hl
        ld      hl,#_scaler_bank0
        add     hl,bc
        ld      a,(hl)
        ld      (#0xFFFE),a
        ld      hl,#_scaler_addr0
        add     hl,bc
        add     hl,bc
        ld      c,(hl)
        inc     hl
        ld      b,(hl)          ; bc = scaler entry
        ld      hl,#00009$
        ex      (sp),hl         ; scaler returns there; hl = tex
        push    bc
        ret                     ; -> scaler (hl = texture column, de = page:ws)
00009$:
        ld      a,(_s1_rdbank)
        ld      (#0xFFFE),a
        ret

;; fillrun: d rows (1..80) from h:l with value a; keeps b, c, h
fillrun:
        ex      af,af'
        ld      a,d
        add     a,a
        ld      e,a
        ld      d,#0
        push    hl
        ld      hl,#fill_end
        or      a
        sbc     hl,de
        ex      (sp),hl
        ex      af,af'
        ret
        ld      (hl),a
        inc     l
        ld      (hl),a
        inc     l
        ld      (hl),a
        inc     l
        ld      (hl),a
        inc     l
        ld      (hl),a
        inc     l
        ld      (hl),a
        inc     l
        ld      (hl),a
        inc     l
        ld      (hl),a
        inc     l
        ld      (hl),a
        inc     l
        ld      (hl),a
        inc     l
        ld      (hl),a
        inc     l
        ld      (hl),a
        inc     l
        ld      (hl),a
        inc     l
        ld      (hl),a
        inc     l
        ld      (hl),a
        inc     l
        ld      (hl),a
        inc     l
        ld      (hl),a
        inc     l
        ld      (hl),a
        inc     l
        ld      (hl),a
        inc     l
        ld      (hl),a
        inc     l
        ld      (hl),a
        inc     l
        ld      (hl),a
        inc     l
        ld      (hl),a
        inc     l
        ld      (hl),a
        inc     l
        ld      (hl),a
        inc     l
        ld      (hl),a
        inc     l
        ld      (hl),a
        inc     l
        ld      (hl),a
        inc     l
        ld      (hl),a
        inc     l
        ld      (hl),a
        inc     l
        ld      (hl),a
        inc     l
        ld      (hl),a
        inc     l
        ld      (hl),a
        inc     l
        ld      (hl),a
        inc     l
        ld      (hl),a
        inc     l
        ld      (hl),a
        inc     l
        ld      (hl),a
        inc     l
        ld      (hl),a
        inc     l
        ld      (hl),a
        inc     l
        ld      (hl),a
        inc     l
        ld      (hl),a
        inc     l
        ld      (hl),a
        inc     l
        ld      (hl),a
        inc     l
        ld      (hl),a
        inc     l
        ld      (hl),a
        inc     l
        ld      (hl),a
        inc     l
        ld      (hl),a
        inc     l
        ld      (hl),a
        inc     l
        ld      (hl),a
        inc     l
        ld      (hl),a
        inc     l
        ld      (hl),a
        inc     l
        ld      (hl),a
        inc     l
        ld      (hl),a
        inc     l
        ld      (hl),a
        inc     l
        ld      (hl),a
        inc     l
        ld      (hl),a
        inc     l
        ld      (hl),a
        inc     l
        ld      (hl),a
        inc     l
        ld      (hl),a
        inc     l
        ld      (hl),a
        inc     l
        ld      (hl),a
        inc     l
        ld      (hl),a
        inc     l
        ld      (hl),a
        inc     l
        ld      (hl),a
        inc     l
        ld      (hl),a
        inc     l
        ld      (hl),a
        inc     l
        ld      (hl),a
        inc     l
        ld      (hl),a
        inc     l
        ld      (hl),a
        inc     l
        ld      (hl),a
        inc     l
        ld      (hl),a
        inc     l
        ld      (hl),a
        inc     l
        ld      (hl),a
        inc     l
        ld      (hl),a
        inc     l
        ld      (hl),a
        inc     l
        ld      (hl),a
        inc     l
        ld      (hl),a
        inc     l
        ld      (hl),a
        inc     l
        ld      (hl),a
        inc     l
        ld      (hl),a
        inc     l
fill_end:
        ret

;; void spr_ray(void) -- one sprite ray: segment list spr_seg (shape bank in
;; slot 2), half height spr_s, page COLBUF + colright.  Leaves the sprite
;; scaler bank in slot 1.
        .globl  _spr_seg, _spr_s, _spr_jp, _sscaler_bank, _sscaler_hi
_spr_ray::
        ld      a,(_spr_s)
        ld      e,a
        ld      d,#0
        ld      hl,#_sscaler_bank
        add     hl,de
        ld      a,(hl)
        ld      (#0xFFFE),a
        ld      hl,#_sscaler_hi
        add     hl,de
        ld      a,(hl)
        ld      (spr_basehi),a
        ld      a,(_colright)
        add     a,#COLBUF
        ld      (spr_page),a
        ld      hl,(_spr_seg)
00001$:
        ld      a,(hl)          ; top
        inc     a
        ret     z
        dec     a
        inc     hl
        ld      b,(hl)          ; count
        inc     hl
        push    hl              ; pixels
        ld      l,a
        ld      a,(spr_basehi)
        ld      h,a
        ld      a,(spr_page)
        ld      d,a
        ld      e,(hl)          ; first row
        set     6,l
        ld      c,(hl)
        res     6,l
        set     7,l
        ld      h,(hl)
        ld      l,c
        ld      (_spr_jp+1),hl
        pop     hl
        call    _spr_jp
        jr      00001$

;; ---------------------------------------------------------------- cast
;; void cast_ray(void) -- casts ray cr_c (render data bank in slot 1, cart
;; RAM in slot 2) and fills ray_s/ray_wall/ray_u/ray_hit/ray_side/ray_tbank/
;; ray_tex.  Per frame: r_fine, r_viewx/y, fxn, fyn, cast_p0, light_tab,
;; dark_tab, vis_stamp.
        .globl  _cr_c, _r_fine, _r_viewx, _r_viewy, _fxn, _fyn, _cast_p0
        .globl  _light_tab, _dark_tab
        .globl  _ray_wall, _ray_u, _ray_hit, _ray_side

;; hl = delta, de = frac (0..256) -> de = frac*delta/256
;; side_lut: hl = delta = INVQ[bc], de = frac (0..256) -> de = side distance
;; (mulfrac8 / side_init result from the SIDE tables)
side_lut:
        ld      a,d
        or      a
        jr      z,00001$
        ex      de,hl           ; frac = 256: delta
        ret
00001$: ld      a,b             ; bank = SIDE_BANK0 + (bc >> 5)
        add     a,a
        add     a,a
        add     a,a
        ld      h,a
        ld      a,c
        rlca
        rlca
        rlca
        and     #7
        or      h
        add     a,#SIDE_BANK0
        ld      (#0xFFFE),a
        ld      a,c             ; 0x4000 + (bc & 31) * 512 + 2 frac
        and     #31
        add     a,a
        add     a,#0x40
        ld      h,a
        ld      l,e
        sla     l
        jr      nc,00002$
        inc     h
00002$: ld      e,(hl)
        inc     hl
        ld      d,(hl)
        ld      a,(_s1_rdbank)
        ld      (#0xFFFE),a
        ret

side_init:
        ld      a,d
        or      a
        jr      z,00001$
        ex      de,hl           ; frac = 256: delta
        ret
00001$:
        ld      a,h
        and     l
        inc     a
        jp      nz,_mulfrac8
        ld      a,e             ; delta saturated
        or      a
        ld      de,#0
        ret     z
        dec     de
        ret

;; hl = N, de = D (N < 256*D) -> a = N / D
div8q:
        ld      c,l
        ld      l,h
        ld      h,#0
        ld      b,#8
00001$:
        sla     c
        adc     hl,hl
        jr      c,00002$
        or      a
        sbc     hl,de
        jr      nc,00003$
        add     hl,de
        djnz    00001$
        ld      a,c
        ret
00002$:
        or      a
        sbc     hl,de
00003$:
        inc     c
        djnz    00001$
        ld      a,c
        ret

;; void set_tex(void) -- ray cr_c: tbank/tex from ray_wall, ray_u
_set_tex_asm::
        ld      a,(_cr_c)
        ld      e,a
        ld      d,#0
        ld      hl,#_ray_wall
        add     hl,de
        ld      b,(hl)          ; w
        ld      hl,#_ray_u
        add     hl,de
        ld      c,(hl)          ; u
        ld      hl,#_ray_tbank
        add     hl,de
        ld      a,b
        rrca
        rrca
        and     #0x3F
        add     a,#TEX_BANK0
        ld      (hl),a
        ld      hl,#_ray_tex
        add     hl,de
        add     hl,de
        ld      a,c
        rrca
        rrca
        ld      d,a             ; u rotated: bits 7-6 = u&3, bits 5-0 = u>>2
        and     #0xC0
        ld      (hl),a          ; low = u << 6
        inc     hl
        ld      a,b
        and     #3
        rlca
        rlca
        rlca
        rlca
        or      #0x80
        ld      e,a
        ld      a,d
        and     #0x3F
        or      e
        ld      (hl),a          ; high = 0x80 | (w&3)<<4 | u>>2
        ret

_cast_ray::
        ld      a,(_cr_c)
        ld      l,a
        ld      h,#0
        add     hl,hl
        ld      (cr_c2),hl
        ld      de,#RD_COLFINE
        add     hl,de
        ld      e,(hl)
        inc     hl
        ld      d,(hl)
        ld      hl,(_r_fine)
        add     hl,de
        ld      de,#3600
        bit     7,h
        jr      z,00001$
        add     hl,de
        jr      00002$
00001$:
        or      a
        sbc     hl,de
        jr      nc,00002$
        add     hl,de
00002$:
        ld      bc,#900
        ld      e,#0
00003$:
        or      a
        sbc     hl,bc
        jr      c,00004$
        inc     e
        jr      00003$
00004$:
        add     hl,bc           ; hl = r, e = quadrant
        ld      a,e
        ld      (cr_q),a
        ex      de,hl           ; de = r
        ld      hl,#900
        or      a
        sbc     hl,de           ; hl = 900 - r
        bit     0,a
        jr      nz,00005$
        ex      de,hl           ; even: ic = r, is = 900 - r
00005$:
        ; now de = is?  (odd: de = r = is, hl = 900-r = ic)
        ld      (cr_ic),hl
        ld      (cr_is),de
        add     hl,hl
        ld      bc,#RD_INVQ
        add     hl,bc
        ld      c,(hl)
        inc     hl
        ld      b,(hl)
        ld      (_cr_dX),bc
        ex      de,hl
        add     hl,hl
        ld      bc,#RD_INVQ
        add     hl,bc
        ld      c,(hl)
        inc     hl
        ld      b,(hl)
        ld      (_cr_dY),bc
        ; cneg = quadrant 1 or 2, ydir = quadrant >= 2
        ld      a,(cr_q)
        dec     a
        cp      #2
        ld      a,#0
        adc     a,a
        ld      (cr_cneg),a
        ld      a,(cr_q)
        srl     a
        ld      (_cr_ydir),a
        ; side x
        ld      a,(_fxn)
        ld      e,a
        ld      d,#0
        ld      a,(cr_cneg)
        or      a
        jr      z,00006$
        ld      hl,#-32
        ld      (_cr_sx),hl
        jr      00007$
00006$:
        ld      hl,#32
        ld      (_cr_sx),hl
        ld      hl,#256
        or      a
        sbc     hl,de
        ex      de,hl
00007$:
        ld      hl,(_cr_dX)
        ld      bc,(cr_ic)
        call    side_lut
        ld      (_cr_sX),de
        ; side y
        ld      a,(_fyn)
        ld      e,a
        ld      d,#0
        ld      a,(_cr_ydir)
        or      a
        jr      z,00008$
        ld      hl,#256
        or      a
        sbc     hl,de
        ex      de,hl
00008$:
        ld      hl,(_cr_dY)
        ld      bc,(cr_is)
        call    side_lut
        ld      (_cr_sY),de
        ld      hl,(cr_c2)
        ld      de,#RD_COLMIN
        add     hl,de
        ld      e,(hl)
        inc     hl
        ld      d,(hl)
        ld      (_cr_lim),de
        ld      hl,(_cast_p0)
        ld      (_cr_p),hl
        call    _dda
        or      a
        jp      z,cast_nohit
        ld      hl,(_cr_p)
        ld      b,(hl)          ; tile
        set     3,h
        ld      a,(_vis_stamp)
        ld      (hl),a
        ld      a,b
        cp      #71
        jp      nc,cast_nohit
        ; wall picture: x side light first, y side dark first
        ld      e,b
        ld      d,#0
        ld      hl,#_anim_tile
        add     hl,de
        ld      a,(hl)
        ld      hl,#_cast_anim
        or      (hl)
        ld      (hl),a
        ld      a,(_cr_side)
        or      a
        ld      hl,(_light_tab)
        ld      bc,(_dark_tab)
        jr      nz,00009$
        push    hl
        push    bc
        pop     hl
        pop     bc
00009$:
        push    hl
        add     hl,de
        ld      a,(hl)
        pop     hl
        cp      #0xFF
        jr      nz,00010$
        ld      h,b
        ld      l,c
        add     hl,de
        ld      a,(hl)
        cp      #0xFF
        jp      z,cast_nohit
00010$:
        ld      (cr_w),a
        ld      hl,(_cr_dist)
        ld      a,h
        or      a
        jr      nz,00011$
        ld      hl,#256
        ld      (_cr_dist),hl
00011$:
        ex      de,hl
        ld      a,d
        cp      #0x40
        jr      nc,00030$
        ld      a,(_cr_c)       ; s from the column table (dist < 16384)
        add     a,#DIV_BANK0
        ld      (#0xFFFE),a
        ld      hl,#0x4000-256
        add     hl,de
        ld      b,(hl)
        ld      a,(_s1_rdbank)
        ld      (#0xFFFE),a
        ld      a,b
        jr      00012$
00030$:
        ld      hl,(cr_c2)
        ld      bc,#RD_COLK
        add     hl,bc
        ld      a,(hl)
        inc     hl
        ld      h,(hl)
        ld      l,a
        call    div8q
        cp      #MAXS+1
        jr      c,00012$
        ld      a,#MAXS
00012$:
        ld      (cr_s),a
        ld      a,(_cr_side)
        or      a
        jr      z,00014$
        ; x side: hy = viewy -+ dist*sin, mirrored when cos <= 0
        ld      hl,(cr_is)
        add     hl,hl
        ld      de,#RD_COSQ
        add     hl,de
        ld      e,(hl)
        inc     hl
        ld      d,(hl)
        ld      hl,(_cr_dist)
        call    _mulhi16
        ld      a,(_cr_ydir)
        or      a
        ld      a,(_r_viewy)
        jr      z,00013$
        add     a,e
        jr      00016$
00013$:
        sub     e
00016$:
        rrca
        rrca
        and     #0x3F
        ld      b,a
        ld      a,(cr_cneg)
        jr      00019$
00014$:
        ; y side: hx = viewx +- dist*cos, mirrored when going south
        ld      hl,(cr_ic)
        add     hl,hl
        ld      de,#RD_COSQ
        add     hl,de
        ld      e,(hl)
        inc     hl
        ld      d,(hl)
        ld      hl,(_cr_dist)
        call    _mulhi16
        ld      a,(cr_cneg)
        or      a
        ld      a,(_r_viewx)
        jr      z,00015$
        sub     e
        jr      00017$
00015$:
        add     a,e
00017$:
        rrca
        rrca
        and     #0x3F
        ld      b,a
        ld      a,(_cr_ydir)
00019$:
        or      a
        ld      a,b
        jr      z,00018$
        cpl
        and     #0x3F
00018$:
        ld      c,a             ; c = u
        ld      a,(_cr_c)
        ld      e,a
        ld      d,#0
        ld      hl,#_ray_u
        add     hl,de
        ld      (hl),c
        ld      a,(cr_w)
        ld      b,a             ; b = w
        ld      hl,#_ray_wall
        add     hl,de
        ld      (hl),b
        ld      hl,#_ray_s
        add     hl,de
        ld      a,(cr_s)
        ld      (hl),a
        ld      hl,#_ray_tbank
        add     hl,de
        ld      a,b
        rrca
        rrca
        and     #0x3F
        add     a,#TEX_BANK0
        ld      (hl),a
        ld      hl,#_ray_side
        add     hl,de
        ld      a,(_cr_side)
        ld      (hl),a
        ld      hl,#_ray_tex
        add     hl,de
        add     hl,de
        ld      a,c
        rrca
        rrca
        ld      c,a
        and     #0xC0
        ld      (hl),a          ; low = u << 6
        inc     hl
        ld      a,b
        and     #3
        rlca
        rlca
        rlca
        rlca
        or      #0x80
        ld      b,a
        ld      a,c
        and     #0x3F
        or      b
        ld      (hl),a          ; high = 0x80 | (w & 3) << 4 | u >> 2
        ld      hl,#_ray_hit
        add     hl,de
        add     hl,de
        ld      bc,(_cr_p)
        ld      (hl),c
        inc     hl
        ld      (hl),b
        ret
cast_nohit:
        ld      a,(_cr_c)
        ld      e,a
        ld      d,#0
        ld      hl,#_ray_s
        add     hl,de
        ld      (hl),#0
        ld      hl,#_ray_wall
        add     hl,de
        ld      (hl),#0xFF
        ld      hl,#_ray_hit
        add     hl,de
        add     hl,de
        xor     a
        ld      (hl),a
        inc     hl
        ld      (hl),a
        ret

;; void s2_setaddr(void) -- VRAM write address = s2_vaddr
_s2_setaddr::
        ld      hl,(_s2_vaddr)
        di
        ld      a,l
        out     (#0xBF),a
        ld      a,h
        or      #0x40
        out     (#0xBF),a
        ei
        ret

;; void s2_rows(void) -- s2_n rows from row s2_y: plane bytes to VRAM
;; (plane table bank in slot 2).  OUTs are 27 cycles apart or more.
_s2_rows::
        ld      a,(_s2_n)
        or      a
        ret     z
        ld      b,a
        ld      a,(_s2_y)
        ld      l,a
        ld      h,#COLBUF
        ld      c,#0xBE
        ld      a,b
        and     #7
        jr      z,00002$
        push    bc
        ld      b,a
00001$:
        ld      d,(hl)
        inc     h
        ld      e,(hl)
        ld      a,(de)
        out     (c),a
        inc     d
        dec     h
        ld      a,(de)
        out     (c),a
        inc     d
        inc     l
        ld      a,(de)
        out     (c),a
        inc     d
        nop
        ld      a,(de)
        out     (c),a
        djnz    00001$
        pop     bc
00002$:
        srl     b
        srl     b
        srl     b
        ret     z
00003$:
        ld      d,(hl)
        inc     h
        ld      e,(hl)
        ld      a,(de)
        out     (c),a
        inc     d
        dec     h
        ld      a,(de)
        out     (c),a
        inc     d
        inc     l
        ld      a,(de)
        out     (c),a
        inc     d
        nop
        ld      a,(de)
        out     (c),a
        ld      d,(hl)
        inc     h
        ld      e,(hl)
        ld      a,(de)
        out     (c),a
        inc     d
        dec     h
        ld      a,(de)
        out     (c),a
        inc     d
        inc     l
        ld      a,(de)
        out     (c),a
        inc     d
        nop
        ld      a,(de)
        out     (c),a
        ld      d,(hl)
        inc     h
        ld      e,(hl)
        ld      a,(de)
        out     (c),a
        inc     d
        dec     h
        ld      a,(de)
        out     (c),a
        inc     d
        inc     l
        ld      a,(de)
        out     (c),a
        inc     d
        nop
        ld      a,(de)
        out     (c),a
        ld      d,(hl)
        inc     h
        ld      e,(hl)
        ld      a,(de)
        out     (c),a
        inc     d
        dec     h
        ld      a,(de)
        out     (c),a
        inc     d
        inc     l
        ld      a,(de)
        out     (c),a
        inc     d
        nop
        ld      a,(de)
        out     (c),a
        ld      d,(hl)
        inc     h
        ld      e,(hl)
        ld      a,(de)
        out     (c),a
        inc     d
        dec     h
        ld      a,(de)
        out     (c),a
        inc     d
        inc     l
        ld      a,(de)
        out     (c),a
        inc     d
        nop
        ld      a,(de)
        out     (c),a
        ld      d,(hl)
        inc     h
        ld      e,(hl)
        ld      a,(de)
        out     (c),a
        inc     d
        dec     h
        ld      a,(de)
        out     (c),a
        inc     d
        inc     l
        ld      a,(de)
        out     (c),a
        inc     d
        nop
        ld      a,(de)
        out     (c),a
        ld      d,(hl)
        inc     h
        ld      e,(hl)
        ld      a,(de)
        out     (c),a
        inc     d
        dec     h
        ld      a,(de)
        out     (c),a
        inc     d
        inc     l
        ld      a,(de)
        out     (c),a
        inc     d
        nop
        ld      a,(de)
        out     (c),a
        ld      d,(hl)
        inc     h
        ld      e,(hl)
        ld      a,(de)
        out     (c),a
        inc     d
        dec     h
        ld      a,(de)
        out     (c),a
        inc     d
        inc     l
        ld      a,(de)
        out     (c),a
        inc     d
        nop
        ld      a,(de)
        out     (c),a
        dec     b
        jp      nz,00003$
        ret

;; void hand_rows(void) -- s2_n rows from row s2_y through the compiled hand
;; overlay, entry index s2_hand = tc * HAND_ROWS + hand row.
_hand_rows::
        ld      a,(_s2_n)
        or      a
        ret     z
        ld      a,#HAND_BANK
        ld      (#0xFFFE),a
        ld      hl,(_s2_hand)
        add     hl,hl
        ld      de,#0x4000
        add     hl,de
        ld      a,(s2_hvar)
        exx
        ld      b,a             ; B' = 1: skip the solid hand tiles
        exx
        ld      e,(hl)
        inc     hl
        ld      d,(hl)
        ld      hl,#00001$
        push    hl
        push    de
        ld      a,(_s2_n)
        ld      b,a
        ld      a,(_s2_y)
        ld      l,a
        ld      h,#COLBUF
        ld      c,#0xBE
        ret
00001$:
        ld      a,(_s1_rdbank)
        ld      (#0xFFFE),a
        ret

;; void rd_ext(u8 chg) -- col_info extents only, changed = chg for all
_rd_ext::
        ld      (re_chg),a
        push    ix
        ld      iy,#_ray_s
        ld      de,#_col_info
        ld      b,#28
re_col:
        ld      a,(re_chg)
        jp      rs_ext

;; void rd_sig(void) -- per tile column: compare the rays with col_sig
;; ({s, tbank, tex lo, tex hi} per ray, updated in place) and fill
;; col_info[tc] = {changed, ymin, ymax, min s}.  tbank/tex only count when
;; the ray hit a wall (s != 0).
        .globl  _col_sig, _col_info, _ray_tex
_rd_sig::
        ld      a,#0xFF
        ld      (re_chg),a
        push    ix
        ld      iy,#_ray_s
        ld      ix,#_col_sig
        ld      hl,#_ray_tex
        ld      de,#_col_info
        ld      b,#28
rs_col:
        ld      c,#0
        ; left ray
        ld      a,0(iy)
        cp      0(ix)
        jr      z,00011$
        ld      0(ix),a
        inc     c
00011$: or      a
        jr      z,00012$
        ld      a,56(iy)
        cp      1(ix)
        jr      z,00013$
        ld      1(ix),a
        inc     c
00013$: ld      a,(hl)
        cp      2(ix)
        jr      z,00014$
        ld      2(ix),a
        inc     c
00014$: inc     hl
        ld      a,(hl)
        cp      3(ix)
        jr      z,00015$
        ld      3(ix),a
        inc     c
00015$: dec     hl
00012$: inc     hl
        inc     hl
        ; right ray
        ld      a,1(iy)
        cp      4(ix)
        jr      z,00021$
        ld      4(ix),a
        inc     c
00021$: or      a
        jr      z,00022$
        ld      a,57(iy)
        cp      5(ix)
        jr      z,00023$
        ld      5(ix),a
        inc     c
00023$: ld      a,(hl)
        cp      6(ix)
        jr      z,00024$
        ld      6(ix),a
        inc     c
00024$: inc     hl
        ld      a,(hl)
        cp      7(ix)
        jr      z,00025$
        ld      7(ix),a
        inc     c
00025$: dec     hl
00022$: inc     hl
        inc     hl
        ld      a,c
rs_ext:
        ld      (de),a
        inc     de
        ; a = max(sL, sR), c = min
        ld      a,0(iy)
        ld      c,1(iy)
        cp      c
        jr      nc,00031$
        ld      c,a
        ld      a,1(iy)
00031$: cp      #41
        jr      nc,00032$
        neg
        add     a,#40
        ld      (de),a          ; ymin = 40 - max
        inc     de
        neg
        add     a,#80
        ld      (de),a          ; ymax = 40 + max
        jr      00033$
00032$: xor     a
        ld      (de),a
        inc     de
        ld      a,#80
        ld      (de),a
00033$: inc     de
        ld      a,c
        ld      (de),a
        inc     de
        inc     iy
        inc     iy
        ld      a,(re_chg)
        inc     a
        jr      nz,00039$       ; rd_ext
        push    de
        ld      de,#8
        add     ix,de
        pop     de
        dec     b
        jp      nz,rs_col
        pop     ix
        ret
00039$: dec     b
        jp      nz,re_col
        pop     ix
        ret

;; void nt_update(void) -- nametable column nt_tc: rows < nt_tr0 sky, rows
;; > nt_tr1 ground, the rest the column's own tiles (tc * 10 + r); cells
;; already right (nt_state) are not written
        .globl  _nt_tc, _nt_tr0, _nt_tr1, _nt_state, _nt_hold
_nt_update::
        ld      a,(_nt_tc)
        ld      l,a
        ld      h,#0
        add     hl,hl
        ld      e,l             ; de = 2 tc
        ld      d,h
        add     hl,hl
        add     hl,hl           ; 8 tc
        add     hl,de           ; 10 tc
        ld      a,l
        ld      (nu_tile),a     ; first own tile (low byte; tc*10 < 280)
        ld      a,h
        ld      (nu_tile+1),a
        ld      de,#_nt_state
        add     hl,de           ; hl = nt_state + 10 tc
        ld      a,(_nt_tc)
        add     a,a
        add     a,#0x40 + 2 * 2 ; NT 0x3800 + (VIEW_TY0 * 32 + VIEW_TX0 + tc) * 2
        ld      e,a
        ld      d,#0x38 | 0x40  ; write address high byte
        ld      a,(_nt_tr0)
        ld      b,a
        ld      a,(_nt_tr1)
        inc     a
        ld      c,a
        xor     a               ; a = r
nu_row:
        push    af
        cp      b
        jr      c,00041$        ; r < tr0: sky (0)
        cp      c
        jr      nc,00042$       ; r > tr1: ground (1)
        ld      a,#2
        jr      00043$
00041$: xor     a
        jr      00043$
00042$: ld      a,#1
00043$: cp      (hl)
        jr      z,00049$
        ld      (hl),a
        push    hl
        or      a
        jr      nz,00044$
        ld      hl,#280         ; TILE_SKY
        jr      00046$
00044$: dec     a
        jr      nz,00045$
        ld      hl,#281         ; TILE_GROUND
        jr      00046$
00045$: pop     hl
        pop     af
        push    af
        push    hl
        push    de
        ld      hl,(nu_tile)
        ld      e,a
        ld      d,#0
        add     hl,de
        pop     de
00046$: ld      a,(_nt_hold)
        or      a
        jr      nz,00048$       ; fizzle pending: nametable untouched
        di
        ld      a,e
        out     (#0xBF),a
        ld      a,d
        out     (#0xBF),a
        ei
        ld      a,l
        out     (#0xBE),a
        nop
        nop
        nop
        ld      a,h
        out     (#0xBE),a
00048$: pop     hl
00049$: inc     hl
        ld      a,e             ; next row: address + 64
        add     a,#64
        ld      e,a
        jr      nc,00047$
        inc     d
00047$: pop     af
        inc     a
        cp      #10
        jr      c,nu_row
        ret

;; void rd_cols(void) -- draw every tile column flagged in col_info: both
;; rays into colbuf, sprites (rd_sprites, C), stage 2 / hand to VRAM and
;; the nametable cells when the drawn row range changed
        .globl  _rd_tc, _rd_hy0, _rd_sprites, _numvis, _rd_hrows
        .globl  _col_tr0, _col_tr1
        .globl  _col_hstat
_rd_cols::
        xor     a
        ld      (_rd_tc),a
rc_loop:
        ld      a,(_rd_tc)
        ld      l,a
        ld      h,#0
        add     hl,hl
        add     hl,hl
        ld      de,#_col_info
        add     hl,de
        ld      a,(hl)
        or      a
        jp      z,rc_next
        inc     hl
        ld      a,(hl)          ; ymin
        inc     hl
        ld      c,(hl)          ; ymax
        cp      c
        jr      c,00001$
        ld      a,#5
        ld      (rc_tr0),a
        dec     a
        ld      (rc_tr1),a
        jp      rc_nt
00001$: rrca
        rrca
        rrca
        and     #0x1F
        ld      (rc_tr0),a
        add     a,a
        add     a,a
        add     a,a
        ld      (_dr_y0),a
        ld      (_s2_y),a
        ld      a,c
        dec     a
        and     #0xF8
        add     a,#8
        ld      (_dr_y1),a
        rrca
        rrca
        rrca
        dec     a
        ld      (rc_tr1),a
        ld      a,(_rd_tc)
        add     a,a
        ld      c,a
        ld      b,#COLBUF
        push    bc
        call    _draw_ray
        pop     bc
        inc     c
        inc     b
        call    _draw_ray
        ld      a,(_numvis)
        or      a
        call    nz,_rd_sprites
        ld      a,#PLANE_BANK
        ld      (#0xFFFF),a
        ; s2_vaddr = tc * 320 + tr0 * 32
        ld      a,(_rd_tc)
        ld      l,a
        ld      h,#0
        add     hl,hl
        add     hl,hl
        add     hl,hl
        add     hl,hl
        add     hl,hl
        add     hl,hl           ; 64 tc
        add     a,h
        ld      h,a             ; + 256 tc
        ld      a,(rc_tr0)
        rrca
        rrca
        rrca                    ; tr0 * 32 (tr0 < 10): rotate into both bytes
        ld      e,a
        and     #0xE0
        ld      d,a
        ld      a,e
        and     #0x1F
        ld      e,d
        ld      d,a
        add     hl,de
        ld      (_s2_vaddr),hl
        call    _s2_setaddr
        ; hand column?
        ld      a,(_rd_hrows)
        or      a
        jr      z,rc_plain
        ld      a,(_rd_tc)
        sub     #HAND_TC0
        jr      c,rc_plain
        cp      #HAND_TCOLS
        jr      nc,rc_plain
        ld      b,a
        ld      a,(_rd_hy0)
        ld      c,a
        ld      a,(_dr_y1)
        cp      c
        jr      c,rc_plain
        jr      z,rc_plain
        ld      a,(_dr_y0)
        cp      c
        jr      nc,00002$
        ld      a,c
00002$: ld      (rc_hs),a       ; hs = max(y0, hy0)
        ld      hl,#_dr_y0
        sub     (hl)
        ld      (_s2_n),a
        push    bc
        call    _s2_rows
        pop     bc
        ld      a,(rc_hs)
        ld      (_s2_y),a
        ld      c,a
        ld      a,(_dr_y1)
        sub     c
        ld      (_s2_n),a
        ; s2_hand = col * HAND_ROWS(48) + hs - hy0
        ld      l,b
        ld      h,#0
        add     hl,hl
        add     hl,hl
        add     hl,hl
        add     hl,hl
        ld      d,h
        ld      e,l
        add     hl,hl
        add     hl,de
        ld      a,(_rd_hy0)
        ld      c,a
        ld      a,(rc_hs)
        sub     c
        ld      e,a
        ld      d,#0
        add     hl,de
        ld      (_s2_hand),hl
        ; hand fully raised and already in VRAM: skip its solid tiles
        ld      e,b
        ld      d,#0
        ld      hl,#_col_hstat
        add     hl,de
        ld      a,(_rd_hrows)
        cp      #HAND_ROWS
        ld      a,#0
        jr      nz,00005$
        ld      a,(hl)
        ld      (hl),#1
00005$: ld      (s2_hvar),a
        call    _hand_rows
        jr      rc_nt
rc_plain:
        ld      a,(_dr_y0)
        ld      c,a
        ld      a,(_dr_y1)
        sub     c
        ld      (_s2_n),a
        call    _s2_rows
rc_nt:
        ld      a,(_rd_tc)
        ld      e,a
        ld      d,#0
        ld      hl,#_col_tr0
        add     hl,de
        ld      a,(rc_tr0)
        cp      (hl)
        jr      nz,00003$
        ld      hl,#_col_tr1
        add     hl,de
        ld      a,(rc_tr1)
        cp      (hl)
        jr      z,rc_next
00003$: ld      hl,#_col_tr0
        add     hl,de
        ld      a,(rc_tr0)
        ld      (hl),a
        ld      (_nt_tr0),a
        ld      hl,#_col_tr1
        add     hl,de
        ld      a,(rc_tr1)
        ld      (hl),a
        ld      (_nt_tr1),a
        ld      a,e
        ld      (_nt_tc),a
        call    _nt_update
rc_next:
        ld      hl,#_rd_tc
        inc     (hl)
        ld      a,(hl)
        cp      #28
        jp      c,rc_loop
        ret

;; void cast_all(void) -- every 4th ray, then each group of 4 refined: rays
;; between two rays on the same face are interpolated, the others cast
        .globl  _ray_u, _ray_wall, _ray_side, _ray_hit, _anim_tile, _cast_anim
_cast_all::
        ; coarse rays every 8 (and 52, 54, 55), then refine the gaps
        xor     a
00001$: ld      (_cr_c),a
        call    _cast_ray
        ld      a,(_cr_c)
        add     a,#8
        cp      #56
        jr      c,00001$
        ld      a,#52
        ld      (_cr_c),a
        call    _cast_ray
        ld      a,#54
        ld      (_cr_c),a
        call    _cast_ray
        ld      a,#55
        ld      (_cr_c),a
        call    _cast_ray
        ld      b,#0
00002$: ld      c,#8
        push    bc
        call    refine
        pop     bc
        ld      a,b
        add     a,#8
        ld      b,a
        cp      #48
        jr      c,00002$
        ld      bc,#0x3004
        call    refine
        ld      bc,#0x3402
        jp      refine

;; refine: b = ray a, c = k (2, 4 or 8; rays a and a + k are cast): fill
;; rays a + 1 .. a + k - 1 by interpolation where a and a + k see one face
;; (or neighbouring tiles of one plane), else cast the middle and recurse
refine:
        push    bc
        ld      a,b
        add     a,c
        ld      c,a
        call    same_face
        pop     bc
        jr      nz,rf_adj
        xor     a
        jr      rf_lerp
rf_adj: push    bc
        ld      a,b
        add     a,c
        ld      c,a
        call    adj_plane
        pop     bc
        jr      nz,rf_split
        ld      a,#1
rf_lerp:
        ld      (ip_adj),a
        ld      a,b
        ld      (ipq_a),a
        ld      a,c
        ld      (ip_k),a
        jp      lerp
rf_split:
        srl     c
        ld      a,b
        add     a,c
        push    bc
        ld      (_cr_c),a
        call    _cast_ray
        pop     bc
        ld      a,c
        cp      #1
        ret     z
        push    bc
        call    refine
        pop     bc
        ld      a,b
        add     a,c
        ld      b,a
        jp      refine

;; lerp: rays ipq_a + 1 .. ipq_a + ip_k - 1 linearly between rays ipq_a and
;; ipq_a + ip_k (ip_k a power of two): s, and u (ip_adj = 0) or the position
;; q = u of ray a / 64 + u of ray b (ip_adj = 1, neighbouring tiles of one
;; plane: q >= 64 switches to ray b texture).  Only s, tbank and tex are
;; written (interpolated rays are never compared).
lerp:
        push    ix
        push    iy
        ld      a,(ipq_a)
        ld      e,a
        ld      a,(ip_k)
        add     a,e
        ld      (ipq_b),a
        ld      c,a
        ld      d,#0
        ld      b,d
        ; end pointer (low byte) and ray b texture
        ld      hl,#_ray_s
        add     hl,bc
        ld      a,l
        ld      (lp_end),a
        ld      hl,#_ray_tbank
        add     hl,bc
        ld      a,(hl)
        ld      (lp_tbb),a
        ld      hl,#_ray_tex+1
        add     hl,bc
        add     hl,bc
        ld      a,(hl)
        and     #0xF0
        ld      (lp_bb),a
        ; s step
        ld      hl,#_ray_s
        add     hl,de
        ld      a,(hl)
        ld      (lp_as+1),a
        ld      hl,#_ray_s
        add     hl,bc
        ld      l,(hl)
        ld      h,d
        push    de
        ld      e,a
        or      a
        sbc     hl,de           ; sb - sa
        call    lp_shift
        ld      (lp_ds),hl
        pop     de
        ; q step
        ld      hl,#_ray_u
        add     hl,de
        ld      a,(hl)
        ld      (lp_aq+1),a
        ld      hl,#_ray_u
        add     hl,bc
        ld      l,(hl)
        ld      h,d
        ld      a,(ip_adj)
        or      a
        jr      z,00001$
        ld      a,#64
        add     a,l
        ld      l,a
00001$: ld      a,(lp_aq+1)
        push    de
        ld      e,a
        ld      d,#0
        or      a
        sbc     hl,de           ; qb - qa
        call    lp_shift
        ld      (lp_dq),hl
        pop     de
        ; pointers: ix = ray_s + a + 1, iy = ray_tex + 2 (a + 1)
        inc     de
        ld      ix,#_ray_s
        add     ix,de
        ld      iy,#_ray_tex
        add     iy,de
        add     iy,de
        dec     de
        ; ray a texture
        ld      hl,#_ray_tbank
        add     hl,de
        ld      c,(hl)
        ld      hl,#_ray_tex+1
        add     hl,de
        add     hl,de
        ld      a,(hl)
        and     #0xF0
        ld      b,a
        ld      a,#0x80
        ld      (lp_as),a
        ld      (lp_aq),a
        ld      hl,(lp_as)
        ld      de,(lp_ds)
        exx                     ; alt: hl acc_s, de ds, b base, c tbank
        ld      hl,(lp_aq)
        ld      de,(lp_dq)      ; main: hl acc_q, de dq
lp_loop:
        exx
        add     hl,de
        ld      (ix),h
        ld      0x38(ix),c      ; ray_tbank = ray_s + 56
        ld      a,b
        exx
        ld      c,a
        add     hl,de
        ld      a,h
        cp      #64
        jr      nc,lp_sw
lp_c:   rrca
        rrca
        ld      b,a
        and     #0xC0
        ld      (iy),a          ; low = u << 6
        ld      a,b
        and     #0x3F
        or      c
        ld      1(iy),a         ; high = base | u >> 2
        inc     ix
        inc     iy
        inc     iy
        ld      a,(lp_end)
        .db     0xDD,0xBD       ; cp ixl
        jr      nz,lp_loop
        pop     iy
        pop     ix
        ret
lp_sw:  ; position past the tile boundary: ray b texture from now on
        ld      a,h
        sub     #0x40
        ld      h,a
        exx
        ld      a,(lp_bb)
        ld      b,a
        ld      a,(lp_tbb)
        ld      c,a
        ld      0x38(ix),c
        ld      a,b
        exx
        ld      c,a
        ld      a,h
        jr      lp_c

;; hl = d * 256 / ip_k
lp_shift:
        ld      a,(ip_k)
        add     hl,hl
        add     hl,hl
        add     hl,hl
        add     hl,hl
        add     hl,hl
        cp      #8
        ret     z
        add     hl,hl
        cp      #4
        ret     z
        add     hl,hl
        ret

;; same_face: rays b and c hit the same face of the same tile -> Z
same_face:
        ld      d,#0
        ld      e,b
        ld      hl,#_ray_hit
        add     hl,de
        add     hl,de
        ld      a,(hl)
        inc     hl
        ld      h,(hl)
        ld      l,a
        or      h
        jr      z,00009$
        push    hl
        ld      e,c
        ld      hl,#_ray_hit
        add     hl,de
        add     hl,de
        ld      a,(hl)
        inc     hl
        ld      h,(hl)
        ld      l,a
        pop     de
        or      a
        sbc     hl,de
        ret     nz
        ld      d,#0
        ld      e,b
        ld      hl,#_ray_side
        add     hl,de
        ld      a,(hl)
        ld      e,c
        ld      hl,#_ray_side
        add     hl,de
        cp      (hl)
        ret     nz
        ld      e,b
        ld      hl,#_ray_wall
        add     hl,de
        ld      a,(hl)
        ld      e,c
        ld      hl,#_ray_wall
        add     hl,de
        cp      (hl)
        ret
00009$: inc     a               ; no hit: NZ
        ret

;; put_ray: ray pt_dst = ray pt_src with s = pt_s, u = pt_u
put_ray:
        ; only what is drawn: s, texture bank and column.  Interpolated
        ; rays are never compared (same_face / adj_plane use cast rays only),
        ; so wall / side / hit / u are not copied.
        ld      a,(pt_dst)
        ld      e,a
        ld      d,#0
        ld      hl,#_ray_s
        add     hl,de
        ld      a,(pt_s)
        ld      (hl),a
        ld      a,(pt_src)
        ld      c,a
        ld      b,d
        ld      hl,#_ray_tbank
        add     hl,bc
        ld      a,(hl)
        ld      hl,#_ray_tbank
        add     hl,de
        ld      (hl),a
        ld      hl,#_ray_tex+1
        add     hl,bc
        add     hl,bc
        ld      a,(hl)
        and     #0xF0           ; 0x80 | (w & 3) << 4
        ld      b,a
        ld      a,(pt_u)
        rrca
        rrca
        ld      c,a
        and     #0x3F
        or      b
        ld      b,a             ; high = .. | u >> 2
        ld      a,c
        and     #0xC0
        ld      c,a             ; low = u << 6
        ld      hl,#_ray_tex
        add     hl,de
        add     hl,de
        ld      (hl),c
        inc     hl
        ld      (hl),b
        ret

;; ip2: a = ray: ray a + 1 = mean of rays a and a + 2
ip2:
        ld      (pt_src),a
        ld      e,a
        inc     a
        ld      (pt_dst),a
        ld      d,#0
        ld      hl,#_ray_s
        add     hl,de
        ld      a,(hl)
        inc     hl
        inc     hl
        add     a,(hl)
        rra
        ld      (pt_s),a
        ld      hl,#_ray_u
        add     hl,de
        ld      a,(hl)
        inc     hl
        inc     hl
        add     a,(hl)
        rra
        ld      (pt_u),a
        jp      put_ray

;; ip4: a = ray: rays a + 1 .. a + 3 interpolated between a and a + 4
ip4:
        ld      (pt_src),a
        ld      e,a
        ld      d,#0
        ld      hl,#_ray_s
        add     hl,de
        ld      b,(hl)          ; sa
        inc     hl
        inc     hl
        inc     hl
        inc     hl
        ld      c,(hl)          ; sb
        ld      hl,#_ray_u
        add     hl,de
        ld      d,(hl)          ; ua
        inc     hl
        inc     hl
        inc     hl
        inc     hl
        ld      e,(hl)          ; ub
        ld      (ip_v),bc       ; ip_v = sb, ip_v + 1 = sa
        ld      (ip_w),de       ; ip_w = ub, ip_w + 1 = ua
        ld      a,b
        add     a,c
        rra
        ld      (ip_sm),a
        ld      (pt_s),a
        ld      a,d
        add     a,e
        rra
        ld      (ip_um),a
        ld      (pt_u),a
        ld      a,(pt_src)
        add     a,#2
        ld      (pt_dst),a
        call    put_ray
        ld      a,(pt_src)
        inc     a
        ld      (pt_dst),a
        ld      a,(ip_v+1)
        ld      hl,#ip_sm
        add     a,(hl)
        rra
        ld      (pt_s),a
        ld      a,(ip_w+1)
        ld      hl,#ip_um
        add     a,(hl)
        rra
        ld      (pt_u),a
        call    put_ray
        ld      a,(pt_src)
        add     a,#3
        ld      (pt_dst),a
        ld      a,(ip_v)
        ld      hl,#ip_sm
        add     a,(hl)
        rra
        ld      (pt_s),a
        ld      a,(ip_w)
        ld      hl,#ip_um
        add     a,(hl)
        rra
        ld      (pt_u),a
        jp      put_ray

;; adj_plane: rays b and c hit the same side of neighbouring tiles of one
;; wall plane (side 1: tiles y +-1 in the same x column, side 0: x +-1) -> Z
adj_plane:
        ld      d,#0
        ld      e,b
        ld      hl,#_ray_side
        add     hl,de
        ld      a,(hl)
        ld      e,c
        ld      hl,#_ray_side
        add     hl,de
        cp      (hl)
        ret     nz
        ld      (ap_side),a
        ld      e,b
        ld      hl,#_ray_hit
        add     hl,de
        add     hl,de
        ld      a,(hl)
        inc     hl
        ld      h,(hl)
        ld      l,a
        or      h
        jr      z,00009$
        push    hl
        ld      e,c
        ld      hl,#_ray_hit
        add     hl,de
        add     hl,de
        ld      a,(hl)
        inc     hl
        ld      h,(hl)
        ld      l,a
        or      h
        pop     de              ; de = hit b, hl = hit c
        jr      z,00009$
        ld      a,l
        xor     e
        ld      b,a             ; low byte difference bits
        or      a
        sbc     hl,de           ; hl = hit c - hit b
        ld      a,(ap_side)
        or      a
        jr      z,00005$
        ; side 1: +-1 inside one 32 tile column
        ld      a,b
        and     #0xE0
        ret     nz
        ld      a,h
        or      a
        jr      nz,00002$
        ld      a,l
        dec     a               ; +1 -> Z
        ret
00002$: inc     a               ; h = 0xFF ?
        ret     nz
        ld      a,l
        inc     a               ; -1 -> Z
        ret
00005$: ; side 0: +-32
        ld      a,h
        or      a
        jr      nz,00006$
        ld      a,l
        cp      #32
        ret
00006$: inc     a
        ret     nz
        ld      a,l
        cp      #0xE0
        ret
00009$: inc     a               ; no hit: NZ (a was 0)
        ret

;; pq: a = position q along the pair (u of ray ipq_a, or 64 + u of ray
;; ipq_b); ray pt_dst = that ray with s = pt_s
pq:
        cp      #64
        jr      c,00001$
        sub     #64
        ld      (pt_u),a
        ld      a,(ipq_b)
        ld      (pt_src),a
        jp      put_ray
00001$: ld      (pt_u),a
        ld      a,(ipq_a)
        ld      (pt_src),a
        jp      put_ray

;; ipq2: a = ray: ray a + 1 between a and a + 2 on neighbouring tiles
ipq2:
        ld      (ipq_a),a
        ld      e,a
        inc     a
        ld      (pt_dst),a
        inc     a
        ld      (ipq_b),a
        ld      d,#0
        ld      hl,#_ray_s
        add     hl,de
        ld      a,(hl)
        inc     hl
        inc     hl
        add     a,(hl)
        rra
        ld      (pt_s),a
        ld      hl,#_ray_u
        add     hl,de
        ld      b,(hl)
        inc     hl
        inc     hl
        ld      a,(hl)
        add     a,#64
        add     a,b
        rra
        jp      pq

;; ipq4: a = ray: rays a + 1 .. a + 3 between a and a + 4 on neighbouring tiles
ipq4:
        ld      (ipq_a),a
        ld      e,a
        add     a,#4
        ld      (ipq_b),a
        ld      d,#0
        ld      hl,#_ray_s
        add     hl,de
        ld      b,(hl)          ; sa
        inc     hl
        inc     hl
        inc     hl
        inc     hl
        ld      c,(hl)          ; sb
        ld      hl,#_ray_u
        add     hl,de
        ld      d,(hl)          ; qa = ua
        inc     hl
        inc     hl
        inc     hl
        inc     hl
        ld      a,(hl)
        add     a,#64
        ld      e,a             ; qb = 64 + ub
        ld      (ip_v),bc       ; ip_v = sb, ip_v + 1 = sa
        ld      (ip_w),de       ; ip_w = qb, ip_w + 1 = qa
        ld      a,b
        add     a,c
        rra
        ld      (ip_sm),a
        ld      (pt_s),a
        ld      a,d
        add     a,e
        rra
        ld      (ip_um),a
        ld      b,a
        ld      a,(ipq_a)
        add     a,#2
        ld      (pt_dst),a
        ld      a,b
        call    pq
        ld      a,(ipq_a)
        inc     a
        ld      (pt_dst),a
        ld      a,(ip_v+1)
        ld      hl,#ip_sm
        add     a,(hl)
        rra
        ld      (pt_s),a
        ld      a,(ip_w+1)
        ld      hl,#ip_um
        add     a,(hl)
        rra
        call    pq
        ld      a,(ipq_a)
        add     a,#3
        ld      (pt_dst),a
        ld      a,(ip_v)
        ld      hl,#ip_sm
        add     a,(hl)
        rra
        ld      (pt_s),a
        ld      a,(ip_w)
        ld      hl,#ip_um
        add     a,(hl)
        rra
        jp      pq
