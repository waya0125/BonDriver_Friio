cmake_minimum_required(VERSION 3.16)
project(BonDriver_Friio LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# MSVC ビルド時は CRT を静的リンク（/MT）し、VC++ 再頒布可能パッケージ不要にする
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_library(friio_core STATIC
    src/AsieStreamCipher.cpp
    src/Tc90522.cpp
    src/SatTsid.cpp
    src/Fc0013.cpp
    src/Qm1d1c0042.cpp
    src/Settler.cpp
    src/Multi2.cpp
    src/B25Decoder.cpp
    src/Mtvu3sCardReader.cpp
    src/SatChannelTable.cpp
)
target_include_directories(friio_core PUBLIC src include)

if(WIN32)
    add_library(BonDriver_Friio SHARED
        src/WinUsbDevice.cpp
        src/Ut100bDevice.cpp
        src/Mtvu3sDevice.cpp
        src/BonDriver.cpp
        src/DllMain.cpp
        src/BonDriver_Friio.def
    )
    target_link_libraries(BonDriver_Friio PRIVATE friio_core winusb setupapi ole32 advapi32 bcrypt)
    target_compile_definitions(BonDriver_Friio PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
    set_target_properties(BonDriver_Friio PROPERTIES
        PREFIX ""
        OUTPUT_NAME "BonDriver_Friio"
    )
    if(MINGW)
        target_link_options(BonDriver_Friio PRIVATE -static -static-libgcc -static-libstdc++)
    endif()

    add_executable(bon_test tests/bon_test.cpp)
    target_include_directories(bon_test PRIVATE include)
    target_compile_definitions(bon_test PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
    if(MINGW)
        target_link_options(bon_test PRIVATE -static -static-libgcc -static-libstdc++)
    endif()
endif()

include(CTest)
if(BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
    add_executable(test_core tests/test_core.cpp)
    target_link_libraries(test_core PRIVATE friio_core)
    add_test(NAME test_core COMMAND test_core)
endif()
