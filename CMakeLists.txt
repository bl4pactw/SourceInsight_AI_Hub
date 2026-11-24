cmake_minimum_required(VERSION 3.16)
project(sihub VERSION 0.8.0 LANGUAGES C CXX RC)

set(LIBSSH2_DIR ${CMAKE_SOURCE_DIR}/third_party/libssh2)
set(CMARK_DIR ${CMAKE_SOURCE_DIR}/third_party/cmark-gfm)
set(WEBVIEW2_DIR ${CMAKE_SOURCE_DIR}/third_party/webview2)

set(LIBSSH2_SOURCE_NAMES
    agent bcrypt_pbkdf channel comp chacha cipher-chachapoly crypt crypto global hostkey
    keepalive kex knownhost mac misc packet pem poly1305 publickey scp session sftp
    transport userauth userauth_kbd_packet version)
list(TRANSFORM LIBSSH2_SOURCE_NAMES PREPEND ${LIBSSH2_DIR}/src/ OUTPUT_VARIABLE LIBSSH2_SOURCES)
list(TRANSFORM LIBSSH2_SOURCES APPEND .c)

add_library(sihub_libssh2 STATIC ${LIBSSH2_SOURCES})
target_include_directories(sihub_libssh2 PUBLIC ${LIBSSH2_DIR}/include)
target_compile_definitions(sihub_libssh2 PRIVATE LIBSSH2_WINCNG LIBSSH2_ECDSA_WINCNG)
target_link_libraries(sihub_libssh2 PUBLIC ws2_32 bcrypt crypt32)

file(GLOB CMARK_SOURCES ${CMARK_DIR}/src/*.c ${CMARK_DIR}/extensions/*.c)
if(MSVC)
    set(CMARK_GENERATED_DIR ${CMARK_DIR}/generated/windows)
    set_source_files_properties(${CMARK_SOURCES} PROPERTIES LANGUAGE CXX)
else()
    set(CMARK_GENERATED_DIR ${CMARK_DIR}/generated/unix)
endif()

add_library(sihub_cmark STATIC ${CMARK_SOURCES})
target_include_directories(sihub_cmark PUBLIC ${CMARK_DIR}/src ${CMARK_DIR}/extensions ${CMARK_GENERATED_DIR})
target_compile_definitions(sihub_cmark PUBLIC CMARK_GFM_STATIC_DEFINE CMARK_GFM_EXTENSIONS_STATIC_DEFINE)
set_target_properties(sihub_cmark PROPERTIES C_STANDARD 99)

file(GLOB SIHUB_SOURCES ${CMAKE_SOURCE_DIR}/src/*.c)
set_source_files_properties(${CMAKE_SOURCE_DIR}/res/sihub.rc PROPERTIES OBJECT_DEPENDS "${CMAKE_SOURCE_DIR}/res/viewer.html;${CMAKE_SOURCE_DIR}/res/sihub.manifest;${CMAKE_SOURCE_DIR}/res/sihub.ico")
add_executable(sihub WIN32 ${SIHUB_SOURCES} ${CMAKE_SOURCE_DIR}/res/sihub.rc)
target_include_directories(sihub PRIVATE ${CMAKE_SOURCE_DIR}/res ${WEBVIEW2_DIR})
target_compile_definitions(sihub PRIVATE UNICODE _UNICODE)
target_link_libraries(sihub PRIVATE sihub_libssh2 sihub_cmark shell32 user32 gdi32 ole32)
set_target_properties(sihub PROPERTIES C_STANDARD 99)

if(MSVC)
    target_compile_options(sihub PRIVATE /W4)
else()
    target_compile_options(sihub PRIVATE -Wall -Wextra)
    target_link_options(sihub PRIVATE -municode -static $<$<CONFIG:Release>:-s>)
endif()

if(CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64|aarch64" OR CMAKE_GENERATOR_PLATFORM MATCHES "ARM64")
    set(WEBVIEW2_ARCH arm64)
elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(WEBVIEW2_ARCH x64)
else()
    set(WEBVIEW2_ARCH x86)
endif()
set(WEBVIEW2_LOADER ${WEBVIEW2_DIR}/${WEBVIEW2_ARCH}/WebView2Loader.dll)

add_custom_command(TARGET sihub POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${WEBVIEW2_LOADER} $<TARGET_FILE_DIR:sihub>)

install(TARGETS sihub RUNTIME DESTINATION .)
install(FILES ${WEBVIEW2_LOADER} ${CMAKE_SOURCE_DIR}/sihub_templates.txt ${CMAKE_SOURCE_DIR}/sihub.ini.example DESTINATION .)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/remote DESTINATION .)
