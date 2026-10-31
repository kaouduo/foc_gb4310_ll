include(FetchContent)

set(CHERRYUSB_VERSION "v1.6.1")
set(CHERRYUSB_COMMIT "c9625ffa773ad10b8824d1b5361bca2ccc1f3d1e")
set(CHERRYUSB_ARCHIVE_SHA256 "04862e5ffcbacc5747bc1b0fa068d58be1e307761bd2076e78c6ceaeda9f028b")
set(CHERRYUSB_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/project/middleware/third_party/CherryUSB")

# 目录已存在时必须校验原始内容，不覆盖本地文件或静默混用版本。
if(NOT EXISTS "${CHERRYUSB_SOURCE_DIR}")
    if(FETCHCONTENT_FULLY_DISCONNECTED)
        message(FATAL_ERROR "CherryUSB source is missing; configure online once before using disconnected mode")
    endif()
    FetchContent_Declare(cherryusb_source
        URL "https://codeload.github.com/cherry-embedded/CherryUSB/zip/refs/tags/${CHERRYUSB_VERSION}"
        URL_HASH "SHA256=${CHERRYUSB_ARCHIVE_SHA256}"
        SOURCE_DIR "${CHERRYUSB_SOURCE_DIR}"
        SOURCE_SUBDIR source_only
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(cherryusb_source)
endif()

set(CHERRYUSB_VERSION_HEADER "${CHERRYUSB_SOURCE_DIR}/common/usb_version.h")
if(NOT EXISTS "${CHERRYUSB_VERSION_HEADER}")
    message(FATAL_ERROR "CherryUSB source is incomplete: common/usb_version.h is missing")
endif()
file(READ "${CHERRYUSB_VERSION_HEADER}" CHERRYUSB_VERSION_CONTENT)
if(NOT CHERRYUSB_VERSION_CONTENT MATCHES "#define[ \t]+CHERRYUSB_VERSION[ \t]+0x010601([ \r\n]|$)"
   OR NOT CHERRYUSB_VERSION_CONTENT MATCHES "#define[ \t]+CHERRYUSB_VERSION_STR[ \t]+\"v1\\.6\\.1\"")
    message(FATAL_ERROR "CherryUSB source version differs from pinned ${CHERRYUSB_VERSION} (${CHERRYUSB_COMMIT})")
endif()

# 清单来自已校验的发布归档；检查本次构建使用的源码和头文件，支持离线核验。
file(STRINGS "${CMAKE_CURRENT_LIST_DIR}/cherryusb.sha256" CHERRYUSB_SOURCE_HASHES)
foreach(CHERRYUSB_ENTRY IN LISTS CHERRYUSB_SOURCE_HASHES)
    if(NOT CHERRYUSB_ENTRY MATCHES "^([0-9a-f]+)  (.+)$")
        message(FATAL_ERROR "Invalid CherryUSB checksum entry: ${CHERRYUSB_ENTRY}")
    endif()
    set(CHERRYUSB_EXPECTED_HASH "${CMAKE_MATCH_1}")
    set(CHERRYUSB_RELATIVE_PATH "${CMAKE_MATCH_2}")
    set(CHERRYUSB_CHECK_PATH "${CHERRYUSB_SOURCE_DIR}/${CHERRYUSB_RELATIVE_PATH}")
    if(NOT EXISTS "${CHERRYUSB_CHECK_PATH}")
        message(FATAL_ERROR "CherryUSB source is incomplete: ${CHERRYUSB_RELATIVE_PATH} is missing")
    endif()
    file(SHA256 "${CHERRYUSB_CHECK_PATH}" CHERRYUSB_ACTUAL_HASH)
    if(NOT CHERRYUSB_ACTUAL_HASH STREQUAL CHERRYUSB_EXPECTED_HASH)
        message(FATAL_ERROR "CherryUSB source differs from pinned ${CHERRYUSB_VERSION}: ${CHERRYUSB_RELATIVE_PATH}")
    endif()
endforeach()

add_library(cherryusb_device STATIC
    ${CHERRYUSB_SOURCE_DIR}/core/usbd_core.c
    ${CHERRYUSB_SOURCE_DIR}/class/cdc/usbd_cdc_acm.c
    ${CHERRYUSB_SOURCE_DIR}/port/fsdev/usb_dc_fsdev.c
)
target_include_directories(cherryusb_device PUBLIC
    "${CMAKE_CURRENT_LIST_DIR}/../project/porting/cherryusb"
    "${CHERRYUSB_SOURCE_DIR}/common"
    "${CHERRYUSB_SOURCE_DIR}/core"
    "${CHERRYUSB_SOURCE_DIR}/class/cdc"
)
# 上游通用回调保留未使用参数，仅对第三方库关闭此项警告。
target_compile_options(cherryusb_device PRIVATE -Wno-unused-parameter)
