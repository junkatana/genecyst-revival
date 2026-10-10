cmake_minimum_required(VERSION 3.16)
project(genecyst C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
find_package(ZLIB REQUIRED)

set(GPGX ${CMAKE_SOURCE_DIR}/third_party/genesis-plus-gx/core)

# --- Genesis Plus GX core (static library) ---------------------------------
set(CORE_SOURCES
  ${GPGX}/z80/z80.c
  ${GPGX}/m68k/m68kcpu.c
  ${GPGX}/m68k/s68kcpu.c
  ${GPGX}/genesis.c ${GPGX}/vdp_ctrl.c ${GPGX}/vdp_render.c ${GPGX}/system.c
  ${GPGX}/io_ctrl.c ${GPGX}/mem68k.c ${GPGX}/memz80.c ${GPGX}/membnk.c
  ${GPGX}/state.c ${GPGX}/loadrom.c
  ${GPGX}/input_hw/input.c ${GPGX}/input_hw/gamepad.c ${GPGX}/input_hw/lightgun.c
  ${GPGX}/input_hw/mouse.c ${GPGX}/input_hw/activator.c ${GPGX}/input_hw/xe_1ap.c
  ${GPGX}/input_hw/teamplayer.c ${GPGX}/input_hw/paddle.c ${GPGX}/input_hw/smash.c
  ${GPGX}/input_hw/sportspad.c ${GPGX}/input_hw/terebi_oekaki.c ${GPGX}/input_hw/graphic_board.c
  ${GPGX}/sound/sound.c ${GPGX}/sound/psg.c ${GPGX}/sound/ym2413.c ${GPGX}/sound/opll.c
  ${GPGX}/sound/ym3438.c ${GPGX}/sound/ym2612.c ${GPGX}/sound/blip_buf.c ${GPGX}/sound/eq.c
  ${GPGX}/cart_hw/sram.c ${GPGX}/cart_hw/svp/svp.c ${GPGX}/cart_hw/svp/ssp16.c
  ${GPGX}/cart_hw/ggenie.c ${GPGX}/cart_hw/areplay.c ${GPGX}/cart_hw/eeprom_93c.c
  ${GPGX}/cart_hw/eeprom_i2c.c ${GPGX}/cart_hw/eeprom_spi.c ${GPGX}/cart_hw/flash_cfi.c
  ${GPGX}/cart_hw/yx5200.c ${GPGX}/cart_hw/md_cart.c ${GPGX}/cart_hw/sms_cart.c ${GPGX}/cart_hw/megasd.c
  ${GPGX}/cd_hw/scd.c ${GPGX}/cd_hw/cdd.c ${GPGX}/cd_hw/cdc.c ${GPGX}/cd_hw/gfx.c
  ${GPGX}/cd_hw/pcm.c ${GPGX}/cd_hw/cd_cart.c
  ${GPGX}/ntsc/sms_ntsc.c ${GPGX}/ntsc/md_ntsc.c
  src/glue/config.c src/glue/error.c src/glue/fileio.c src/glue/unzip.c
)

add_library(gpgx_core STATIC ${CORE_SOURCES})
target_include_directories(gpgx_core PUBLIC
  ${GPGX} ${GPGX}/z80 ${GPGX}/m68k ${GPGX}/sound ${GPGX}/sound/minimp3 ${GPGX}/input_hw ${GPGX}/cart_hw
  ${GPGX}/cart_hw/svp ${GPGX}/cd_hw ${GPGX}/ntsc ${CMAKE_SOURCE_DIR}/src/glue)
target_compile_definitions(gpgx_core PUBLIC
  LSB_FIRST USE_32BPP_RENDERING MAXROMSIZE=33554432
  HAVE_YM3438_CORE HAVE_OPLL_CORE ENABLE_SUB_68K_ADDRESS_ERROR_EXCEPTIONS)
if(NOT WIN32)
  target_compile_definitions(gpgx_core PUBLIC HAVE_ALLOCA_H)
endif()
target_compile_options(gpgx_core PRIVATE -O2 -fomit-frame-pointer -Wno-strict-aliasing -w)
target_link_libraries(gpgx_core PUBLIC ZLIB::ZLIB m)

# --- Genecyst frontend ------------------------------------------------------
file(GLOB GUI_SOURCES ${CMAKE_SOURCE_DIR}/src/gui/*.c)
add_executable(genecyst src/main.c src/emu.c src/paths.c src/dialogs.c src/filters.c src/pads.c src/cfgfile.c ${GUI_SOURCES})
target_include_directories(genecyst PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_options(genecyst PRIVATE -Wall -Wextra -Wno-unused-parameter)
target_link_libraries(genecyst PRIVATE gpgx_core PkgConfig::SDL2)
if(WIN32)
  # SDL2main provides WinMain; -mwindows hides the console window
  target_link_libraries(genecyst PRIVATE mingw32 SDL2main)
  target_link_options(genecyst PRIVATE -mwindows -static-libgcc)
  if(MINGW)
    # ship a self-contained exe (static libgcc/winpthread); SDL2.dll stays dynamic
    target_link_options(genecyst PRIVATE -Wl,-Bstatic -lwinpthread -Wl,-Bdynamic)
  endif()
endif()
