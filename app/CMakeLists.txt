cmake_minimum_required(VERSION 3.4.1)

 # TODO 添加第三方库
 # TODO add_library(libavcodec
 # TODO 原先生成的.so文件在编译后会自动添加上前缀lib和后缀.so，
 # TODO       在定义库的名字时，不要加前缀lib和后缀 .so，
 # TODO       不然会报错:java.lang.UnsatisfiedLinkError: Couldn't load xxx : findLibrary returned null
 add_library(avcodec
             # TODO STATIC表示静态的.a的库，SHARED表示.so的库
             SHARED
             IMPORTED)
 set_target_properties(avcodec
                       PROPERTIES IMPORTED_LOCATION
                       # TODO ${CMAKE_SOURCE_DIR}：表示 CMakeLists.txt的当前文件夹路径
                       # TODO ${ANDROID_ABI}：编译时会自动根据 CPU架构去选择相应的库
                       # TODO ABI文件夹上面不要再分层，直接就 jniLibs/${ANDROID_ABI}/
                       # TODO ${CMAKE_SOURCE_DIR}/src/main/jniLibs/ffmpeg/${ANDROID_ABI}/libavcodec.so
                       ${CMAKE_SOURCE_DIR}/src/main/jniLibs/${ANDROID_ABI}/libavcodec.so)

add_library( avdevice
             SHARED
             IMPORTED)
set_target_properties(avdevice
                      PROPERTIES IMPORTED_LOCATION
                      ${CMAKE_SOURCE_DIR}/src/main/jniLibs/${ANDROID_ABI}/libavdevice.so)

add_library( avfilter
             SHARED
             IMPORTED)
set_target_properties(avfilter
                      PROPERTIES IMPORTED_LOCATION
                      ${CMAKE_SOURCE_DIR}/src/main/jniLibs/${ANDROID_ABI}/libavfilter.so)

add_library( avformat
             SHARED
             IMPORTED)
set_target_properties(avformat
                      PROPERTIES IMPORTED_LOCATION
                      ${CMAKE_SOURCE_DIR}/src/main/jniLibs/${ANDROID_ABI}/libavformat.so)

add_library( avresample
             SHARED
             IMPORTED)
set_target_properties(avresample
                      PROPERTIES IMPORTED_LOCATION
                      ${CMAKE_SOURCE_DIR}/src/main/jniLibs/${ANDROID_ABI}/libavresample.so)

add_library( avutil
             SHARED
             IMPORTED)
set_target_properties(avutil
                      PROPERTIES IMPORTED_LOCATION
                      ${CMAKE_SOURCE_DIR}/src/main/jniLibs/${ANDROID_ABI}/libavutil.so)

add_library( swresample
             SHARED
             IMPORTED)
set_target_properties(swresample
                      PROPERTIES IMPORTED_LOCATION
                      ${CMAKE_SOURCE_DIR}/src/main/jniLibs/${ANDROID_ABI}/libswresample.so)

add_library( swscale
             SHARED
             IMPORTED)
set_target_properties(swscale
                      PROPERTIES IMPORTED_LOCATION
                      ${CMAKE_SOURCE_DIR}/src/main/jniLibs/${ANDROID_ABI}/libswscale.so)

# TODO include_directories( src/main/jniLibs/${ANDROID_ABI}/include )
# TODO 路径指向上面会编译出错，指向下面的路径就没问题
include_directories( src/main/cpp/include )

add_library( native-lib
             SHARED
             src/main/cpp/native-lib.cpp )

find_library(log-lib
             log )

target_link_libraries( native-lib

                       ${log-lib}

                       avcodec
                       avdevice
                       avfilter
                       avformat
                       avresample
                       avutil
                       swresample
                       swscale)