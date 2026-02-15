cmake_minimum_required(VERSION 3.10)
project(axi_interconnect_kit LANGUAGES CXX)

option(AXI_KIT_BUILD_TESTS "Build AXI kit unit tests" ON)
option(AXI_KIT_BUILD_DEMOS "Build AXI kit demos" ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(AXI_KIT_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/axi_interconnect/include
    ${CMAKE_CURRENT_SOURCE_DIR}/sim_ddr/include
    ${CMAKE_CURRENT_SOURCE_DIR}/mmio/include
)

add_library(axi_kit_axi4 STATIC
    axi_interconnect/AXI_Interconnect.cpp
    sim_ddr/SimDDR.cpp
)
target_include_directories(axi_kit_axi4 PUBLIC ${AXI_KIT_INCLUDE_DIRS})
target_compile_features(axi_kit_axi4 PUBLIC cxx_std_20)

add_library(axi_kit_axi3 STATIC
    axi_interconnect/AXI_Interconnect_AXI3.cpp
    axi_interconnect/AXI_Router_AXI3.cpp
    sim_ddr/SimDDR_AXI3.cpp
    mmio/MMIO_Bus_AXI3.cpp
    mmio/UART16550_Device.cpp
)
target_include_directories(axi_kit_axi3 PUBLIC ${AXI_KIT_INCLUDE_DIRS})
target_compile_features(axi_kit_axi3 PUBLIC cxx_std_20)
if(AXI_KIT_BUILD_TESTS)
  target_compile_definitions(axi_kit_axi3 PRIVATE
      MMIO_TEST_BASE=0x200000
      MMIO_TEST_SIZE=0x1000
  )
endif()

if(AXI_KIT_BUILD_TESTS)
  enable_testing()

  add_executable(sim_ddr_test
      sim_ddr/sim_ddr_test.cpp
  )
  target_link_libraries(sim_ddr_test PRIVATE axi_kit_axi4)

  add_executable(axi_interconnect_test
      axi_interconnect/axi_interconnect_test.cpp
  )
  target_link_libraries(axi_interconnect_test PRIVATE axi_kit_axi4)

  add_executable(sim_ddr_axi3_test
      sim_ddr/sim_ddr_axi3_test.cpp
  )
  target_link_libraries(sim_ddr_axi3_test PRIVATE axi_kit_axi3)

  add_executable(axi_interconnect_axi3_test
      axi_interconnect/axi_interconnect_axi3_test.cpp
  )
  target_compile_definitions(axi_interconnect_axi3_test PRIVATE
      MMIO_TEST_BASE=0x200000
      MMIO_TEST_SIZE=0x1000
  )
  target_link_libraries(axi_interconnect_axi3_test PRIVATE axi_kit_axi3)

  add_executable(mmio_router_axi3_test
      mmio/mmio_router_axi3_test.cpp
  )
  target_compile_definitions(mmio_router_axi3_test PRIVATE
      MMIO_TEST_BASE=0x200000
      MMIO_TEST_SIZE=0x1000
  )
  target_link_libraries(mmio_router_axi3_test PRIVATE axi_kit_axi3)

  add_test(NAME sim_ddr_test COMMAND sim_ddr_test)
  add_test(NAME axi_interconnect_test COMMAND axi_interconnect_test)
  add_test(NAME sim_ddr_axi3_test COMMAND sim_ddr_axi3_test)
  add_test(NAME axi_interconnect_axi3_test COMMAND axi_interconnect_axi3_test)
  add_test(NAME mmio_router_axi3_test COMMAND mmio_router_axi3_test)
endif()

if(AXI_KIT_BUILD_DEMOS)
  add_executable(axi4_smoke_demo demos/axi4_smoke.cpp)
  target_link_libraries(axi4_smoke_demo PRIVATE axi_kit_axi4)

  add_executable(axi3_smoke_demo demos/axi3_smoke.cpp)
  target_link_libraries(axi3_smoke_demo PRIVATE axi_kit_axi3)
endif()
