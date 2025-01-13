set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

add_definitions(-DGIT_VERSION="\""$ENV{OSM_GIT_VERSION}"\"" -DGIT_SHA1="$ENV{OSM_GIT_SHA1}")

add_compile_options(-Wall
    -Werror
    -Wno-format          # int != int32_t as far as the compiler is concerned because gcc has int32_t as long int
    -Wno-unused-function # we have some for the docs that aren't called
    -Wno-unused-parameter
    -Wno-maybe-uninitialized
    -Wno-discarded-qualifier
    -Wextra
    -Wformat=2
    -Wcast-align
    -Wpointer-arith
    -Wwrite-strings
    -Wunreachable-code
    -Wstrict-aliasing=2
    -ffloat-store
    -fno-common
    -fstrict-aliasing
)


