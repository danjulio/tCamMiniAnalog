# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/Users/danjulio/esp/esp-idf/components/bootloader/subproject"
  "/Users/danjulio/work/thermal_camera/tCam/github/tCamMiniAnalog/firmware/build/bootloader"
  "/Users/danjulio/work/thermal_camera/tCam/github/tCamMiniAnalog/firmware/build/bootloader-prefix"
  "/Users/danjulio/work/thermal_camera/tCam/github/tCamMiniAnalog/firmware/build/bootloader-prefix/tmp"
  "/Users/danjulio/work/thermal_camera/tCam/github/tCamMiniAnalog/firmware/build/bootloader-prefix/src/bootloader-stamp"
  "/Users/danjulio/work/thermal_camera/tCam/github/tCamMiniAnalog/firmware/build/bootloader-prefix/src"
  "/Users/danjulio/work/thermal_camera/tCam/github/tCamMiniAnalog/firmware/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/Users/danjulio/work/thermal_camera/tCam/github/tCamMiniAnalog/firmware/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
