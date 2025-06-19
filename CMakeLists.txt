cmake_minimum_required(VERSION 3.16)
project(FileUploadClient)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找 wxWidgets
find_package(wxWidgets REQUIRED COMPONENTS core base)
include(${wxWidgets_USE_FILE})

# 项目1: FileUploadClient (原项目)
file(GLOB_RECURSE CLIENT_SOURCES "src/interface/*.cpp" "src/utils/*.cpp" "src/net/*.app" "src/service/client.cpp")
file(GLOB_RECURSE CLIENT_HEADERS "src/interface/*.h" "src/utils/*.h" "src/net/*.h")

add_executable(FileUploadClient ${CLIENT_SOURCES} ${CLIENT_HEADERS})
target_link_libraries(FileUploadClient ${wxWidgets_LIBRARIES} ibverbs)

# 项目2: FileUploadServer (新项目)
file(GLOB_RECURSE SERVER_SOURCES "src/interface/*.cpp" "src/utils/*.cpp" "src/net/*.app" "src/service/server.cpp")
file(GLOB_RECURSE SERVER_HEADERS "src/server/*.h" "src/utils/*.h" "src/net/*.h")

add_executable(FileUploadServer ${SERVER_SOURCES} ${SERVER_HEADERS})
target_link_libraries(FileUploadServer ${wxWidgets_LIBRARIES} ibverbs)

# 设置编译选项（应用到所有项目）
if(MSVC)
    target_compile_options(FileUploadClient PRIVATE /W4)
    target_compile_options(FileUploadServer PRIVATE /W4)
else()
    target_compile_options(FileUploadClient PRIVATE -Wall -Wextra -pedantic)
    target_compile_options(FileUploadServer PRIVATE -Wall -Wextra -pedantic)
endif()
