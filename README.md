# SocketsCPP
Wrapper class around the Unix sockets for OOP operation.
Under heavy development, use at own risk!

Copyright © Manuel Olguín (molguin@kth.se) 2017 -

Distributed under a BSD-3 Clause license, see LICENSE
for details.

# Compiling

The CMake project takes a couple of optional parameters:

- `-DCOMPILE_PROTOBUF:BOOL=(TRUE/FALSE)`:
Compile with Google Protobuf Support.
- `-DCOMPILE_LOGURU:BOOL=(TRUE/FALSE)`:
Compile with **loguru** (https://github.com/emilk/loguru) as a logging
backend. If you're using **loguru** in your project as well, you must
also pass `-DSOCKETS_EXTERNAL_LOGURU:BOOL=TRUE` to CMake to avoid
including the logging library twice.

## Using in CMake project

The best way of using this library is as an external dependency
in a CMake project.

Example CMakeLists.txt:

```cmake
...

include(ExternalProject)

...

# add the relevant folders
include_directories(include)
link_directories(... lib lib/static)

...

# add the project as an external dependency
set(SOCKETSCPP_ROOT ${PROJECT_SOURCE_DIR}/src/socketscpp)
ExternalProject_Add(libsocketscpp
        GIT_REPOSITORY git@github.com:molguin92/SocketsCPP.git
        GIT_PROGRESS TRUE
        SOURCE_DIR ${SOCKETSCPP_ROOT}
        CMAKE_ARGS -DCMAKE_INSTALL_PREFIX:PATH=${PROJECT_SOURCE_DIR})

...

add_dependencies(<Project Name> libsocketscpp)

...

# Link the library
target_link_libraries(<Project Name> socketscpp ...) # note: socketscpp
```

## Standalone install

1. Clone repository: `git clone git@github.com:molguin92/SocketsCPP.git`
2. Build the library (optionally including CMake flags):

```bash
> cd SocketsCPP
> cmake --build -DCOMPILE_PROTOBUF:BOOL=TRUE -DCOMPILE_LOGURU:BOOL=TRUE
```

3. Finally, install the library:
```bash
> sudo make install
```
