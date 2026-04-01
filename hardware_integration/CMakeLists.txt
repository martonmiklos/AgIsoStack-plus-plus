cmake_minimum_required(VERSION 3.16)

# Set source and include directories
set(HARDWARE_INTEGRATION_SRC_DIR "src")
set(HARDWARE_INTEGRATION_INCLUDE_DIR "include/isobus/hardware_integration")

if(NOT CAN_DRIVER)
  if(WIN32)
    set(CAN_DRIVER "WindowsPCANBasic")
  elseif(APPLE)
    set(CAN_DRIVER "MacCANPCAN")
  else()
    set(CAN_DRIVER "SocketCAN")
  endif()

  message(
    AUTHOR_WARNING
      "No CAN driver specified, choosing ${CAN_DRIVER} by default. Set the CAN_DRIVER variable with -DCAN_DRIVER=<driver> to specify another driver. or -DCAN_DRIVER=\"<driver1>;<driver2>\" to specify multiple drivers."
  )
endif()

if(BUILD_TESTING AND NOT "VirtualCAN" IN_LIST CAN_DRIVER)
  message(STATUS "Including VirtualCAN driver for testing.")
  list(APPEND CAN_DRIVER "VirtualCAN")
endif()

# Set the source files
set(HARDWARE_INTEGRATION_SRC "can_hardware_interface.cpp"
                             "vector_asc_logger.cpp")

# Set the include files
set(HARDWARE_INTEGRATION_INCLUDE
    "can_hardware_interface.hpp" "can_hardware_plugin.hpp"
    "vector_asc_logger.hpp" "available_can_drivers.hpp")

# Add the source/include files based on the CAN driver chosen
if("SocketCAN" IN_LIST CAN_DRIVER)
  list(APPEND HARDWARE_INTEGRATION_SRC "socket_can_interface.cpp")
  list(APPEND HARDWARE_INTEGRATION_INCLUDE "socket_can_interface.hpp")
endif()
if("WindowsPCANBasic" IN_LIST CAN_DRIVER)
  list(APPEND HARDWARE_INTEGRATION_SRC "pcan_basic_windows_plugin.cpp")
  list(APPEND HARDWARE_INTEGRATION_INCLUDE "pcan_basic_windows_plugin.hpp")
endif()
if("VirtualCAN" IN_LIST CAN_DRIVER)
  list(APPEND HARDWARE_INTEGRATION_SRC "virtual_can_plugin.cpp")
  list(APPEND HARDWARE_INTEGRATION_INCLUDE "virtual_can_plugin.hpp")
endif()
if("TWAI" IN_LIST CAN_DRIVER)
  list(APPEND HARDWARE_INTEGRATION_SRC "twai_plugin.cpp")
  list(APPEND HARDWARE_INTEGRATION_INCLUDE "twai_plugin.hpp")
endif()
if("MCP2515" IN_LIST CAN_DRIVER)
  list(APPEND HARDWARE_INTEGRATION_SRC "mcp2515_can_interface.cpp")
  list(APPEND HARDWARE_INTEGRATION_INCLUDE "mcp2515_can_interface.hpp")
  if(ESP_PLATFORM)
    list(APPEND HARDWARE_INTEGRATION_SRC "spi_interface_esp.cpp")
    list(APPEND HARDWARE_INTEGRATION_INCLUDE "spi_interface_esp.hpp")
  endif()
endif()
if("TouCAN" IN_LIST CAN_DRIVER)
  list(APPEND HARDWARE_INTEGRATION_SRC "toucan_vscp_canal.cpp")
  list(APPEND HARDWARE_INTEGRATION_INCLUDE "toucan_vscp_canal.hpp")
  list(APPEND HARDWARE_INTEGRATION_INCLUDE "canal.h")
  list(APPEND HARDWARE_INTEGRATION_INCLUDE "canal_a.h")
endif()
if("MacCANPCAN" IN_LIST CAN_DRIVER)
  message(
    AUTHOR_WARNING
      "Use of the MacCAN driver is governed by their eula. Visit their webside mac-can.com to download the library and follow the included instructions."
  )
  message(
    AUTHOR_WARNING
      "If you get a popup from your OS saying it can't run the driver, you may need to add a security exception to your Mac. This is not something we can control."
  )
  list(APPEND HARDWARE_INTEGRATION_INCLUDE "PCBUSB.h")
  list(APPEND HARDWARE_INTEGRATION_INCLUDE "mac_can_pcan_plugin.hpp")
  list(APPEND HARDWARE_INTEGRATION_SRC "mac_can_pcan_plugin.cpp")
endif()
if("WindowsInnoMakerUSB2CAN" IN_LIST CAN_DRIVER)
  message(
    AUTHOR_WARNING
      "
      This is not legal advice. The InnoMaker USB2CAN driver uses libusb, which is an LGPL-2.1 library. Be sure you understand the implications of this before proceeding."
  )
  message(
    AUTHOR_WARNING
      "
      The WindowsInnoMakerUSB2CAN driver requires the InnoMakerUsb2CANLibxx.dll to be in the same directory as the executable. A copy of this file is included in the repository,
      under the /hardware_integration/lib/Windows directory. It is recommended to use this copy, as it is known to work with the WindowsInnoMakerUSB2CAN driver provided by the stack.
      ")
  list(APPEND HARDWARE_INTEGRATION_SRC "innomaker_usb2can_windows_plugin.cpp")
  list(APPEND HARDWARE_INTEGRATION_INCLUDE
       "innomaker_usb2can_windows_plugin.hpp")
endif()
if("SYS_TEC" IN_LIST CAN_DRIVER)
  list(APPEND HARDWARE_INTEGRATION_SRC "sys_tec_windows_plugin.cpp")
  list(APPEND HARDWARE_INTEGRATION_INCLUDE "sys_tec_windows_plugin.hpp")
  list(APPEND HARDWARE_INTEGRATION_INCLUDE "Usbcan32.h")
  list(APPEND HARDWARE_INTEGRATION_INCLUDE "UsbCanLs.h")
  list(APPEND HARDWARE_INTEGRATION_INCLUDE "UsbCanUp.h")
endif()
if("NTCAN" IN_LIST CAN_DRIVER)
  list(APPEND HARDWARE_INTEGRATION_SRC "ntcan_plugin.cpp")
  list(APPEND HARDWARE_INTEGRATION_INCLUDE "ntcan_plugin.hpp")
endif()
if("SILKit" IN_LIST CAN_DRIVER)
  list(APPEND HARDWARE_INTEGRATION_SRC "sil_kit_plugin.cpp")
  list(APPEND HARDWARE_INTEGRATION_INCLUDE "sil_kit_plugin.hpp")
endif()
if("WindowsCandleDLL" IN_LIST CAN_DRIVER)
  list(APPEND HARDWARE_INTEGRATION_SRC "candle_windows_plugin.cpp")
  list(APPEND HARDWARE_INTEGRATION_INCLUDE "candle_windows_plugin.hpp")
endif()

# Prepend the source directory path to all the source files
prepend(HARDWARE_INTEGRATION_SRC ${HARDWARE_INTEGRATION_SRC_DIR}
        ${HARDWARE_INTEGRATION_SRC})

# Prepend the include directory path to all the include files
prepend(HARDWARE_INTEGRATION_INCLUDE ${HARDWARE_INTEGRATION_INCLUDE_DIR}
        ${HARDWARE_INTEGRATION_INCLUDE})

# Create the library from the source and include files
add_library(HardwareIntegration ${HARDWARE_INTEGRATION_SRC}
                                ${HARDWARE_INTEGRATION_INCLUDE})
add_library(${PROJECT_NAME}::HardwareIntegration ALIAS HardwareIntegration)

target_compile_features(HardwareIntegration PUBLIC cxx_std_11)
set_target_properties(HardwareIntegration PROPERTIES CXX_EXTENSIONS OFF)
target_link_libraries(HardwareIntegration PRIVATE ${PROJECT_NAME}::Utility
                                                  ${PROJECT_NAME}::Isobus)
if(ESP_PLATFORM)
  target_link_libraries(HardwareIntegration PRIVATE idf::driver)
endif()

if("WindowsPCANBasic" IN_LIST CAN_DRIVER)
  if(MSVC)
    # See https://gitlab.kitware.com/cmake/cmake/-/issues/15170
    set(CMAKE_SYSTEM_PROCESSOR ${MSVC_CXX_ARCHITECTURE_ID})
  endif()

  message(STATUS "Target Arch: ${CMAKE_SYSTEM_PROCESSOR}")
  if(CMAKE_SYSTEM_PROCESSOR STREQUAL "AMD64" OR CMAKE_SYSTEM_PROCESSOR STREQUAL
                                                "x64")
    message(STATUS "Detected AMD64, linking to PCAN x64 Library")
    target_link_libraries(
      HardwareIntegration
      PRIVATE ${CMAKE_CURRENT_LIST_DIR}/lib/Windows/PCANBasic_x64.lib)
    add_custom_command(
      OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/lib/Windows/PCANBasic_x64.lib
      COMMENT "Copying PCANBasic_x64.lib to build directory"
      COMMAND
        ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_LIST_DIR}/lib/Windows/PCANBasic_x64.lib
        ${CMAKE_CURRENT_BINARY_DIR}/PCANBasic_x64.lib)
  elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "ARM64")
    message(STATUS "Detected ARM64, linking to PCAN ARM64 Library")
    target_link_libraries(
      HardwareIntegration
      PRIVATE ${CMAKE_CURRENT_LIST_DIR}/lib/Windows/PCANBasic_ARM64.lib)
    add_custom_command(
      OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/lib/Windows/PCANBasic_ARM64.lib
      COMMENT "Copying PCANBasic_ARM64.lib to build directory"
      COMMAND
        ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_LIST_DIR}/lib/Windows/PCANBasic_ARM64.lib
        ${CMAKE_CURRENT_BINARY_DIR}/PCANBasic_ARM64.lib)
  elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64" OR CMAKE_SYSTEM_PROCESSOR
                                                     STREQUAL "X86")
    message(STATUS "Detected x86, linking to PCAN x86 Library")
    target_link_libraries(
      HardwareIntegration
      PRIVATE ${CMAKE_CURRENT_LIST_DIR}/lib/Windows/PCANBasic_x86.lib)
    add_custom_command(
      OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/lib/Windows/PCANBasic_x86.lib
      COMMENT "Copying PCANBasic_x86.lib to build directory"
      COMMAND
        ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_LIST_DIR}/lib/Windows/PCANBasic_x86.lib
        ${CMAKE_CURRENT_BINARY_DIR}/PCANBasic_x86.lib)
  else()
    message(
      FATAL_ERROR
        "Windows PCAN Selected but no supported processor arch was detected. Only x64, x86, and ARM64 are supported by PEAK's drivers."
    )
  endif()
endif()

if("WindowsInnoMakerUSB2CAN" IN_LIST CAN_DRIVER)
  if(MSVC)
    # See https://gitlab.kitware.com/cmake/cmake/-/issues/15170
    set(CMAKE_SYSTEM_PROCESSOR ${MSVC_CXX_ARCHITECTURE_ID})
  endif()

  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU" AND MINGW)
    message(
      FATAL_ERROR
        "The InnoMaker driver is compatible only with MSVC, and won't work with mingw"
    )
  else()
    message(STATUS "Target Arch: ${CMAKE_SYSTEM_PROCESSOR}")
    if(CMAKE_SYSTEM_PROCESSOR STREQUAL "AMD64" OR CMAKE_SYSTEM_PROCESSOR
                                                  STREQUAL "x64")
      message(STATUS "Detected AMD64, linking to INNOMAKER USB2CAN x64 Library")
      target_link_libraries(
        HardwareIntegration
        PRIVATE ${CMAKE_CURRENT_LIST_DIR}/lib/Windows/InnoMakerUsb2CanLib64.lib)
    elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64" OR CMAKE_SYSTEM_PROCESSOR
                                                       STREQUAL "X86")
      message(STATUS "Detected x86, linking to INNOMAKER USB2CAN x86 Library")
      target_link_libraries(
        HardwareIntegration
        PRIVATE ${CMAKE_CURRENT_LIST_DIR}/lib/Windows/InnoMakerUsb2CanLib32.lib)
    else()
      message(
        FATAL_ERROR
          "Windows InnoMaker USB2CAN Selected but no supported processor arch was detected. Only x64 and x86 are supported."
      )
    endif()
  endif()
endif()

if("MacCANPCAN" IN_LIST CAN_DRIVER)
  target_link_libraries(
    HardwareIntegration
    PRIVATE ${CMAKE_CURRENT_LIST_DIR}/lib/Mac/libPCBUSB.0.12.dylib)
endif()

if("TouCAN" IN_LIST CAN_DRIVER)
  if(MSVC)
    # See https://gitlab.kitware.com/cmake/cmake/-/issues/15170
    set(CMAKE_SYSTEM_PROCESSOR ${MSVC_CXX_ARCHITECTURE_ID})
  endif()
  if(WIN32)
    if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64" OR CMAKE_SYSTEM_PROCESSOR
                                                   STREQUAL "X86")
      message(STATUS "Detected x86, linking to CANAL 1.0.5 x86 library")
      message(
        STATUS
          "To lean more about this driver, visit https://github.com/rusoku/CANAL-DLL or https://www.rusoku.com/"
      )
      message(
        AUTHOR_WARNING
          "
      The TouCAN driver requires canal.dll to be in the same directory as the executable.
      A copy of this file is included in the repository under the /hardware_integration/lib/Windows directory as canal32.dll.
      It is recommended to use this copy, as it is known to work with the TouCAN driver provided by the stack.
      ")
      target_link_libraries(
        HardwareIntegration
        PRIVATE ${CMAKE_CURRENT_LIST_DIR}/lib/Windows/canal32.lib)
    elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "AMD64" OR CMAKE_SYSTEM_PROCESSOR
                                                      STREQUAL "x64")
      message(STATUS "Detected x64, linking to CANAL 1.0.5 x64 library")
      message(
        STATUS
          "To lean more about this driver, visit https://github.com/rusoku/CANAL-DLL or https://www.rusoku.com/"
      )
      message(
        AUTHOR_WARNING
          "
      The TouCAN driver requires canal.dll to be in the same directory as the executable. A copy of this file is included in the repository,
      under the /hardware_integration/lib/Windows directory as canal64.dll.
      It is recommended to use this copy and rename it to canal.dll, as it is known to work with the TouCAN driver provided by the stack.
      ")
      target_link_libraries(
        HardwareIntegration
        PRIVATE ${CMAKE_CURRENT_LIST_DIR}/lib/Windows/canal64.lib)
    else()
      message(
        FATAL_ERROR
          "TouCAN Selected but no supported processor arch was detected. Only x64 and x86 are supported."
      )
    endif()
  else()
    message(
      FATAL_ERROR
        "TouCAN Selected but no supported OS was detected. Only Windows is supported currently."
    )
  endif()
endif()
if("SYS_TEC" IN_LIST CAN_DRIVER)
  if(MSVC)
    # See https://gitlab.kitware.com/cmake/cmake/-/issues/15170
    set(CMAKE_SYSTEM_PROCESSOR ${MSVC_CXX_ARCHITECTURE_ID})
  endif()
  if(WIN32)
    if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64" OR CMAKE_SYSTEM_PROCESSOR
                                                   STREQUAL "X86")
      message(STATUS "Detected x86, linking to SYS TEC x86 library")
      message(
        AUTHOR_WARNING
          "Make sure you've installed the driver for your SYS TEC device before using this plugin.
          https://www.systec-electronic.com/ ")
      target_link_libraries(
        HardwareIntegration
        PRIVATE ${CMAKE_CURRENT_LIST_DIR}/lib/Windows/USBCAN32.lib)
    elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "AMD64" OR CMAKE_SYSTEM_PROCESSOR
                                                      STREQUAL "x64")
      message(STATUS "Detected x64, linking to SYS TEC x64 library")
      message(
        AUTHOR_WARNING
          "Make sure you've installed the driver for your SYS TEC device before using this plugin.
          https://www.systec-electronic.com/ ")
      target_link_libraries(
        HardwareIntegration
        PRIVATE ${CMAKE_CURRENT_LIST_DIR}/lib/Windows/USBCAN64.lib)
    else()
      message(
        FATAL_ERROR
          "SYS TEC Selected but no supported processor arch was detected. Only x64 and x86 are supported."
      )
    endif()
  else()
    message(
      FATAL_ERROR
        "SYS TEC Selected but no supported OS was detected. Only Windows is supported currently."
    )
  endif()
endif()
if("WindowsCandleDLL" IN_LIST CAN_DRIVER)
  if(WIN32)
    message(
      AUTHOR_WARNING
        "
      The WindowsCandleDLL driver requires candle.dll to be available next to your executable,
      or in a location discoverable by LoadLibrary. Source: https://github.com/Andrei-Errapart/candle_dll
      "
    )

    # Download the latest candle_dll release asset automatically.
    # We intentionally parse the GitHub API JSON using regex to keep compatibility
    # with CMake 3.16 (without string(JSON ...)).
    set(CANDLE_DLL_RELEASE_API
        "https://api.github.com/repos/Andrei-Errapart/candle_dll/releases/latest")
    set(CANDLE_DLL_DOWNLOAD_DIR
        "${CMAKE_CURRENT_BINARY_DIR}/candle_dll_release")
    file(MAKE_DIRECTORY "${CANDLE_DLL_DOWNLOAD_DIR}")

    set(CANDLE_DLL_RELEASE_JSON
        "${CANDLE_DLL_DOWNLOAD_DIR}/candle_release_latest.json")
    file(
      DOWNLOAD "${CANDLE_DLL_RELEASE_API}" "${CANDLE_DLL_RELEASE_JSON}"
      STATUS CANDLE_DLL_RELEASE_STATUS TLS_VERIFY ON
      HTTPHEADER "User-Agent: AgIsoStack-plus-plus-CMake")
    list(GET CANDLE_DLL_RELEASE_STATUS 0 CANDLE_DLL_RELEASE_STATUS_CODE)
    if(NOT CANDLE_DLL_RELEASE_STATUS_CODE EQUAL 0)
      message(
        WARNING
          "Failed to query candle_dll latest release metadata from GitHub API. Status: ${CANDLE_DLL_RELEASE_STATUS}.")
    else()
      file(READ "${CANDLE_DLL_RELEASE_JSON}" CANDLE_DLL_RELEASE_JSON_TEXT)

      # Extract browser_download_url entries first (best source for direct asset download).
      string(
        REGEX MATCHALL
              "\"browser_download_url\"[ \t\r\n]*:[ \t\r\n]*\"[^\"]+\""
              CANDLE_DLL_BROWSER_URL_FIELDS "${CANDLE_DLL_RELEASE_JSON_TEXT}")

      set(CANDLE_DLL_RELEASE_URLS "")
      foreach(CANDLE_DLL_BROWSER_URL_FIELD ${CANDLE_DLL_BROWSER_URL_FIELDS})
        string(
          REGEX REPLACE
                "^.*\"browser_download_url\"[ \t\r\n]*:[ \t\r\n]*\"([^\"]+)\".*$"
                "\\1" CANDLE_DLL_BROWSER_URL
                "${CANDLE_DLL_BROWSER_URL_FIELD}")
        string(REPLACE "\\/" "/" CANDLE_DLL_BROWSER_URL
                       "${CANDLE_DLL_BROWSER_URL}")
        list(APPEND CANDLE_DLL_RELEASE_URLS "${CANDLE_DLL_BROWSER_URL}")
      endforeach()

      # Backward-compatible fallback in case browser_download_url is missing.
      if(NOT CANDLE_DLL_RELEASE_URLS)
        string(
          REGEX MATCHALL
                "https://[^\"[:space:]]*/releases/download/[^\"[:space:]]*"
                CANDLE_DLL_RELEASE_URLS "${CANDLE_DLL_RELEASE_JSON_TEXT}")
      endif()

      set(CANDLE_DLL_SELECTED_ASSET_URL "")
      if(MSVC)
        # See https://gitlab.kitware.com/cmake/cmake/-/issues/15170
        set(CMAKE_SYSTEM_PROCESSOR ${MSVC_CXX_ARCHITECTURE_ID})
      endif()

      foreach(CANDLE_DLL_ASSET_URL ${CANDLE_DLL_RELEASE_URLS})
        if(CMAKE_SYSTEM_PROCESSOR STREQUAL "AMD64" OR CMAKE_SYSTEM_PROCESSOR
                                                  STREQUAL "x64")
          if(CANDLE_DLL_ASSET_URL MATCHES
             "([Xx]64|[Aa][Mm][Dd]64).*(\\.zip|\\.7z|\\.dll)$")
            set(CANDLE_DLL_SELECTED_ASSET_URL "${CANDLE_DLL_ASSET_URL}")
            break()
          endif()
        elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64"
               OR CMAKE_SYSTEM_PROCESSOR STREQUAL "X86")
          if(CANDLE_DLL_ASSET_URL MATCHES
             "([Xx]86|[Ii]386).*(\\.zip|\\.7z|\\.dll)$")
            set(CANDLE_DLL_SELECTED_ASSET_URL "${CANDLE_DLL_ASSET_URL}")
            break()
          endif()
        endif()
      endforeach()

      if(CANDLE_DLL_SELECTED_ASSET_URL STREQUAL "")
        list(LENGTH CANDLE_DLL_RELEASE_URLS CANDLE_DLL_RELEASE_URL_COUNT)
        if(CANDLE_DLL_RELEASE_URL_COUNT GREATER 0)
          list(GET CANDLE_DLL_RELEASE_URLS 0 CANDLE_DLL_SELECTED_ASSET_URL)
        endif()
      endif()

      if(NOT CANDLE_DLL_SELECTED_ASSET_URL STREQUAL "")
        get_filename_component(CANDLE_DLL_ASSET_NAME
                               "${CANDLE_DLL_SELECTED_ASSET_URL}" NAME)
        set(CANDLE_DLL_ASSET_PATH
            "${CANDLE_DLL_DOWNLOAD_DIR}/${CANDLE_DLL_ASSET_NAME}")
        file(
          DOWNLOAD "${CANDLE_DLL_SELECTED_ASSET_URL}"
          "${CANDLE_DLL_ASSET_PATH}" STATUS CANDLE_DLL_ASSET_DOWNLOAD_STATUS
          TLS_VERIFY ON HTTPHEADER
          "User-Agent: AgIsoStack-plus-plus-CMake")
        list(GET CANDLE_DLL_ASSET_DOWNLOAD_STATUS 0
             CANDLE_DLL_ASSET_DOWNLOAD_STATUS_CODE)
        if(NOT CANDLE_DLL_ASSET_DOWNLOAD_STATUS_CODE EQUAL 0)
          message(
            WARNING
              "Failed to download candle_dll release asset: ${CANDLE_DLL_SELECTED_ASSET_URL}. Status: ${CANDLE_DLL_ASSET_DOWNLOAD_STATUS}")
        else()
          if(CANDLE_DLL_ASSET_PATH MATCHES "\\.zip$")
            file(ARCHIVE_EXTRACT INPUT "${CANDLE_DLL_ASSET_PATH}"
                 DESTINATION "${CANDLE_DLL_DOWNLOAD_DIR}")
          endif()
        endif()
      else()
        message(
          WARNING
            "Could not identify any downloadable assets in the latest candle_dll release metadata.")
      endif()

      file(
        GLOB_RECURSE CANDLE_DLL_FILES
        "${CANDLE_DLL_DOWNLOAD_DIR}/candle*.dll"
        "${CANDLE_DLL_DOWNLOAD_DIR}/Candle*.dll")

      if(CANDLE_DLL_FILES)
        list(GET CANDLE_DLL_FILES 0 CANDLE_DLL_FILE)
        add_custom_command(
          TARGET HardwareIntegration
          POST_BUILD
          COMMAND
            ${CMAKE_COMMAND} -E copy_if_different "${CANDLE_DLL_FILE}"
            "$<TARGET_FILE_DIR:HardwareIntegration>/candle.dll"
          COMMENT
            "Copying downloaded candle.dll next to HardwareIntegration output")
      else()
        message(
          WARNING
            "No candle DLL binary found after download/extract. You may need to place candle.dll manually next to your executable.")
      endif()
    endif()
  else()
    message(
      FATAL_ERROR
        "WindowsCandleDLL Selected but no supported OS was detected. Only Windows is supported currently."
    )
  endif()
endif()

if("NTCAN" IN_LIST CAN_DRIVER)
  if(MSVC)
    # See https://gitlab.kitware.com/cmake/cmake/-/issues/15170
    set(CMAKE_SYSTEM_PROCESSOR ${MSVC_CXX_ARCHITECTURE_ID})
  endif()
  if(WIN32)
    if(DEFINED ENV{CanSdkDir})
      if(CMAKE_SYSTEM_PROCESSOR STREQUAL "AMD64" OR CMAKE_SYSTEM_PROCESSOR
                                                    STREQUAL "x64")
        message(STATUS "Detected x64, linking to NTCAN x64 library")
        target_link_libraries(HardwareIntegration
                              PRIVATE $ENV{CanSdkDir}/lib/VC/amd64/ntcan.lib)
        target_include_directories(HardwareIntegration
                                   PUBLIC $ENV{CanSdkDir}/include)
      elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64" OR CMAKE_SYSTEM_PROCESSOR
                                                         STREQUAL "X86")
        message(STATUS "Detected x86, linking to NTCAN x86 library")
        target_link_libraries(HardwareIntegration
                              PRIVATE $ENV{CanSdkDir}/lib/VC/i386/ntcan.lib)
        target_include_directories(HardwareIntegration
                                   PUBLIC $ENV{CanSdkDir}/include)
      else()
        message(
          FATAL_ERROR
            "NTCAN Selected but no supported processor arch was detected. Only x64 and x86 are supported."
        )
      endif()
    else()
      message(
        FATAL_ERROR
          "NTCAN Selected but no NTCAN SDK was found. Set the 'CanSdkDir' environment variable to the path of the NTCAN SDK."
      )
    endif()
  else()
    message(
      FATAL_ERROR
        "NTCAN Selected but no supported OS was detected. Only Windows is supported currently."
    )
  endif()
endif()

if("SILKit" IN_LIST CAN_DRIVER)
  find_package(SilKit REQUIRED)
  target_link_libraries(HardwareIntegration PUBLIC SilKit::SilKit)
endif()

# Mark the compiled CAN drivers available to other modules. In the form:
# `ISOBUS_<uppercase CAN_DRIVER>_AVAILABLE` as a preprocessor definition.
foreach(available_driver ${CAN_DRIVER})
  string(TOUPPER ${available_driver} available_driver)
  string(PREPEND available_driver "ISOBUS_")
  string(APPEND available_driver "_AVAILABLE")
  target_compile_definitions(HardwareIntegration PUBLIC ${available_driver})
endforeach()

# Specify the include directory to be exported for other moduels to use. The
# PUBLIC keyword here allows other libraries or exectuables to link to this
# library and use its functionality.
target_include_directories(
  HardwareIntegration
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
         $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
if(CAN_STACK_DISABLE_THREADS)
  target_include_directories(
    HardwareIntegration PRIVATE BEFORE $<BUILD_INTERFACE:${NO_STD_THREADS_DIR}>)
endif()

install(
  TARGETS HardwareIntegration
  EXPORT isobusTargets
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  RUNTIME DESTINATION bin)

install(
  DIRECTORY include/
  DESTINATION include
  FILES_MATCHING
  PATTERN "*.hpp")
