# Copyright (c) 2024 Internet Mastering & Company
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at:
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Check if Go is available
find_program(GO_EXECUTABLE go)
if(NOT GO_EXECUTABLE)
  message(WARNING "Go not found - outline_server plugin disabled")
  return()
endif()

# Get Go version
execute_process(
  COMMAND ${GO_EXECUTABLE} version
  OUTPUT_VARIABLE GO_VERSION_OUTPUT
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_QUIET
)

message(STATUS "Found Go: ${GO_EXECUTABLE} (${GO_VERSION_OUTPUT})")

# Build the outline-ss-server binary
set(OUTLINE_SERVER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/server)
set(OUTLINE_SERVER_BINARY ${CMAKE_CURRENT_BINARY_DIR}/outline-ss-server)

# Check if server directory exists and has go.mod
if(NOT EXISTS ${OUTLINE_SERVER_DIR}/go.mod)
  message(WARNING "outline-ss-server submodule not initialized - run 'git submodule update --init --recursive'")
  return()
endif()

add_custom_command(
  OUTPUT ${OUTLINE_SERVER_BINARY}
  COMMAND ${CMAKE_COMMAND} -E env CGO_ENABLED=0
          ${GO_EXECUTABLE} build -o ${OUTLINE_SERVER_BINARY}
          ./cmd/outline-ss-server
  WORKING_DIRECTORY ${OUTLINE_SERVER_DIR}
  COMMENT "Building outline-ss-server with Go"
  VERBATIM
)

add_custom_target(outline_server_binary ALL
  DEPENDS ${OUTLINE_SERVER_BINARY}
)

# Add VPP plugin for outline server management
add_vpp_plugin(outline_server
  SOURCES
  outline_server.c
  outline_server.h
  outline_server_cli.c
  outline_server_api.c

  API_FILES
  outline_server.api

  INSTALL_HEADERS
  outline_server.h
)

# Ensure the Go binary is built before the plugin
add_dependencies(outline_server_plugin outline_server_binary)

# Install the outline-ss-server binary
install(
  PROGRAMS ${OUTLINE_SERVER_BINARY}
  DESTINATION ${VPP_LIBRARY_DIR}/vpp_plugins
)
