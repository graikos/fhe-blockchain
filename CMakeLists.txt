cmake_minimum_required(VERSION 2.8)
set(CMAKE_CXX_STANDARD 20)
set(CXX_STANDARD_REQUIRED ON)

project(libsetup)

set(
  CURVE
  "BN128"
  CACHE
  STRING
  "Default curve: one of ALT_BN128, BN128, EDWARDS, MNT4, MNT6"
)

set(
  DEPENDS_DIR
  "${CMAKE_CURRENT_SOURCE_DIR}/depends"
  CACHE
  STRING
  "Optionally specify the dependency installation directory relative to the source directory (default: inside dependency folder)"
)

set(
  OPT_FLAGS
  ""
  CACHE
  STRING
  "Override C++ compiler optimization flags"
)

option(
  MULTICORE
  "Enable parallelized execution, using OpenMP"
  ON
)

option(
  WITH_PROCPS
  "Use procps for memory profiling"
  ON
)

option(
  VERBOSE
  "Print internal messages"
  OFF
)

if(CMAKE_COMPILER_IS_GNUCXX OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
  # Common compilation flags and warning configuration
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -Wextra -Wfatal-errors -pthread")

  if("${MULTICORE}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fopenmp")
  endif()

   # Default optimizations flags (to override, use -DOPT_FLAGS=...)
  if("${OPT_FLAGS}" STREQUAL "")
    set(OPT_FLAGS "-ggdb3 -O2 -march=native -mtune=native")
  endif()
endif()

add_definitions(-DCURVE_${CURVE})

if(${CURVE} STREQUAL "BN128")
  add_definitions(-DBN_SUPPORT_SNARK=1)
endif()

if("${VERBOSE}")
  add_definitions(-DVERBOSE=1)
endif()

if("${MULTICORE}")
  add_definitions(-DMULTICORE=1)
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OPT_FLAGS}")

include(FindPkgConfig)
if("${WITH_PROCPS}")
  pkg_check_modules(PROCPS REQUIRED libprocps)
else()
  add_definitions(-DNO_PROCPS)
endif()


set(THIRDPARTYDIR ${CMAKE_CURRENT_SOURCE_DIR}/depends/zkOpenFHE/third-party)
include_directories(${DEPENDS_DIR})
include_directories(${THIRDPARTYDIR}/cereal/include)
include_directories(${CMAKE_BINARY_DIR}/depends/zkOpenFHE/src/core)
include_directories(${CMAKE_BINARY_DIR}/depends/zkOpenFHE/src/core)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/depends/zkOpenFHE/src/pke/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/depends/zkOpenFHE/src/pke/lib)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/depends/zkOpenFHE/src/core/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/depends/zkOpenFHE/src/core/lib)

include_directories(${DEPENDS_DIR}/libiop/depends/libff_liop)
include_directories(${THIRDPARTYDIR}/libsnark)
include_directories(${THIRDPARTYDIR}/libsnark/depends/libfqfft)
include_directories(${THIRDPARTYDIR}/libsnark/depends/libff)

include_directories(${DEPENDS_DIR}/libiop)

include_directories(${CMAKE_SOURCE_DIR}/incl)


add_subdirectory(depends)

find_package(Protobuf REQUIRED)


set(PROTO_FILES
    proto/message.proto
)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/")

protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILES})

if (Protobuf_FOUND)
    include_directories(${PROTOBUF_INCLUDE_DIR})
    include_directories(${CMAKE_CURRENT_BINARY_DIR})
else()
    message(FATAL_ERROR "Protobuf not found")
endif()

find_package(sodium REQUIRED)

# Source files
set(SOURCES
    src/net/conn_mngr.cpp
    src/net/router.cpp
    src/net/rpc_router.cpp
    src/net/peer.cpp
    src/util/util.cpp
    src/node/node.cpp
    src/msg/message.cpp
    src/net/rpc_server.cpp
    ${PROTO_SRCS}
    src/core/transaction_input.cpp
    src/core/transaction_output.cpp
    src/core/transaction.cpp
    src/core/coinbase_transaction.cpp
    src/core/block_header.cpp
    src/core/block.cpp
    src/core/merkle.cpp
    src/util/util.cpp
    src/store/mem_chainstate.cpp
    src/store/mem_blockstore.cpp
    src/store/mem_compstore.cpp
    src/store/mem_pool.cpp
    src/chain/chain.cpp
    src/chain/fork.cpp
    src/chain/chain_manager.cpp
    src/chain/miner.cpp
    src/computer/ast.cpp
    src/computer/fhe_computation.cpp
    src/computer/fhe_computer.cpp
    src/computer/concrete_computation_factory.cpp
    src/wallet/wallet.cpp
)


add_executable( main src/main.cpp ${SOURCES})

message(${PKELIBS}="${PKELIBS}")
target_link_libraries(main ${PKELIBS})
target_link_libraries(main PRIVATE snark)
target_link_libraries(main PRIVATE iop)
target_link_libraries(main PRIVATE sodium)
target_link_libraries(main ${Protobuf_LIBRARIES})

add_executable( conn src/conn.cpp ${SOURCES})

message(${PKELIBS}="${PKELIBS}")
target_link_libraries(conn ${PKELIBS})
target_link_libraries(conn PRIVATE snark)
target_link_libraries(conn PRIVATE iop)
target_link_libraries(conn PRIVATE sodium)
target_link_libraries(conn ${Protobuf_LIBRARIES})

add_executable(
	gen_comp


	src/gen.cpp
  src/computer/ast.cpp
  src/computer/fhe_computation.cpp
  src/computer/fhe_computer.cpp
  src/util/util.cpp
	)

message(${PKELIBS}="${PKELIBS}")
target_link_libraries(gen_comp ${PKELIBS})
target_link_libraries(gen_comp PRIVATE snark)
target_link_libraries(gen_comp PRIVATE iop)
target_link_libraries(gen_comp PRIVATE sodium)

add_executable(
	gen_keys


	src/gen_keys.cpp
	)

target_link_libraries(gen_keys PRIVATE sodium)

add_executable(
	oldmain

	src/oldmain.cpp
)

message(${PKELIBS}="${PKELIBS}")
target_link_libraries(oldmain ${PKELIBS})
target_link_libraries(oldmain PRIVATE snark)
target_link_libraries(oldmain PRIVATE iop)
# target_link_libraries(main PRIVATE sodium)

add_executable(
	decryptor

	src/decryptor.cpp
)

target_link_libraries(decryptor ${PKELIBS})
target_link_libraries(decryptor PRIVATE snark)
target_link_libraries(decryptor PRIVATE iop)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/config/ DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/config/)