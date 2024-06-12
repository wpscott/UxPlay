cmake_minimum_required(VERSION 3.5)
aux_source_directory(. hlsparse_src)
set(DIR_SRCS ${hlsparse_src})
include_directories(.)
add_library( hlsparse
        STATIC
        ${DIR_SRCS})

