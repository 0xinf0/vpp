# Copyright (c) 2025 Internet Mastering & Company, Inc.
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

##############################################################################
# Sing-box Integration Plugin
##############################################################################

# Include directories for VPP headers
include_directories(${CMAKE_SOURCE_DIR})

# Include generated API headers
include_directories(${CMAKE_BINARY_DIR})

add_vpp_plugin(singbox
  SOURCES
  singbox.c
  node.c
  socks5.c
  socks5_server.c
  session.c

  MULTIARCH_SOURCES
  node.c

  API_FILES
  singbox.api

  INSTALL_HEADERS
  singbox.h

  COMPONENT vpp-plugin-singbox
)
