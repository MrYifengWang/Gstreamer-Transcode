
cmake_minimum_required(VERSION 2.8)
# project name
project(cvts_transcoder)

#option
option(MK_ORANGE "option for target" OFF)
option(MK_DEBIAN "option for target" OFF)
option(MK_X86 "option for target" OFF)
option(MK_X86_CPU "option for target" OFF)

option(MK_GDK "option for target" OFF)
option(MK_VCT "option for target" OFF)

if(MK_GDK)
	ADD_DEFINITIONS(-DMAKE_GDK)
else()
	ADD_DEFINITIONS(-DMAKE_VCT)
endif()

if(MK_DEBIAN)
  ADD_DEFINITIONS(-DGST_V14)
  ADD_DEFINITIONS(-DNO_ENHANCE)
  ADD_DEFINITIONS(-DGST_V16)
endif()

if(MK_X86)
  ADD_DEFINITIONS(-DGST_V16)
  ADD_DEFINITIONS(-DMAKE_X86)
endif()

if(MK_X86_CPU)
  ADD_DEFINITIONS(-DGST_V16)
  ADD_DEFINITIONS(-DNO_ENHANCE)
  ADD_DEFINITIONS(-DMAKE_X86)
endif()

set(CMAKE_C_COMPILER gcc)
set(CMAKE_CXX_COMPILER g++)

set(CMAKE_BUILD_RPATH_USE_ORIGIN TRUE)
set(CMAKE_SKIP_RPATH FALSE)
set(CMAKE_SKIP_BUILD_RPATH FALSE)
set(CMAKE_BUILD_RPATH "\${ORIGIN}/lib")
set(CMAKE_INSTALL_RPATH "\${ORIGIN}/lib")
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

set(ENV{PKG_CONFIG_PATH} usr/lib/x86_64-linux-gnu/pkgconfig)

find_package(PkgConfig REQUIRED)

#pkg_search_module(PKG_OPENCV REQUIRED opencv)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -g ")
ADD_DEFINITIONS(-DDD_DEBUG)
MESSAGE(STATUS "operation system is ${CMAKE_SYSTEM}")
MESSAGE(STATUS "CMAKE_SYSTEM_NAME is ${CMAKE_SYSTEM}")


if(MK_X86_CPU)
  set(SYS_USR_LIB /usr/lib/x86_64-linux-gnu)
  set(SYS_LIB /lib/x86_64-linux-gnu)
  set(PLATFORM_SUB x86_64-linux-gnu)
  set(MX_PATH /usr/local/VideoFX)
  set(MX_AUDIO_PATH /usr/local/Audio_Effects_SDK/nvafx)
  

else()
  set(SYS_USR_LIB /usr/lib/aarch64-linux-gnu)
  set(SYS_LIB /lib/aarch64-linux-gnu)
  set(PLATFORM_SUB aarch64-linux-gnu)
endif()

# gst
include_directories(
       /usr/includes
        /usr/include/gstreamer-1.0
        /usr/lib/${PLATFORM_SUB}/gstreamer-1.0
        /usr/lib/gstreamer-1.0
        /usr/include/glib-2.0
        ${SYS_USR_LIB}/glib-2.0/include
        /usr/include/orc-0.4
        /usr/include/rockchip
        /usr/local/include/opencv2
        /usr/local/include
        /usr/local/lib/${PLATFORM_SUB}
        ${CMAKE_CURRENT_SOURCE_DIR}/include
       # ${CMAKE_CURRENT_SOURCE_DIR}/librga
        # rk3588
        /usr/local/include
        /usr/local/lib/${PLATFORM_SUB}
        /usr/include/opencv4
        /home/orangepi/ts/demo/libcurl
        ${MX_PATH}/include
        ${MX_AUDIO_PATH}/include
        /usr/include/opencv4/opencv2
        /usr/local/cuda/include
      
)


link_libraries(
        ${SYS_USR_LIB}/libgstreamer-1.0.so.0
        ${SYS_USR_LIB}/gstreamer-1.0/libgstcoreelements.so
      #  ${SYS_USR_LIB}/gstreamer-1.0/libgstvideo4linux2.so
        ${SYS_USR_LIB}/gstreamer-1.0/libgstvoaacenc.so
        ${SYS_USR_LIB}/gstreamer-1.0/libgstaudiotestsrc.so
        ${SYS_USR_LIB}/gstreamer-1.0/libgstaudioparsers.so
        ${SYS_USR_LIB}/libgstrtsp-1.0.so
        ${SYS_USR_LIB}/libgstsdp-1.0.so
        ${SYS_USR_LIB}/libgobject-2.0.so.0
        ${SYS_USR_LIB}/libglib-2.0.so.0
        ${SYS_USR_LIB}/gstreamer-1.0/libgstx265.so
        ${SYS_USR_LIB}/gstreamer-1.0/libgstx264.so
        ${SYS_USR_LIB}/libgstbase-1.0.so.0
        ${SYS_USR_LIB}/libgstaudio-1.0.so.0
        ${SYS_USR_LIB}/libcurl.so.4
        ${SYS_USR_LIB}/libgstpbutils-1.0.so
	      ${SYS_USR_LIB}/libgstvideo-1.0.so.0
        ${SYS_USR_LIB}/libgstapp-1.0.so.0
        ${SYS_LIB}/libc.so.6
        ${SYS_LIB}/libdl.so.2
        ${SYS_LIB}/libpthread.so.0
        ${SYS_USR_LIB}/libgmodule-2.0.so.0
        ${SYS_LIB}/libm.so.6
        ${SYS_LIB}/libpcre.so.3
        ${SYS_USR_LIB}/libffi.so

)
if(MK_ORANGE)
link_libraries(
	/usr/local/lib/gstreamer-1.0/libgstrtspclientsink.so
	/usr/local/lib/libgstrtspserver-1.0.so 
)
endif()
if(MK_X86)
link_libraries(
	${SYS_USR_LIB}/gstreamer-1.0/libgstrtspclientsink.so
#	${SYS_USR_LIB}/gstreamer-1.0/libgstrtspserver-1.0.so 
  ${SYS_USR_LIB}/gstreamer-1.0/deepstream/libgstnvvideo4linux2.so
  ${SYS_USR_LIB}/gstreamer-1.0/deepstream/libgstnvvideoconvert.so
          ${MX_PATH}/lib/libNVCVImage.so
        ${MX_PATH}/lib/libVideoFX.so
        ${MX_PATH}/lib/libNVTRTLogger.so
        ${MX_AUDIO_PATH}/lib/libnv_audiofx.so
        /usr/local/cuda/lib64/libcudart.so 
                ${SYS_USR_LIB}/libopencv_core.so
        ${SYS_USR_LIB}/libopencv_highgui.so
        ${SYS_USR_LIB}/libopencv_imgproc.so
        ${SYS_USR_LIB}/libopencv_imgcodecs.so
)
endif()


find_package(CURL REQUIRED)
find_package(OpenCV REQUIRED)
set(link_libs curl pthread opencv_core opencv_highgui opencv_imgcodecs)

set(SRC_LIST  
	./src/GstTask.cpp
	./src/RtspTask.cpp
	./src/DirectTranscoding.cpp
	./src/JpgTranscoding.cpp
	./src/RtpTask.cpp
	./src/TaskManager.cpp
	./src/YuvTask.cpp
	./src/TaskFlow.cpp
	./src/EncodingTask.cpp
	./src/EnhanceTask.cpp
	./src/PacketQueue.cpp
	./src/StatusQueue.cpp	
	./src/GstUtil.cpp
	./src/CVCTInterface.cpp
 ./src/DebugLog.cpp
 ./src/FileSrcTask.cpp
	./common/ts_event.cpp
	./common/ts_mutex.cpp
	./common/ts_semaphore.cpp
	./common/ts_time.cpp
	./common/ts_jsoncpp.cpp
  ./common/aydes.cpp
  ./common/EdUrlParser.cpp
  ./common/ts_thread.cpp)
  
if(MK_X86)
list(APPEND SRC_LIST ./src/AudioFilterTask.cpp) 
endif()
	
if(MK_GDK)
  set(APP_SRC  main.c cJSON.c ./src/IVCTInterface.cpp ${SRC_LIST})
  add_executable (demo ${APP_SRC})
  add_library(gdk STATIC ./src/IVCTInterface.cpp ${SRC_LIST})
  add_library(gdk_so SHARED ./src/IVCTInterface.cpp ${SRC_LIST})
else()
  set(APP_SRC  main.c cJSON.c ./src/IVCTInterface.cpp ${SRC_LIST})
  add_executable (demo ${APP_SRC})
  add_library(vct STATIC ./src/IVCTInterface.cpp ${SRC_LIST})
  add_library(vct_so SHARED ./src/IVCTInterface.cpp ${SRC_LIST})
endif()

#target_link_libraries( main ${PKG_OPENCV_LIBRARIES})
#target_link_libraries(main_gst_rtsp_ ${link_libs})

