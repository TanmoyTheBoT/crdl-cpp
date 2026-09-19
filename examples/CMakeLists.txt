# Example: Simple episode download
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/simple_download.cpp")
    add_executable(simple_download simple_download.cpp)
    target_link_libraries(simple_download PRIVATE crdl_core)
endif()

# Example: Batch download
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/batch_download.cpp")
    add_executable(batch_download batch_download.cpp)
    target_link_libraries(batch_download PRIVATE crdl_core)
endif()
cmake_minimum_required(VERSION 3.15)
add_executable(test_cpp_cdm_integration test_cpp_cdm_integration.cpp)
target_link_libraries(test_cpp_cdm_integration PRIVATE crdl_core)
target_include_directories(test_cpp_cdm_integration PRIVATE ${CMAKE_SOURCE_DIR}/include)
