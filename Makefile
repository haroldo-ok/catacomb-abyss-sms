# Catacomb Abyss for the Sega Master System
DK      ?= $(HOME)/.devkitsms
# SDCC 4.4.0: 4.5.0 #15242 miscompiles "if (a != g) { g = a; ... }" with a in
# register A (the compare result overwrites a)
SDCCBIN ?= $(HOME)/sdcc440/sdcc-4.4.0/bin
CC      = $(SDCCBIN)/sdcc
CFLAGS  = -mz80 -Isrc -Isrc/game -Igen --peep-file $(DK)/include/peep-rules.txt --opt-code-speed
AS      = $(SDCCBIN)/sdasz80
OUT     = build
ROM     = catacomb_abyss.sms
# first bank of the packed data (gen/data.bin); code banks are 1..DATA_BANK-1
DATA_BANK = 8

# bank 0 (fixed) sources
FIXED   = src/main.c src/hw.c src/screen.c src/snd.c src/render.c src/plat0.c \
          gen/assets_tab.c gen/states.c src/game/c4_const.c
# banked sources (code and constants in the same bank)
BANK1   = src/game/c4_main.c src/game/c4_game.c src/game/c4_debug.c src/game/c4_draw.c
BANK2   = src/game/c4_play.c src/game/c4_state.c
BANK3   = src/game/c4_wiz.c
BANK4   = src/game/c4_act1.c
BANK5   =
BANK6   = src/menu.c src/save.c src/intro.c
BANK7   = src/gfx.c src/hud.c src/vid.c src/input.c gen/gamefont.c
BANKS   = $(foreach b,1 2 3 4 5 6 7,$(if $(BANK$(b)),$(b)))

rel = $(OUT)/$(notdir $(1:.c=.rel))
FIXED_REL  = $(foreach f,$(FIXED),$(call rel,$(f)))
BANKED_REL = $(foreach b,$(BANKS),$(foreach f,$(BANK$(b)),$(call rel,$(f))))
ASM_REL    = $(OUT)/crt0.rel $(OUT)/header.rel $(OUT)/render_a.rel
HDRS       = src/*.h src/game/*.h gen/assets.h gen/states.h

all: $(ROM)

$(OUT):
	mkdir -p $(OUT)

gen/assets.h gen/assets.inc gen/assets_tab.c gen/data.bin: tools/*.py
	cd tools && python3 build_assets.py

$(OUT)/crt0.rel: src/crt0.s | $(OUT)
	$(AS) -plosgff -o $@ $<
$(OUT)/header.rel: src/header.s | $(OUT)
	$(AS) -plosgff -o $@ $<
$(OUT)/render_a.rel: src/render_a.s gen/assets.inc | $(OUT)
	$(AS) -plosgff -o $@ $<

define FIXED_RULE
$(call rel,$(1)): $(1) $(HDRS) | $(OUT)
	$(CC) $(CFLAGS) -c -o $$@ $$<
endef
define BANK_RULE
$(call rel,$(1)): $(1) $(HDRS) | $(OUT)
	$(CC) $(CFLAGS) --codeseg BANK$(2) --constseg BANK$(2) -c -o $$@ $$<
endef
$(foreach f,$(FIXED),$(eval $(call FIXED_RULE,$(f))))
$(foreach b,$(BANKS),$(foreach f,$(BANK$(b)),$(eval $(call BANK_RULE,$(f),$(b)))))

BANK_LOCS = $(foreach b,$(BANKS),-Wl-b_BANK$(b)=0x$(b)4000)

$(OUT)/game.ihx: $(ASM_REL) $(FIXED_REL) $(BANKED_REL)
	$(CC) -mz80 --no-std-crt0 -Wl-u --code-loc 0x0080 --data-loc 0xC000 $(BANK_LOCS) \
	    -o $@ $(ASM_REL) $(FIXED_REL) $(BANKED_REL)

$(ROM): $(OUT)/game.ihx gen/data.bin
	makesms -mbin gen/data.bin:0:$$(stat -c %s gen/data.bin):$$(($(DATA_BANK)*16384)) $(OUT)/game.ihx $(ROM)
	python3 tools/checkrom.py $(OUT)/game.map $(ROM) $(DATA_BANK)

clean:
	rm -rf $(OUT) $(ROM)
