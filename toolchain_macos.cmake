set(CMAKE_SYSTEM_NAME Darwin)
set(CMAKE_SYSTEM_PROCESSOR arm64)

set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)

# Set compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++20")

# Set optimization flags
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")

# Enable all warnings
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")

# Add Homebrew paths for dependencies
list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew/opt/icu4c")
list(APPEND CMAKE_LIBRARY_PATH "/opt/homebrew/lib")
list(APPEND CMAKE_INCLUDE_PATH "/opt/homebrew/include")

# Remove x86 specific flags for Apple Silicon
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=armv8-a")
