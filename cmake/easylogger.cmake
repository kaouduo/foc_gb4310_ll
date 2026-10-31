include(FetchContent)

set(EASYLOGGER_COMMIT "806328e131836662fa83dd43364a53f699fd76ac")
set(EASYLOGGER_ARCHIVE_SHA256 "8836303efe2a90afe3081fbedb106129f63c7092206cf0995e81f8ff8cc82fe0")
set(EASYLOGGER_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/project/middleware/third_party/EasyLogger")

# 已存在目录只校验，不自动覆盖本地源码。
if(NOT EXISTS "${EASYLOGGER_SOURCE_DIR}")
    if(FETCHCONTENT_FULLY_DISCONNECTED)
        message(FATAL_ERROR "EasyLogger source is missing; configure online once before using disconnected mode")
    endif()
    FetchContent_Declare(easylogger_source
        URL "https://codeload.github.com/armink/EasyLogger/zip/${EASYLOGGER_COMMIT}"
        URL_HASH "SHA256=${EASYLOGGER_ARCHIVE_SHA256}"
        SOURCE_DIR "${EASYLOGGER_SOURCE_DIR}"
        SOURCE_SUBDIR source_only
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(easylogger_source)
endif()

# 清单包含全部直接编译输入、上游头文件及许可证，支持离线核验。
file(STRINGS "${CMAKE_CURRENT_LIST_DIR}/easylogger.sha256" EASYLOGGER_SOURCE_HASHES)
foreach(EASYLOGGER_ENTRY IN LISTS EASYLOGGER_SOURCE_HASHES)
    if(NOT EASYLOGGER_ENTRY MATCHES "^([0-9a-f]+)  (.+)$")
        message(FATAL_ERROR "Invalid EasyLogger checksum entry: ${EASYLOGGER_ENTRY}")
    endif()
    set(EASYLOGGER_EXPECTED_HASH "${CMAKE_MATCH_1}")
    set(EASYLOGGER_RELATIVE_PATH "${CMAKE_MATCH_2}")
    set(EASYLOGGER_CHECK_PATH "${EASYLOGGER_SOURCE_DIR}/${EASYLOGGER_RELATIVE_PATH}")
    if(NOT EXISTS "${EASYLOGGER_CHECK_PATH}")
        message(FATAL_ERROR "EasyLogger source is incomplete: ${EASYLOGGER_RELATIVE_PATH}")
    endif()
    file(SHA256 "${EASYLOGGER_CHECK_PATH}" EASYLOGGER_ACTUAL_HASH)
    if(NOT EASYLOGGER_ACTUAL_HASH STREQUAL EASYLOGGER_EXPECTED_HASH)
        message(FATAL_ERROR "EasyLogger source differs from pinned ${EASYLOGGER_COMMIT}: ${EASYLOGGER_RELATIVE_PATH}")
    endif()
endforeach()

# 只归档上游核心；移植钩子由固件的 porting/easylogger 提供。
add_library(easylogger STATIC
    ${EASYLOGGER_SOURCE_DIR}/easylogger/src/elog.c
    ${EASYLOGGER_SOURCE_DIR}/easylogger/src/elog_utils.c
)

target_include_directories(easylogger PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/project/porting/easylogger"
    "${EASYLOGGER_SOURCE_DIR}/easylogger/inc"
)

# PUBLIC 保证未来调用方与库内宏使用同一编译期日志等级。
target_compile_definitions(easylogger PUBLIC $<$<CONFIG:Debug>:FOC_LOG_DEBUG_BUILD=1>)
