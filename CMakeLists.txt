cmake_minimum_required(VERSION 3.16)
project(FaceIDProtector)
set(CMAKE_CXX_STANDARD 20)

set(CMAKE_PREFIX_PATH "C:/Qt/6.10.2/msvc2022_64")

set(VCPKG "C:/vcpkg/installed/x64-windows")
set(OpenCV_DIR "${VCPKG}/share/opencv4")
set(Protobuf_DIR "${VCPKG}/share/protobuf")
set(flatbuffers_DIR "${VCPKG}/share/flatbuffers")
set(absl_DIR "${VCPKG}/share/absl")
set(utf8_range_DIR "${VCPKG}/share/utf8-range")
set(quirc_DIR "${VCPKG}/share/quirc")
set(TIFF_LIBRARY "${VCPKG}/lib/tiff.lib")
set(TIFF_INCLUDE_DIR "${VCPKG}/include")
set(JPEG_LIBRARY "${VCPKG}/lib/jpeg.lib")
set(JPEG_INCLUDE_DIR "${VCPKG}/include")
set(PNG_LIBRARY "${VCPKG}/lib/libpng16.lib")
set(PNG_PNG_INCLUDE_DIR "${VCPKG}/include")
set(ZLIB_LIBRARY "${VCPKG}/lib/zlib.lib")
set(ZLIB_INCLUDE_DIR "${VCPKG}/include")
set(WebP_DIR "${VCPKG}/share/webp")

list(APPEND CMAKE_PREFIX_PATH "${VCPKG}")

set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)
find_package(OpenCV REQUIRED COMPONENTS core imgproc highgui objdetect face videoio)

add_executable(FaceIDProtector WIN32 main.cpp)

# UAC — запрос прав администратора
target_link_options(FaceIDProtector PRIVATE
    "/MANIFESTUAC:level='requireAdministrator' uiAccess='false'"
)

target_link_libraries(FaceIDProtector PRIVATE
    Qt6::Widgets
    ${OpenCV_LIBS}
    shell32
    ole32
    uuid
    advapi32
    winhttp
    rpcrt4
)

target_include_directories(FaceIDProtector PRIVATE ${OpenCV_INCLUDE_DIRS})
