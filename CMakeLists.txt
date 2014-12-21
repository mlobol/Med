project(Med)
cmake_minimum_required(VERSION 2.8.8)

set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt5Widgets REQUIRED)

add_definitions("-std=c++1y")
include_directories(src)
set(Med_SRCS src/editor/drbtree.cpp src/editor/buffer.cpp src/editor/buffers.cpp src/qt_gui/MainWindow.cpp)
add_library(Med ${Med_SRCS})
qt5_use_modules(Med Widgets)

add_executable(MedMain src/main.cpp)
target_link_libraries(MedMain Med)
install(TARGETS MedMain RUNTIME DESTINATION bin)

set(GMOCK_ROOT ${PROJECT_SOURCE_DIR}/gmock-1.7.0)
add_subdirectory(${GMOCK_ROOT})
set(GMOCK_MAIN_LIBRARIES ${GMOCK_ROOT}/libgmock_main.a)
set(GMOCK_LIBRARIES ${GMOCK_ROOT}/libgmock.a)
set(GMOCK_INCLUDE_DIRS ${GMOCK_ROOT}/include)

enable_testing()
set(GTEST_ROOT ${GMOCK_ROOT}/gtest)
find_package(GTest REQUIRED)
include_directories(${GMOCK_INCLUDE_DIRS} ${GTEST_INCLUDE_DIRS})

set(MedTest_SRCS src/editor/drbtree_test.cpp)
add_executable(MedTest ${MedTest_SRCS})
target_link_libraries(MedTest Med ${GMOCK_MAIN_LIBRARIES} ${GMOCK_LIBRARIES} ${GTEST_LIBRARIES} -lpthread)

add_test(MedTest MedTest)
