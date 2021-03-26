# set source files
set (server-src-files
     provider.c)

set (client-src-files
     client.c)

set (admin-src-files
     admin.c)

set (dummy-src-files
     dummy/dummy-backend.c)

set (bedrock-module-src-files
     bedrock-module.c)

# load package helper for generating cmake CONFIG packages
include (CMakePackageConfigHelpers)

# where to install files for "find_package"
set (symbiomon-pkg "share/cmake/symbiomon")

# set version fo shared library
set (symbiomon-vers "${SYMBIOMON_VERSION_MAJOR}.${SYMBIOMON_VERSION_MINOR}")

if(${ENABLE_AGGREGATOR})
  add_definitions("-DUSE_AGGREGATOR")
endif(${ENABLE_AGGREGATOR})

# server library
add_library (symbiomon-server ${server-src-files} ${dummy-src-files})
target_link_libraries (symbiomon-server
    PkgConfig::MARGO
    PkgConfig::ABTIO
    PkgConfig::UUID)
#    PkgConfig::JSONC)
target_include_directories (symbiomon-server PUBLIC $<INSTALL_INTERFACE:include>)
target_include_directories (symbiomon-server BEFORE PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>)
target_include_directories (symbiomon-server BEFORE PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>)
set_target_properties (symbiomon-server
    PROPERTIES VERSION ${SYMBIOMON_VERSION}
    SOVERSION ${SYMBIOMON_VERSION_MAJOR})

# client library
add_library (symbiomon-client ${client-src-files})
target_link_libraries (symbiomon-client PkgConfig::MARGO PkgConfig::UUID)
target_include_directories (symbiomon-client PUBLIC $<INSTALL_INTERFACE:include>)
target_include_directories (symbiomon-client BEFORE PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>)
target_include_directories (symbiomon-client BEFORE PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>)
set_target_properties (symbiomon-client
    PROPERTIES VERSION ${SYMBIOMON_VERSION}
    SOVERSION ${SYMBIOMON_VERSION_MAJOR})

if(${ENABLE_AGGREGATOR})
  target_include_directories (symbiomon-server PUBLIC ${SDSKV_INCLUDE_DIR})
  target_link_libraries (symbiomon-server PkgConfig::SDSKV)
endif(${ENABLE_AGGREGATOR})

# admin library
add_library (symbiomon-admin ${admin-src-files})
target_link_libraries (symbiomon-admin PkgConfig::MARGO PkgConfig::UUID)
target_include_directories (symbiomon-admin PUBLIC $<INSTALL_INTERFACE:include>)
target_include_directories (symbiomon-admin BEFORE PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>)
target_include_directories (symbiomon-admin BEFORE PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>)
set_target_properties (symbiomon-admin
    PROPERTIES VERSION ${SYMBIOMON_VERSION}
    SOVERSION ${SYMBIOMON_VERSION_MAJOR})

if (${ENABLE_BEDROCK})
# bedrock-module library
add_library (symbiomon-bedrock-module ${bedrock-module-src-files})
target_link_libraries (symbiomon-bedrock-module symbiomon-server symbiomon-admin symbiomon-client)
target_include_directories (symbiomon-bedrock-module PUBLIC $<INSTALL_INTERFACE:include>)
target_include_directories (symbiomon-bedrock-module BEFORE PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>)
target_include_directories (symbiomon-bedrock-module BEFORE PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>)
set_target_properties (symbiomon-bedrock-module
    PROPERTIES VERSION ${SYMBIOMON_VERSION}
    SOVERSION ${SYMBIOMON_VERSION_MAJOR})
endif ()

# installation stuff (packaging and install commands)
write_basic_package_version_file(
    "symbiomon-config-version.cmake"
    VERSION ${SYMBIOMON_VERSION}
    COMPATIBILITY AnyNewerVersion)

# generate our config file for find_package()
set(INCLUDE_INSTALL_DIR include/)
set(LIB_INSTALL_DIR lib/)
configure_package_config_file(symbiomon-config.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/symbiomon-config.cmake
  INSTALL_DESTINATION ${LIB_INSTALL_DIR}/symbiomon/cmake
  PATH_VARS INCLUDE_INSTALL_DIR)

# some bits for the pkg-config file
set (DEST_DIR "${CMAKE_INSTALL_PREFIX}")
set (SERVER_PRIVATE_LIBS "-lsymbiomon-server")
set (CLIENT_PRIVATE_LIBS "-lsymbiomon-client")
set (ADMIN_PRIVATE_LIBS  "-lsymbiomon-admin")
configure_file ("symbiomon-server.pc.in" "symbiomon-server.pc" @ONLY)
configure_file ("symbiomon-client.pc.in" "symbiomon-client.pc" @ONLY)
configure_file ("symbiomon-admin.pc.in"  "symbiomon-admin.pc"  @ONLY)

# configure config.h
configure_file ("config.h.in" "config.h" @ONLY)

# "make install" rules
install (TARGETS symbiomon-server symbiomon-client symbiomon-admin
         EXPORT symbiomon-targets
         ARCHIVE DESTINATION lib
         LIBRARY DESTINATION lib)
if (${ENABLE_BEDROCK})
    install (TARGETS symbiomon-bedrock-module
             ARCHIVE DESTINATION lib
             LIBRARY DESTINATION lib)
endif ()
install (EXPORT symbiomon-targets
         DESTINATION ${symbiomon-pkg}
         FILE "symbiomon-targets.cmake")
install (FILES "${CMAKE_CURRENT_BINARY_DIR}/symbiomon-config.cmake"
               "${CMAKE_CURRENT_BINARY_DIR}/symbiomon-config-version.cmake"
         DESTINATION ${symbiomon-pkg} )
install (DIRECTORY ../include/symbiomon
         DESTINATION include
         FILES_MATCHING PATTERN "*.h")
install (FILES "${CMAKE_CURRENT_BINARY_DIR}/symbiomon-server.pc"
               "${CMAKE_CURRENT_BINARY_DIR}/symbiomon-client.pc"
               "${CMAKE_CURRENT_BINARY_DIR}/symbiomon-admin.pc"
         DESTINATION "lib/pkgconfig/")
