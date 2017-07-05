## ======================================================================== ##
## Copyright 2009-2017 Intel Corporation                                    ##
##                                                                          ##
## Licensed under the Apache License, Version 2.0 (the "License");          ##
## you may not use this file except in compliance with the License.         ##
## You may obtain a copy of the License at                                  ##
##                                                                          ##
##     http://www.apache.org/licenses/LICENSE-2.0                           ##
##                                                                          ##
## Unless required by applicable law or agreed to in writing, software      ##
## distributed under the License is distributed on an "AS IS" BASIS,        ##
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. ##
## See the License for the specific language governing permissions and      ##
## limitations under the License.                                           ##
## ======================================================================== ##

IF (WIN32)

  SET(FLAGS_SSE2  "/QxSSE2")
  SET(FLAGS_SSE42 "/QxSSE4.2")
  SET(FLAGS_AVX   "/arch:AVX")
  SET(FLAGS_AVX2  "/QxCORE-AVX2")
  SET(FLAGS_AVX512KNL "/QxMIC-AVX512")
  SET(FLAGS_AVX512SKX "/QxCORE-AVX512")

  SET(COMMON_CXX_FLAGS "")
  SET(COMMON_CXX_FLAGS "${COMMON_CXX_FLAGS} /EHsc")        # catch C++ exceptions only and extern "C" functions never throw a C++ exception
  SET(COMMON_CXX_FLAGS "${COMMON_CXX_FLAGS} /MP")          # compile source files in parallel
  SET(COMMON_CXX_FLAGS "${COMMON_CXX_FLAGS} /GR")          # enable runtime type information (on by default)
  SET(COMMON_CXX_FLAGS "${COMMON_CXX_FLAGS} /GS")          # protects against return address overrides (on by default)
  SET(COMMON_CXX_FLAGS "${COMMON_CXX_FLAGS} /Qvec-")       # disable auto vectorizer

  SET(COMMON_CXX_FLAGS "${COMMON_CXX_FLAGS} /Qdiag-disable:11074 ")  # remark #11074: Inlining inhibited by limit max-size
  SET(COMMON_CXX_FLAGS "${COMMON_CXX_FLAGS} /Qdiag-disable:11075 ")  # remark #11075: To get full report use -Qopt-report:4 -Qopt-report-phase ipo

  SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${COMMON_CXX_FLAGS}")
  SET(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} /DEBUG")        # generate debug information
  SET(CMAKE_SHARED_LINKER_FLAGS_DEBUG "${CMAKE_SHARED_LINKER_FLAGS_DEBUG} /DEBUG")  # generate debug information

  SET(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${COMMON_CXX_FLAGS}")
  SET(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Ox")                       # enable full optimizations
  SET(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Oi")                       # inline intrinsic functions
  SET(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Gy")                       # package individual functions
  SET(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Qinline-max-total-size-")  # no size limit when performing inlining
  SET(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Qinline-factor=200")       # increase default inline factors by 2x

  SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} ${COMMON_CXX_FLAGS}")
  SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /Ox")                      # enable full optimizations
  SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /Oi")                      # inline intrinsic functions
  SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /Gy")                      # package individual functions
  SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /Qinline-max-total-size-") # no size limit when performing inlining
  SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /Qinline-factor=200")      # increase default inline factors by 2x
  SET(CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO "${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO} /DEBUG")        # generate debug information
  SET(CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO "${CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO} /DEBUG")  # generate debug information

  SET(SECURE_LINKER_FLAGS "")
  SET(SECURE_LINKER_FLAGS "${SECURE_LINKER_FLAGS} /NXCompat")    # compatible with data execution prevention (on by default)
  SET(SECURE_LINKER_FLAGS "${SECURE_LINKER_FLAGS} /DynamicBase") # random rebase of executable at load time
  IF (CMAKE_SIZEOF_VOID_P EQUAL 4)
    SET(SECURE_LINKER_FLAGS "${SECURE_LINKER_FLAGS} /SafeSEH")     # invoke known exception handlers (Win32 only, x64 exception handlers are safe by design)
  ENDIF()
  SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${SECURE_LINKER_FLAGS}")
  SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${SECURE_LINKER_FLAGS}")

  SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /IGNORE:4217")  # locally defined symbol XXX imported in function YYY (happens as the ISPC API layer uses exported library functions)
  SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /IGNORE:4049")  # warning LNK4049: locally defined symbol _rtcOccluded1M imported
  
  INCLUDE(msvc_post)

ELSE()

  IF (APPLE)
    SET(FLAGS_SSE2 "-xssse3") # on MacOSX ICC does not support SSE2
  ELSE()
    SET(FLAGS_SSE2 "-xsse2")
  ENDIF()
  SET(FLAGS_SSE42  "-xsse4.2")
  SET(FLAGS_AVX    "-xAVX")
  SET(FLAGS_AVX2   "-xCORE-AVX2")
  SET(FLAGS_AVX512KNL "-xMIC-AVX512")
  SET(FLAGS_AVX512SKX "-xCORE-AVX512")

  OPTION(EMBREE_IGNORE_CMAKE_CXX_FLAGS "When enabled Embree ignores default CMAKE_CXX_FLAGS." ON)
  IF (EMBREE_IGNORE_CMAKE_CXX_FLAGS)
    SET(CMAKE_CXX_FLAGS "")
  ENDIF()

  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")                       # enables most warnings
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wformat -Wformat-security")  # enables string format vulnerability warnings
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIE")                       # enables support for more secure position independent execution
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")                       # generate position independent code suitable for shared libraries
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")                  # enables C++11 features
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden")         # makes all symbols hidden by default
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility-inlines-hidden") # makes all inline symbols hidden by default
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -no-ansi-alias")              # disables strict aliasing rules
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fstack-protector")           # protects against return pointer overrides
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_FORTIFY_SOURCE=2")         # perform extra security checks for some standard library calls
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -no-vec")                     # disable auto vectorizer
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fasm-blocks")                # enable assembly blocks
  IF(NOT CMAKE_CXX_COMPILER_WRAPPER STREQUAL "CrayPrgEnv")
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-intel")             # links intel runtime statically
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -no-intel-extensions")      # disables some intel extensions which cause symbols to be exported
  ENDIF()
  
  SET(CMAKE_CXX_FLAGS_DEBUG "")
  SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG")         # enables assertions
  SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DTBB_USE_DEBUG") # configures TBB in debug mode
  SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g")              # generate debug information
  SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0")             # disable all optimizations

  SET(CMAKE_CXX_FLAGS_RELEASE "")
  SET(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DNDEBUG")                    # disable assertions
  SET(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")                         # full optimizations
  SET(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -restrict")                   # enable restrict keyword
  SET(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -no-inline-max-total-size")   # no size limit when performing inlining
  SET(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -no-inline-max-per-compile")  # no maximal number of inlinings per compilation unit
  SET(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -inline-factor=200")          # increase default inline factors limits by 2x
  
  SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO "")
  SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -DDEBUG")                     # enables assertions
  SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -DTBB_USE_DEBUG")             # configures TBB in debug mode
  SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -g")                          # generate debug information
  SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -O3")                         # full optimizations
  SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -restrict")                   # enable restrict keyword
  SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -no-inline-max-total-size")   # no size limit when performing inlining
  SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -no-inline-max-per-compile")  # no maximal number of inlinings per compilation unit
  SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -inline-factor=200")          # increase default inline factors by 2x
    
  #SET(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -g -debug inline-debug-info")
  #SET(CMAKE_EXE_LINKER_FLAGS "-g") 

  IF (APPLE)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mmacosx-version-min=10.7")   # makes sure code runs on older MacOSX versions
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --stdlib=libc++")             # link against libc++ which supports C++11 features
  ELSE(APPLE)
    SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--no-undefined") # issues link error for undefined symbols in shared library
    SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -z noexecstack")     # we do not need an executable stack
    SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -z relro -z now")    # re-arranges data sections to increase security
    SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -z noexecstack")           # we do not need an executable stack
    SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -z relro -z now")          # re-arranges data sections to increase security
    SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pie")                     # enables position independent execution for executable
  ENDIF(APPLE)

ENDIF()
