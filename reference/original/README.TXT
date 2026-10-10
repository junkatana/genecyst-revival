=================================================================
 genecyst                                 Version x.xx (xx/xx/9x)
 by Bloodlust Software
=================================================================

Do not send any files without asking first.

Do not ask about SNESticle (or anything else you want emulated).


---------------------
     What it is
---------------------

genecyst is a DOS based emulator for the Sega Genesis.

The 68K, Z80, DAC, digital FM emulation and graphics
rendering were written from *scratch* in assembly. The rest is in C++.

It is freeware and it is not to be distributed with game
ROMs and it is not to be modified or sold.

Loads roms in the BIN, SMD or split (*.1) file formats.

OS/2 users: -disablelfn


---------------------
     Revisions
---------------------

Version x.xx: 
 - ????


---------------------
    What it does
---------------------

What is emulated:

   -68000 CPU @ 8Mhz
   -Z80 secondary CPU @ 4Mhz
   -DAC output
   -YM2612 FM synthesis
   -TI76489 PSG 
   -VDP
   -DMA 
   -ScrollA/ScrollB/Sprites/Window
   -Some raster effects
   -H/V interrupts
   -Dual 4-button/7-button joypad emulation
   -US/Japan/Europe variants

Other neat features:

   -Multiple save states and battery backed ram
   -Joystick support
   -VESA support
   -Save .pcx snapshots
   -Game genie codes
   -Rom checksum fixer
   -Wave output logging

---------------------
   What it lacks
---------------------

Shadow/highlighting effects
Interlaced modes

Many games do not work correctly (or at all).
Dont bother me if a ROM doesn't work.

---------------------
  What you need
---------------------

A Pentium with at least 8MB of RAM.

genecyst can take advantage of VESA 2.0 support if it exists.
If it does not exist, genecyst will resort to only the
video modes 320x200, 256x224, 256x240, 256x256, 320x224, 320x240.
The last 2 modes are planar (unchained) ModeX modes, meaning that
they are not linear and are very SLOW. Get a VESA extender such as Scitech
Display Doctor (www.scitechsoft.com) and use 320x240 instead.

---------------------
  Work it baby
---------------------

Use alt-<key> to access the menu options (ie alt-l loads)

The default keys for the first controller are ABC='zxc', Start='v', XYZ='asd',
and the arrow keys. They can be remapped from the Settings/Input device menu.
Enter/Tab will simulate Start/C on controller 1 if you have a 2 button joystick,
ONLY when the GUI is disabled or the game window has input focus (click on it)

6-button Genesis controllers can be enabled from the Settings menu. Having a
6-button controller enabled on older games will most likely cause them to go
haywire. The emulation of the 6-button controllers is only a guess, but it
seems to work for most games. Most games need to be reset if you change from
3 to 6 buttons. 6-button support will be automatically disabled for games
which don't support them as specified in their ROM header.

Some carts check to see what version of hardware they are running
on (US/Japan/Europe), and they'll bitch if played on the wrong system.
The setting in the Version menu is the default hardware preference. With 
Auto-Version on, if the default version isn't listed in the ROM header, 
the version will be automatically changed.

Some games contain battery backed RAM. This RAM is automatically
loaded and saved to "xxxx.GSV" files.

F5/F7 saves and loads states. Pressing the keyboard keys 0-9 (not the numeric
keypad ones) will change the current state slot for subsequent saves/loads.
The state filename follows the form "xxxx.GS?" where 'xxxx' is the romname
and ? is the current save slot (0-9).

Know that the battery backed RAM is saved in each state file. The
battery RAM from any state file that is loaded will overwrite
the ROM's .GSV file.

"Raster effects" refers to changes to the display made during a frame.
Many games use H-interrupts to change video registers and change the 
screen output as it is being drawn. H-ints are automatically enabled with 
raster effects on.

Raster effect emulation is by default off because its still a bit buggy.
For most games there will be little speed difference with or without
raster effects, however some games change the video registers many
times per frame. Games that do that will slow down a lot with raster
effects on. Palette-based raster effects aren't emulated yet, but they
are used quite frequently. Also, the raster effects will not persist
with the cpu paused. These problems will be remedied in later versions.
Examples of games that benefit from raster effects are TJ&E,G&G, sf2turbo,
and Landstalker.

Some games create a letterbox effect by turning the display
enable bit off near the bottom of the screen (usually after a H-interrupt).
The 'force display enable' menu option will display the screen regardless 
of the status of this bit. This is only needed for a few games, and only
if you choose to leave raster effects off.

The DMAfix option prevents writes to the VDP during DMA transfers
(fixes Superhydlide and Wiley wars).

---------------------
      Sound
---------------------

22050 is the default sample rate. 
22050 sucks, use 44100.
mono sucks, use stereo.

Genesis sound output is driven by a secondary Z80 processor running at 4Mhz,
however the 68000 can also control the sound hardware directly. The Genesis
uses a YM2612 sound chip which has six FM channels and one digital channel.

All sound can be disabled with the -nosound command line option.
Z80 emulation can be disabled with the -disablez80 command line option.

genecyst can log the YM2612/PSG register writes to a .GYM file. 
Someone (not me) could possibly write a utility to convert 
this data into a .MID or other music file. 
The GYM format works like this:  
 while (!EOF)
  switch (readbyte()) // readbyte() reads the next byte from the file
  {
   case 0: 1/60th of a second has elapsed; 
   case 1: ymport=0; ymreg=readbyte(); ymdata=readbyte();
   case 2: ymport=1; ymreg=readbyte(); ymdata=readbyte();
   case 3: write readbyte() to psg port
  }




---------------------
   ROM patching
---------------------


genecyst supports GameGenie codes and general patching of the
CPU ROM address space (0-3FFFFE) at even addresses.

Bring up the ROM patching dialog with F6 or from the CPU menu.
There are two edit fields in the dialog, the Name and the Code.
The Code field is where the actual code goes (eg SCRA-BJX0).
The Name field lets you type in a description of the code
(eg "Rings worth 2"), if no name is entered then the code string is
used as the name. Once you've filled these in, clicking "Add" (or
pressing enter) will add it to the list of patches. To activate
or deactivate the patch, doubleclick on the code name in the listbox
or click on "Toggle". "Remove" deletes the patch completely.
"Save" writes all the patches to the text file <romname>.pat,
this file is decribed below.

genecyst accepts either GameGenie codes or raw patches.
GameGenie codes are 8 digits long and use the symbols:
     ABCDEFGHJKLMNPRSTVWXYZ0123456789
The use of a '-' is optional.

The raw patches are in the format:
        <$addr>:<$val>
<$addr> is the 24-bit hex address (must be even)
<$val> is the 16-bit hex value to be patched at that address.

genecyst can save a .pat file that contains all the patches
created during a game. This file can be modified using a
text editor. Each line of the .pat file looks like this:
            <code> [<name>]
<code> is the patch itself (GG or raw format).
<name> is the text description of the code, if no name is supplied
        then the code is used as the name

Note: If more than one patch modifies the same address then only one
can be active at once. Activating one will deactivate the others.

Another Note: Most Genesis games do a rom checksum in order to verify
their integrity. Therefore, having a patch active during the ROM's startup
code could in most cases cause the ROM to crash itself. (Hence the use
of so called 'master codes')


---------------------
Command line options
---------------------


@<filename>     : Parses the file <filename> for command line options

-res <xw> <yw>  : Sets the resolution to xw,yw on startup.

-setinput <num> <type>
                : Sets input device <num> to <type>
-remapbut <device> <a> <b> <c> <start> <x> <y> <z>
                : Remaps buttons for <device>
                : <device> can be JOY1, JOY2, GRAVIS, GRIP1, GRIP2, 6BUTTON
-setkey <device> <ul> <u> <ur> <l> <r> <dl> <d> <dr> <a> <b> <c> <start> <x> <y> <z>
                : Sets the keyboard scancodes for <device>
                : <device> can be KEY1 or KEY2
-setjoythresh <num>  <left> <right> <up> <down>
                : Sets the joystick threshold for analog joystick <num>
                : left,right,up,down define the joystick's "dead-zone"
-analogjoyres <num>
                : Sets the maximum number of times the joystick port
                : is polled. Increase this number if the calibration values
                : seem to max out.

-waitvsync      : Wait for the vertical sync to draw frames
-hidegui        : Hide GUI on startup
-nomsgpopup     : Disables the last message from being shown on screen

-load <rom>     : Load <rom>
-run <rom>      : Load & Run <rom>

-romdir <dir>   : Sets the startup dir for roms
-savedir <dir>  : Sets the directory for .GS? files
-pcxdir <dir>   : Sets the directory for saving snapshots

-? or -h        : Display command line help
-disablelfn     : Disables long file name support
-novesa         : Don't use VESA extensions
-linear         : Force linear VESA mode
-banked         : Force banked VESA mode

-nosound        : Disables both FM and DAC

-disablez80     : Disables the Z80 processor (enabled by default)
-fakez80        : Turns Z80 faking on (disabled by default)

-rdtsc          : Enable the use of the pentium RDTSC profiling instruction
                : Enabling this on a non-Pentium will cause genecyst to crash.

-fpucopy        : Use the FPU for mem->vidmem copies
                : This will only provide speedups for pentium class processors

-version <type> : <type> can be USA, Japan, or Europe

-exclusive      : This switch will disable windows multitasking. This result in a speedup
                : although all your other apps will pause while genecyst is running.
                : Using this switch is similar to running genecyst under pure DOS, but without
                : having to exit windows.

---------------------
     Contact
---------------------

bldlust@maelstrom.net
http://????

Don't email me.

---------------------
  Acknowledgements
---------------------

The authors and contributors to the Sega Programming Faq

Ishmair for YM2612 information and help.

Neill Corlett for general 68K information

Merlyn LeRoy for the Sega Game Genie format

Special thanks to Loaded and Benny Boola Hill

Yuzo Koshiro would be proud

Marat Fayzullin, Jens Restemeier, Carl-Henrik Skårstedt, and Zophar

And Y0SHi for kindly donating webspace.

---------------------
     Disclaimer
---------------------

genecyst Copyright 1997,1998 Bloodlust Software
Sega and Genesis are trademarks of Sega Enterprises, Ltd.
Neither Bloodlust Software nor the author are affiliated with Sega
 nor advocate the piracy of Genesis games.
genecyst is freeware and can be distributed freely as long as it is not
 modified and ROMs are not packaged with the program.
When you use this software you do so at your own risk. The author is
 not responsible for any loss or damage resulting from the use or
 misuse of this software.
If you do not agree with these terms delete this software now.

