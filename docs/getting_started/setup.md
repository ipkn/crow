This page explains how to set Crow up for use with your project.


##Requirements
 - C++ compiler with C++14 support.
    - Continuous Testing on g++-9.3 and clang-7.0, AMD64 (x86_64) and Arm64 v8
 - boost library (1.70 or later).
 - (optional) CMake and Python3 to build tests and/or examples.
 - (optional) Linking with jemalloc/tcmalloc is recommended for speed.
<br><br>

##Installing Requirements
###Ubuntu
`sudo apt-get install libboost-all-dev`

###OSX
`brew install boost`

###Windows
Download boost from [here](https://www.boost.org/) and install it

##Downloading
Either run `git clone https://github.com/crowcpp/crow.git` or download `crow_all.h` from the releases section. You can also download a zip of the project on github.

##Includes folder
1. Copy the `/includes` folder to your project's root folder.
2. Add `#!cpp #include "path/to/includes/crow.h"` to your `.cpp` file.
3. For any middlewares, add `#!cpp #include "path/to/includes/middlewares/some_middleware.h"`.
<br><br>

##Single header file
If you've downloaded `crow_all.h`, you can skip to step 4.

1. Make sure you have python 3 installed. 
2. Open a terminal (or `cmd.exe`) instance in `/path/to/crow/scripts`.
3. Run `python merge_all.py ../include crow_all.h` (replace `/` with `\` if you're on Windows).
4. Copy the `crow_all.h` file to where you put your libraries (if you don't know where this is, you can put it anywhere).
5. Add `#!cpp #include "path/to/crow_all.h"` to your `.cpp` file.
<br><br>
**Note**: All middlewares are included with the merged header file, if you would like to include or exclude middlewares use the `-e` or `-i` arguments.
<br><br>

##building via CLI
To build a crow Project, do the following:

###GCC (G++)
 - Release: `g++ main.cpp -lpthread -lboost_system`.
 - Debug: `g++ main.cpp -ggdb -lpthread -lboost_system -D CROW_ENABLE_DEBUG`.
 - SSL: `g++ main.cpp -lssl -lcrypto -lpthread -lboost_system -D CROW_ENABLE_SSL`.

###Clang
 - Release: `clang++ main.cpp -lpthread -lboost_system`.
 - Debug: `clang++ main.cpp -g -lpthread -lboost_system -DCROW_ENABLE_DEBUG`.
 - SSL: `clang++ main.cpp -lssl -lcrypto -lpthread -lboost_system -DCROW_ENABLE_SSL`.

###Microsoft Visual Studio
***Help needed***


##building via CMake
Add the following to your `CMakeLists.txt`:
``` cmake linenums="1"
find_package(Threads)
find_package(OpenSSL)

if(OPENSSL_FOUND)
	include_directories(${OPENSSL_INCLUDE_DIR})
endif()

if (NOT CMAKE_BUILD_TYPE)
	message(STATUS "No build type selected, default to Release")
	set(CMAKE_BUILD_TYPE "Release")
endif()

if (MSVC)
	set(Boost_USE_STATIC_LIBS "On")
	find_package( Boost 1.70 COMPONENTS system thread regex REQUIRED )
else()
	find_package( Boost 1.70 COMPONENTS system thread REQUIRED )
endif()

include_directories(${Boost_INCLUDE_DIR})

set(PROJECT_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include)

include_directories("${PROJECT_INCLUDE_DIR}")
```
**Note**: The last 2 lines are unnecessary if you're using `crow_all.h`.

##Building Crow tests and examples
Out-of-source build with CMake is recommended.

```
mkdir build
cd build
cmake ..
make
```
Running Cmake will create `crow_all.h` file and place it in the build directory.

##Installing Crow

if you wish to use crow globally without copying `crow_all.h` in your projects, you can install `crow` on your machine by using `make install` command instead of `make` after `cmake..`. Thus, the procedure will look like

```
mkdir build
cd build
cmake ..
make install
```
`make install` will copy `crow_all.h` automatically in your `/usr/local/include` thus making it available globally for use.<br>

You can run tests with following commands:
```
ctest -V
```
