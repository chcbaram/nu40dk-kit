cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(nu40dk-kit)

# 지정한 폴더에 있는 파일만 포함한다.
#
file(GLOB SRC_FILES CONFIGURE_DEPENDS
  src/*.c
  src/ap/*.c  
)

# 하위폴더에 있는 파일까지 포함한다.
#
file(GLOB_RECURSE SRC_FILES_RECURSE CONFIGURE_DEPENDS
  src/ap/modules/*.c
  src/bsp/*.c
  src/common/*.c
  src/hw/*.c
)

target_sources(app PRIVATE 
  ${SRC_FILES}
  ${SRC_FILES_RECURSE}
)

target_include_directories(app PRIVATE 
  src  
  src/ap
  src/ap/modules
  src/ap/modules/common    
  src/bsp  
  src/common
  src/common/core
  src/common/hw/include
  src/hw
  src/hw/driver
  src/lib
  src/ap/modules/emul/gb
  src/ap/modules/emul/apu/minigb_apu
)

# minigb_apu: int16 스테레오 11025Hz (gb_run.c GB_AUDIO_RATE 와 같아야 함, 게임 실행 중 I2S 도 이 주파수)
target_compile_definitions(app PRIVATE MINIGB_APU_AUDIO_FORMAT_S16SYS AUDIO_SAMPLE_RATE=11025)


zephyr_linker_sources(SECTIONS src/bsp/ldscript/ldscript.ld)
