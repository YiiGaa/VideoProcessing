cmake_minimum_required(VERSION 3.5)

#the name of Compiled program
project(sample)

#c++ file root directory
FILE(GLOB ROOTCPP "${CMAKE_SOURCE_DIR}/*.cpp")

message("")
message("※target file:")
foreach(v ${ROOTCPP})
    message("   ${v}")
endforeach()

#link lib
message("")
set(LINKER_FLAGS "-lavformat -lavutil -lavcodec")
message("※dev lib:")
    message("   ${LINKER_FLAGS}")

#Building goals
foreach(v ${ROOTCPP})
    STRING( REGEX REPLACE "${CMAKE_SOURCE_DIR}/" "" prjName ${v} )
    STRING( REGEX REPLACE ".cpp" "" prjName ${prjName} )
    add_executable(${prjName} ${v} ${MODULECPP} ${COMMONCPP} ${CONFIGCPP} )
    target_link_libraries(${prjName} ${LINKER_FLAGS})
endforeach()

message("")

set(CMAKE_CXX_FLAGS "-std=c++11 ${CMAKE_CXX_FLAGS}")             # c++11
set(CMAKE_CXX_FLAGS "-g ${CMAKE_CXX_FLAGS}")                     # 调试信息
set(CMAKE_CXX_FLAGS "-Wall ${CMAKE_CXX_FLAGS}")                  # 开启所有警告