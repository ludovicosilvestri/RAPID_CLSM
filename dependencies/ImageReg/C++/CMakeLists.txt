project(ImageReg)

find_package(ITK REQUIRED)
include(${ITK_USE_FILE})

add_library(ImageReg SHARED ImageReg.cpp)

target_link_libraries(ImageReg ${ITK_LIBRARIES})