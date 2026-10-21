"""Texts rewritten for the Master System (controls, cartridge, PAUSE).

Help pages: the Genesis port's pages with the Genesis specific ones
rewritten; the "- B: play ... -" footer is dropped (the viewer shows its own
page strip).  `^` centres a line, a TAB starts the second table column.
"""
import re
from common import D

FOOTER = b'^- B: play   LEFT/RIGHT: page   UP/DOWN: scroll -'

HELP_REPLACE = {
    1: """^TABLE OF CONTENTS FOR THIS FILE
^-------------------------------

^Getting Started

^Control Pad

^System Function Controls

^The Story So Far...

^Tips and Suggestions

^Note: The Registered Version of THE CATACOMB ABYSS includes an extra section containing GAME SOLUTIONS and CHEAT KEYS!

^< These pages are also in the USER'S GUIDE of the title screen >
""",
    2: """^GETTING STARTED
^---------------
*  The Catacomb Abyss for the Sega Master System runs directly from the cartridge.

*  Up to three games can be saved in the cartridge's battery backed memory.  Press PAUSE during play and choose Save or Restore.

*  The game is played with the control pad.  The next page lists what the buttons do.

*  The PC Speaker sound effects can be turned off and on: press PAUSE during play and choose Sound.
""",
    3: """^CONTROL PAD
^-----------

Button...\tResult...
UP/DOWN\tForward/Back
LEFT/RIGHT\tTurn
L/R twice\tQuick Turn
2 + L/R\tSidestep
1\tMagick Missile
tap 2\tZapper
2, then 1\tXterminator
2 + DOWN\tCure Potion
PAUSE\tOptions, Scrolls

Hold button 2 and press LEFT or RIGHT to step sideways.  Tap button 2 alone to fire a Zapper (a stream of missiles).  Hold 2 and press 1 for an Xterminator (a circle of missiles).  Hold 2 and press DOWN to drink a Cure Potion.  Tap LEFT or RIGHT twice and keep holding it for a Quick Turn.
""",
    5: """^SYSTEM FUNCTION CONTROLS
^------------------------

PAUSE (on the console) opens the options menu.  LEFT/RIGHT chooses, button 1 confirms, button 2 cancels.

Option...\tResult...
Help\tThese pages
Sound\tEffects on/off
Scrolls\tRead a scroll
Save\tSave the game
Restore\tLoad a saved game
New\tStart a new game
Quit\tBack to the title
""",
}

# simple substitutions on the remaining pages
HELP_SUBST = [
    (b'by pressing START and choosing Scrolls (or MODE on a 6 button pad)',
     b'by pressing PAUSE and choosing Scrolls'),
    (b'by pressing the B button', b'by pressing button 1'),
    (b'(hold A, or Z on a 6 button pad, while turning)',
     b'(tap LEFT or RIGHT twice and hold it)'),
    (b'Selecting "Restore" after pressing START', b'Selecting "Restore" after pressing PAUSE'),
    (b'You can press START at any time to pause', b'You can press PAUSE at any time to pause'),
    (b'                                                                                                ...more...',
     b'^...more...'),
]

LEVEL_TEXT_REPLACE = {
    15: b'For openers, read the Help (PAUSE), then...',
    18: b'Be ready with buttons 1 and 2...',
}

TITLE_GENESIS_WORDS = [b'START', b'Genesis', b'Mega Drive', b'6 button', b'BUTTON_', b' B button', b'MODE']


def help_pages():
    pages = []
    for i, p in enumerate(D['help_pages']):
        if i == 4:          # 6 button pad page: no such pad on the SMS
            continue
        if i in HELP_REPLACE:
            p = HELP_REPLACE[i].encode('latin1')
        else:
            p = p.replace(FOOTER, b'')
            for a, b in HELP_SUBST:
                p = p.replace(a, b)
        p = p.replace(FOOTER, b'').rstrip(b'\n') + b'\n'
        for w in TITLE_GENESIS_WORDS:
            assert not re.search(rb'\b' + re.escape(w.strip()) + rb'\b', p), (i, w, p)
        pages.append(p)
    return pages


def level_texts():
    lt = list(D['level_texts'])
    for k, v in LEVEL_TEXT_REPLACE.items():
        lt[k] = v
    return lt


LEVEL_NAMES = [
    "The Towne Cemetery",
    "The Garden of Tears",
    "The Den of Zombies",
    "The Mausoleum Grounds",
    "The Main Floor of the Mausoleum",
    "Mike's Blastable Passage",
    "The Crypt of Nemesis the Undead",
    "The Subterranean Vault",
    "The Ancient Aqueduct",
    "The Orc Mines",
    "The Lair of the Troll",
    "The Demon's Inferno",
    "The Battleground of the Titans",
    "The Coven of Mages",
    "The Inner Sanctum",
    "The Haunt of Nemesis",
    "The Passage to the Surface",
    "Big Jim's Domain",
    "Nolan's Nasty",
]


if __name__ == '__main__':
    for p in help_pages():
        print('=' * 40)
        print(p.decode('latin1'))
