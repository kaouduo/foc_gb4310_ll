include(FetchContent)

set(CHERRYSH_COMMIT "c8c1c0964bc0033f24efb255cd607b35e3a19490")
set(CHERRYSH_ARCHIVE_SHA256 "45316d2517dfc470461c08141b5578954a6131d56a61d30c9ac4d8c65a833e93")
set(CHERRYSH_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/project/middleware/third_party/CherrySH")

# 已存在目录只校验，不自动覆盖本地源码。
if(NOT EXISTS "${CHERRYSH_SOURCE_DIR}")
    if(FETCHCONTENT_FULLY_DISCONNECTED)
        message(FATAL_ERROR "CherrySH source is missing; configure online once before using disconnected mode")
    endif()
    FetchContent_Declare(cherrysh_source
        URL "https://codeload.github.com/cherry-embedded/CherrySH/zip/${CHERRYSH_COMMIT}"
        URL_HASH "SHA256=${CHERRYSH_ARCHIVE_SHA256}"
        SOURCE_DIR "${CHERRYSH_SOURCE_DIR}"
        SOURCE_SUBDIR source_only
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(cherrysh_source)
endif()

# 清单包含全部直接编译输入、上游头文件及许可证，支持离线核验。
file(STRINGS "${CMAKE_CURRENT_LIST_DIR}/cherrysh.sha256" CHERRYSH_SOURCE_HASHES)
foreach(CHERRYSH_ENTRY IN LISTS CHERRYSH_SOURCE_HASHES)
    if(NOT CHERRYSH_ENTRY MATCHES "^([0-9a-f]+)  (.+)$")
        message(FATAL_ERROR "Invalid CherrySH checksum entry: ${CHERRYSH_ENTRY}")
    endif()
    set(CHERRYSH_EXPECTED_HASH "${CMAKE_MATCH_1}")
    set(CHERRYSH_RELATIVE_PATH "${CMAKE_MATCH_2}")
    set(CHERRYSH_CHECK_PATH "${CHERRYSH_SOURCE_DIR}/${CHERRYSH_RELATIVE_PATH}")
    if(NOT EXISTS "${CHERRYSH_CHECK_PATH}")
        message(FATAL_ERROR "CherrySH source is incomplete: ${CHERRYSH_RELATIVE_PATH}")
    endif()
    file(SHA256 "${CHERRYSH_CHECK_PATH}" CHERRYSH_ACTUAL_HASH)
    if(NOT CHERRYSH_ACTUAL_HASH STREQUAL CHERRYSH_EXPECTED_HASH)
        message(FATAL_ERROR "CherrySH source differs from pinned ${CHERRYSH_COMMIT}: ${CHERRYSH_RELATIVE_PATH}")
    endif()
endforeach()

# 阶段 0 归档目标参与构建，但不链接到固件，不要求移植钩子已实现。
add_library(cherrysh STATIC
    ${CHERRYSH_SOURCE_DIR}/chry_shell.c
    ${CHERRYSH_SOURCE_DIR}/cherryrl/chry_readline.c
)

target_include_directories(cherrysh PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/project/porting/cherrysh"
    "${CHERRYSH_SOURCE_DIR}"
)

# 上游回调保留未使用参数，仅对第三方目标关闭此项警告。
target_compile_options(cherrysh PRIVATE -Wno-unused-parameter)
# 上游使用 POSIX strnlen；仅此库启用 Newlib 的声明，不降低严格标准检查。
target_compile_definitions(cherrysh PRIVATE _POSIX_C_SOURCE=200809L)
