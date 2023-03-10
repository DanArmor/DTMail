cmake_minimum_required(VERSION 3.24)

project(DTMail
    LANGUAGES C
    VERSION 1.0
)

set(CMAKE_BUILD_TYPE Release)

set(CMAKE_C_STANDARD 23)

include_directories(Graphics)
include_directories(inc)

link_directories(lib)

add_library(utils src/util.c)

add_library(guiutils src/guiutil.c)
add_library(smtpservice src/smtpservice.c)
add_library(pop3service src/pop3service.c)
add_library(base64 src/base64.c)

target_link_libraries(guiutils PRIVATE -lws2_32 -liup -lgdi32 -lcomdlg32 -lcomctl32 -luuid -loleaut32 -lole32)
target_link_libraries(utils PRIVATE guiutils -lws2_32 -liup -lgdi32 -lcomdlg32 -lcomctl32 -luuid -loleaut32 -lole32)
target_link_libraries(smtpservice PRIVATE base64 -lws2_32 -liup -lgdi32 -lcomdlg32 -lcomctl32 -luuid -loleaut32 -lole32)
target_link_libraries(pop3service PRIVATE -lws2_32 -liup -lgdi32 -lcomdlg32 -lcomctl32 -luuid -loleaut32 -lole32)

add_executable(mailserver src/mailserver.c)
add_executable(husers src/husers.c)

target_link_libraries(mailserver PRIVATE guiutils smtpservice pop3service utils -lws2_32 -liup -lgdi32 -lcomdlg32 -lcomctl32 -luuid -loleaut32 -lole32)
