# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "C:/Users/svanleeuwen/esp/v5.2/esp-idf/components/bootloader/subproject"
  "C:/_projects/ESP32-C6/https_request/https_request/build/bootloader"
  "C:/_projects/ESP32-C6/https_request/https_request/build/bootloader-prefix"
  "C:/_projects/ESP32-C6/https_request/https_request/build/bootloader-prefix/tmp"
  "C:/_projects/ESP32-C6/https_request/https_request/build/bootloader-prefix/src/bootloader-stamp"
  "C:/_projects/ESP32-C6/https_request/https_request/build/bootloader-prefix/src"
  "C:/_projects/ESP32-C6/https_request/https_request/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "C:/_projects/ESP32-C6/https_request/https_request/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "C:/_projects/ESP32-C6/https_request/https_request/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
