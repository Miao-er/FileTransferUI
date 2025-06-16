cmake_minimum_required(VERSION 3.16)
project(FileUploadClient)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找 wxWidgets
find_package(wxWidgets REQUIRED COMPONENTS core base)
include(${wxWidgets_USE_FILE})

# 源文件
file(GLOB_RECURSE SOURCES "interface/*.cpp") # "net/*.cpp")

# 头文件
file(GLOB_RECURSE HEADERS "interface/*.h" "net/*.h")
# 创建可执行文件
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# 链接 wxWidgets 库
target_link_libraries(${PROJECT_NAME} ${wxWidgets_LIBRARIES})

# 设置编译选项
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -pedantic)
endif()
