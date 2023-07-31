cmake_minimum_required(VERSION 3.24.1)

set(Recipe_Name "Chess_cpp")
project(${Recipe_Name})
add_executable(${Recipe_Name})

source_group("include" REGULAR_EXPRESSION "inc/*")
source_group("source" REGULAR_EXPRESSION "src/*")

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

add_subdirectory(src)
add_subdirectory(inc)

# Define C++ version to be used for building the project
set_property(TARGET ${Recipe_Name} PROPERTY CXX_STANDARD 17)
set_property(TARGET ${Recipe_Name} PROPERTY CXX_STANDARD_REQUIRED ON)

# Define C version to be used for building the project
set_property(TARGET ${Recipe_Name} PROPERTY C_STANDARD 99)
set_property(TARGET ${Recipe_Name} PROPERTY C_STANDARD_REQUIRED ON)