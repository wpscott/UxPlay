cmake_minimum_required(VERSION 3.5)
aux_source_directory(. MediaDataStore_src)
set(DIR_SRCS ${MediaDataStore_src})
include_directories(.)
add_library( MediaDataStore
        STATIC
        ${DIR_SRCS})

