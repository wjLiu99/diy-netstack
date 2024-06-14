# cmake最低版本要求
cmake_minimum_required(VERSION 3.7)

project(net)
set(CMAKE_C_STANDARD 99)


# set(CMAKE_C_FLAGS "-g")
add_definitions("-Wall -g")
# set(CMAKE_BUILD_TYPE "Debug")


include_directories(
        ${PROJECT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/src/net/net 
        ${PROJECT_SOURCE_DIR}/src/plat
        ${PROJECT_SOURCE_DIR}/src/app
)


file(GLOB_RECURSE SOURCE_LIST "src/*.c" "src/*.h")

add_executable(${PROJECT_NAME} ${SOURCE_LIST})

add_definitions(-DNET_DRIVER_PCAP)    # use pcap


add_definitions(-DSYS_PLAT_LINUX)
target_link_libraries(${PROJECT_NAME} pthread pcap)



