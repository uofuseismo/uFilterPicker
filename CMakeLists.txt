cmake_minimum_required(VERSION 3.28)
project(uFilterPicker VERSION 0.0.1 LANGUAGES CXX)
enable_testing()
include(FetchContent)

option(BUILD_SHARED_LIBS "Build using shared libraries" OFF)
option(BUILD_TESTS "Compile unit tests" ON)
option(BUILD_TRAINING "Compile training modules" ON)
option(WITH_CONAN "Build using Conan" OFF)
option(USE_CLANG_TIDY "Build using clang-tidy" OFF)
include(CheckCXXCompilerFlag)
include(GenerateExportHeader)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)

# Boost
if(POLICY CMP0167)
  cmake_policy(SET CMP0167 NEW)
endif()
if (NOT BUILD_SHARED_LIBS)
   message("Will attempt to use Boost static libraries")
   set(Boost_USE_STATIC_LIBS ON) 
endif()

#find_package(absl CONFIG REQUIRED)
#find_package(utf8_range CONFIG REQUIRED)
find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)
#find_package(uSignal REQUIRED)
if (${WITH_CONAN})
   message("Will use Conan OTLP exporters")
   find_package(opentelemetry-cpp CONFIG COMPONENTS exporter_otlp_grpc exporters_otlp_http REQUIRED)
else()
   find_package(opentelemetry-cpp CONFIG COMPONENTS exporters_otlp_http REQUIRED)
endif()
find_package(spdlog REQUIRED)
find_package(Boost COMPONENTS headers program_options REQUIRED)
find_package(Threads REQUIRED)
find_package(Catch2 3 REQUIRED)
find_package(TBB COMPONENTS tbb REQUIRED)
#find_package(IPP COMPONENTS ippcore ipps ippvm REQUIRED)
if (${BUILD_TRAINING})
   find_package(libpqxx REQUIRED)
   find_package(GeographicLib REQUIRED)
   find_package(HDF5 NAMES hdf5 hdf5_cpp COMPONENTS C CXX REQUIRED)
endif()
if (${BUILD_TRAINING} OR ${BUILD_TESTS})
   find_package(libmseed REQUIRED)
   FetchContent_Declare(libmseed 
                        GIT_REPOSITORY https://github.com/EarthScope/libmseed.git
                        GIT_TAG        v3.2.24)
   
endif()

FetchContent_Declare(uSignal
                     SYSTEM
                     GIT_REPOSITORY https://github.com/uofuseismo/uSignal.git
                     GIT_TAG ebe08b6)
FetchContent_MakeAvailable(uSignal)


if (${USE_CLANG_TIDY} AND PROJECT_IS_TOP_LEVEL)
   message("Looking for clang-tidy")
   find_program(CLANG_TIDY_EXECUTABLE NAMES "clang-tidy")
   if (CLANG_TIDY_EXECUTABLE)
      message("Using clang-tidy  ${CLANG_TIDY_EXECUTABLE}")
      set(CMAKE_C_CLANG_TIDY   ${CLANG_TIDY_EXECUTABLE} -p ${CMAKE_BINARY_DIR})
      set(CMAKE_CXX_CLANG_TIDY ${CLANG_TIDY_EXECUTABLE} -p ${CMAKE_BINARY_DIR})
      #set(CLANG_TIDY_COMMAND ${CLANG_TIDY_EXECUTABLE} "-config=${CMAKE_SOURCE_DIR}/.clang-tidy")
      set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
   else()
      message("clang-tidy not found")
   endif()
endif()


set(LIBRARY_SRC
    src/options.cpp
    src/envelope.cpp
    src/characteristicFunction.cpp
    src/narrowBandFilter.cpp
    src/pipeline.cpp
    src/thresholdTrigger.cpp
    src/detector.cpp)
set(IMPORT_PROTO_SRC
    uDataPacketServiceAPI/v1/data_type.proto
    uDataPacketServiceAPI/v1/stream_identifier.proto
    uDataPacketServiceAPI/v1/packet.proto
    uDataPacketServiceAPI/v1/subscribe_to_all_request.proto
    uDataPacketServiceAPI/v1/subscription_request.proto
    uDataPacketServiceAPI/v1/broadcast.proto
    )   
set(LIBRARY_HEADER_FILES 
    include/uFilterPicker/characteristicFunction.hpp
    include/uFilterPicker/detector.hpp
    include/uFilterPicker/envelope.hpp
    include/uFilterPicker/narrowBandFilter.hpp
    include/uFilterPicker/thresholdTrigger.hpp)
set(LIBRARY_MODULE_FILES
    src/modules/utilities.cppm)

add_library(libuFilterPicker STATIC ${LIBRARY_SRC} ${IMPORT_PROTO_SRC})
add_library(uFilterPicker::libuFilterPicker ALIAS libuFilterPicker)
set_target_properties(libuFilterPicker PROPERTIES
                      OUTPUT_NAME libuFilterPicker
                      CXX_STANDARD 23
                      CXX_STANDARD_REQUIRED YES 
                      CXX_EXTENSIONS NO)
target_include_directories(libuFilterPicker
                           PUBLIC  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
                           PRIVATE $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
                           PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
                           PUBLIC  $<INSTALL_INTERFACE:/install>)
target_link_libraries(libuFilterPicker
                      PRIVATE protobuf::libprotobuf
                              spdlog::spdlog_header_only
                              gRPC::grpc
                              uSignal::libuSignal)
target_sources(libuFilterPicker
               PUBLIC FILE_SET HEADERS
                      BASE_DIRS
                         ${CMAKE_CURRENT_SOURCE_DIR}
                      FILES
                         ${LIBRARY_HEADER_FILES}
               PUBLIC FILE_SET CXX_MODULES
                      BASE_DIRS
                         src/modules
                      FILES
                         ${LIBRARY_MODULE_FILES})
protobuf_generate(TARGET libuFilterPicker
                  LANGUAGE cpp 
                 )
protobuf_generate(TARGET libuFilterPicker
                  LANGUAGE grpc
                  PLUGIN "protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>"
                  PLUGIN_OPTIONS generate_mock_code=true
                  GENERATE_EXTENSIONS .grpc.pb.h .grpc.pb.cc
                 )


#add_executable(uFilterPicker
#               src/filterPicker.cpp)
#set_target_properties(uFilterPicker PROPERTIES
#                      CXX_STANDARD 23
#                      CXX_STANDARD_REQUIRED YES 
#                      CXX_EXTENSIONS NO)
#target_include_directories(uFilterPicker
#                           PRIVATE $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
#                           PRIVATE $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
#                           PRIVATE $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>)
#target_link_libraries(uFilterPicker
#                      PRIVATE uFilterPicker::libuFilterPicker
#                              spdlog::spdlog_header_only
#                              gRPC::grpc
#                              gRPC::grpc++
#                              gRPC::grpcpp_otel_plugin
#                              opentelemetry-cpp::metrics
#                              Boost::program_options
#                              TBB::tbb)

#add_executable(pickProxy src/grpc/pickProxy.cpp)
#set_target_properties(pickProxy PROPERTIES
#                      CXX_STANDARD 20
#                      CXX_STANDARD_REQUIRED YES 
#                      CXX_EXTENSIONS NO) 
#target_link_libraries(pickProxy
#                      PRIVATE uFilterPicker::libuFilterPicker
#                              spdlog::spdlog_header_only
#                              gRPC::grpc
#                              gRPC::grpc++
#                              gRPC::grpcpp_otel_plugin
#                              opentelemetry-cpp::metrics
#                              Boost::program_options
#                              TBB::tbb)
#target_include_directories(pickProxy
#                           PRIVATE $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
#                           PRIVATE $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>)

if (${BUILD_TRAINING})
   add_executable(gatherWaveforms
                  training/gatherWaveforms.cpp)   
   set_target_properties(gatherWaveforms PROPERTIES
                         CXX_STANDARD 23
                         CXX_STANDARD_REQUIRED YES
                         CXX_EXTENSIONS NO)
   target_link_libraries(gatherWaveforms
                         PRIVATE uFilterPicker::libuFilterPicker #IPP::ippcore IPP::ipps IPP::ippvm
                                 spdlog::spdlog_header_only
                                 mseed::mseed libpqxx::pqxx GeographicLib::GeographicLib
                                 ${HDF5_CXX_LIBRARIES} ${HDF5_C_LIBRARIES})
   target_include_directories(gatherWaveforms
                              PRIVATE $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
                              PRIVATE $<BUILD_INTERFACE:${HDF5_INCLUDE_DIRS}>)
endif()

if (PROJECT_IS_TOP_LEVEL AND ${BUILD_TESTS})
   add_executable(unitTests
                  testing/utilities.cpp
                  uDataPacketServiceAPI/v1/packet.proto)
   set_target_properties(unitTests PROPERTIES
                         CXX_STANDARD 23
                         CXX_STANDARD_REQUIRED YES 
                         CXX_EXTENSIONS NO) 
   target_link_libraries(unitTests
                         PUBLIC
                            TBB::tbb
                         PRIVATE 
                            uFilterPicker::libuFilterPicker
                            spdlog::spdlog_header_only
                            gRPC::grpc gRPC::grpc++
                            Threads::Threads
                            Catch2::Catch2 Catch2::Catch2WithMain)
   target_include_directories(unitTests
                              PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>)
   add_test(NAME unitTests
            COMMAND $<TARGET_FILE:unitTests>) 
   protobuf_generate(TARGET unitTests
                     LANGUAGE cpp 
                     )   
   protobuf_generate(TARGET unitTests
                     LANGUAGE grpc
                     PLUGIN "protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>"
                     PLUGIN_OPTIONS generate_mock_code=true
                     GENERATE_EXTENSIONS .grpc.pb.h .grpc.pb.cc
                    )   


#   add_executable(convertData
#                  testing/convertData.cpp)
#   set_target_properties(convertData PROPERTIES
#                         CXX_STANDARD 23
#                         CXX_STANDARD_REQUIRED YES
#                         CXX_EXTENSIONS NO)
#   target_link_libraries(convertData
#                         PRIVATE uFilterPicker::libuFilterPicker #IPP::ippcore IPP::ipps IPP::ippvm
#                                 spdlog::spdlog_header_only uSignal::libuSignal
#                                 mseed::mseed)
#   target_include_directories(convertData
#                              PRIVATE $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
#                              PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>)
##                             PRIVATE $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/lib>
##                             PRIVATE $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
##                             PUBLIC $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include/uWaveServer>
##                             PUBLIC $<INSTALL_INTERFACE:include>)
endif()


##########################################################################################
#                                      Installation                                      #
##########################################################################################
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
configure_package_config_file(
    cmake/${PROJECT_NAME}Config.cmake.in "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}")
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
    VERSION "${version}"
    COMPATIBILITY AnyNewerVersion
)

install(TARGETS libuFilterPicker
        EXPORT ${PROJECT_NAME}Targets
        FILE_SET HEADERS
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILE_SET CXX_MODULES
        DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        COMPONENT libraries)
#install(TARGETS detect
#        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
#        COMPONENT applications)
install(EXPORT ${PROJECT_NAME}Targets
        NAMESPACE ${PROJECT_NAME}::
        FILE ${PROJECT_NAME}Targets.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME})
install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME})

##########################################################################################
#                                     CPACK Packaging                                    #
##########################################################################################
include(CPackComponent)
set(CPACK_PACKAGE_NAME ${PROJECT_NAME})
set(CPACK_PACKAGE_VENDOR "UUSS")
set(CPACK_PACKAGE_CONTACT "ben.baker@utah.edu")
set(CPACK_PACKAGE_LICENSE "MIT")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Implementation of a Lomax style filter picker for the UUSS Kubernetes environment.")
set(CPACK_PACKAGE_INSTALL_DIRECTORY ${CPACK_PACKAGE_NAME})
set(CPACK_VERBATIM_VARIABLES YES)
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
if (${BUILD_SHARED_LIBS})
   cpack_add_component(libraries
                       DISPLAY_NAME "uFilterPicker common libraries"
                       DEPENDS ${GRPC_LIBRARY} ${GRPC_GRPC++_LIBRARY})
else()
   cpack_add_component(libraries
                       DISPLAY_NAME "uFilterPicker common libraries")
endif()
cpack_add_component(headers
                    DISPLAY_NAME "uFilterPicker library header files"
                    DEPENDS libraries)
cpack_add_component(applications
                    DISPLAY_NAME "uFilterPicker applications"
                    DEPENDS libraries)
set(CPACK_GENERATOR ZIP TGZ)
if (WIN32)
   list(APPEND CPACK_GENERATOR WIX)
elseif (APPLE)
   list(APPEND CPACK_GENERATOR productbuild)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
   find_program(RPMBUILD_PROGRAM rpmbuild)
   if (RPMBUILD_PROGRAM)
      list(APPEND CPACK_GENERATOR RPM)
   endif()
   if (${BUILD_FOR_DEBIAN})
      find_program(DPKG_PROGRAM dpkg REQUIRED)
   else()
      find_program(DPKG_PROGRAM dpkg)
   endif()
   if (DPKG_PROGRAM)
      list(APPEND CPACK_GENERATOR DEB)
      set(CPACK_DEBIAN_PACKAGE_DEPENDS "${CPACK_DEBIAN_PACKAGE_DEPENDS}")
      set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
   endif()
else()
   message("Unknown operating system")
endif()
set(CPACK_SOURCE_IGNORE_FILES
  /\\.git/
  \\.swp
  \\.orig
  /CMakeLists\\.txt\\.user
  /private/
)
include(CPack) # Put this last!
