cmake_minimum_required(VERSION 3.24)

project(DTMail
    LANGUAGES C
    VERSION 0.4
)

set(CMAKE_BUILD_TYPE Debug)

set(CMAKE_C_STANDARD 23)

include_directories(Graphics)
link_directories(lib)

add_library(utils util.c)

add_executable(mailserver mailserver.c)

target_link_libraries(utils PRIVATE -lws2_32)
FILE(COPY etc/iup.rc DESTINATION "${CMAKE_BINARY_DIR}")
target_link_libraries(mailserver PRIVATE utils -lws2_32 -liup -lgdi32 -lcomdlg32 -lcomctl32 -luuid -loleaut32 -lole32)