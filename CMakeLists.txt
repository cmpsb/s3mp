cmake_minimum_required(VERSION 3.7 FATAL_ERROR)
project(s3mp LANGUAGES C)

add_subdirectory(src/slopt)

include(FindPkgConfig)

pkg_search_module(SDL REQUIRED sdl2)
pkg_search_module(SDL_MIXER REQUIRED SDL2_mixer)

include_directories(${SDL_INCLUDE_DIRS} ${SDL_MIXER_INCLUDE_DIRS})

set(CMAKE_C_FLAGS "-march=native -O3 -flto -Wall -Wextra -pedantic")

add_executable(s3mp src/main.c src/s3m.c src/audio.c)
target_link_libraries(s3mp slopt m samplerate ${SDL_LIBRARIES} ${SDL_MIXER_LIBRARIES})
