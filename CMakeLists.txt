SET(PROJECT_NAME DSO)
PROJECT(${PROJECT_NAME})
CMAKE_MINIMUM_REQUIRED(VERSION 2.8.12)

# Set standard C++ 14
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(BUILD_TYPE Release)
set(EXECUTABLE_OUTPUT_PATH bin)
set(LIBRARY_OUTPUT_PATH lib)
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

# --- Architecture detection ---
# Build artifacts live inside the build directory chosen by the caller
# (e.g. build_arm/ via build_jetson.sh, build_ubuntu/ via build_ubuntu.sh),
# so ARM and x86_64 builds never collide.
if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|armv8|armv7|arm")
    set(DSO_ARCH "ARM")
    message(STATUS "DSO: target architecture = ARM (${CMAKE_SYSTEM_PROCESSOR})")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|amd64")
    set(DSO_ARCH "X86")
    message(STATUS "DSO: target architecture = x86_64 (${CMAKE_SYSTEM_PROCESSOR})")
else()
    set(DSO_ARCH "UNKNOWN")
    message(WARNING "DSO: unknown architecture (${CMAKE_SYSTEM_PROCESSOR}); falling back to generic flags")
endif()

# --- Required Libraries ---
find_package(SuiteParse REQUIRED)
find_package(Eigen3 REQUIRED)
find_package(Boost COMPONENTS system thread REQUIRED)
find_package(OpenGL REQUIRED)

# --- WORKAROUND FOR EIGEN TARGET ERROR ---
if(NOT TARGET Eigen3::Eigen)
    add_library(Eigen3::Eigen INTERFACE IMPORTED)
    set_target_properties(Eigen3::Eigen PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${EIGEN3_INCLUDE_DIR}"
    )
endif()

# --- Optional Libraries ---
find_package(LibZip QUIET)
find_package(Pangolin REQUIRED)
find_package(OpenCV REQUIRED)

# Flags
add_definitions("-DENABLE_SSE")
# Release-grade optimisation. NOTE: do NOT add -ffast-math: it strips DSO's
# NaN/Inf guards (Gauss-Newton residual checks) and the solver diverges.
if(DSO_ARCH STREQUAL "ARM")
    # On ARM the SSE intrinsics in DSO are translated to NEON via sse2neon.
    set(CMAKE_CXX_FLAGS "${SSE_FLAGS} -O3 -DNDEBUG -march=native -mtune=native -funroll-loops -DGL_GLEXT_PROTOTYPES")
elseif(DSO_ARCH STREQUAL "X86")
    # On x86_64 use the real SSE/AVX intrinsics from the toolchain headers.
    set(CMAKE_CXX_FLAGS "${SSE_FLAGS} -O3 -DNDEBUG -march=native -mtune=native -msse4.2 -funroll-loops -DGL_GLEXT_PROTOTYPES")
else()
    set(CMAKE_CXX_FLAGS "${SSE_FLAGS} -O3 -DNDEBUG -funroll-loops -DGL_GLEXT_PROTOTYPES")
endif()

# Source files
set(dso_SOURCE_FILES
  ${PROJECT_SOURCE_DIR}/src/FullSystem/FullSystem.cpp
  ${PROJECT_SOURCE_DIR}/src/FullSystem/FullSystemOptimize.cpp
  ${PROJECT_SOURCE_DIR}/src/FullSystem/FullSystemOptPoint.cpp
  ${PROJECT_SOURCE_DIR}/src/FullSystem/FullSystemDebugStuff.cpp
  ${PROJECT_SOURCE_DIR}/src/FullSystem/FullSystemMarginalize.cpp
  ${PROJECT_SOURCE_DIR}/src/FullSystem/Residuals.cpp
  ${PROJECT_SOURCE_DIR}/src/FullSystem/CoarseTracker.cpp
  ${PROJECT_SOURCE_DIR}/src/FullSystem/CoarseInitializer.cpp
  ${PROJECT_SOURCE_DIR}/src/FullSystem/ImmaturePoint.cpp
  ${PROJECT_SOURCE_DIR}/src/FullSystem/HessianBlocks.cpp
  ${PROJECT_SOURCE_DIR}/src/FullSystem/PixelSelector2.cpp
  ${PROJECT_SOURCE_DIR}/src/OptimizationBackend/EnergyFunctional.cpp
  ${PROJECT_SOURCE_DIR}/src/OptimizationBackend/AccumulatedTopHessian.cpp
  ${PROJECT_SOURCE_DIR}/src/OptimizationBackend/AccumulatedSCHessian.cpp
  ${PROJECT_SOURCE_DIR}/src/OptimizationBackend/EnergyFunctionalStructs.cpp
  ${PROJECT_SOURCE_DIR}/src/util/settings.cpp
  ${PROJECT_SOURCE_DIR}/src/util/Undistort.cpp
  ${PROJECT_SOURCE_DIR}/src/util/globalCalib.cpp
  ${PROJECT_SOURCE_DIR}/src/util/AnypointRemap.cpp
  ${PROJECT_SOURCE_DIR}/src/util/AnypointParams.cpp
)

# Include Directories
include_directories(
  ${PROJECT_SOURCE_DIR}/src
  ${PROJECT_SOURCE_DIR}/thirdparty/Sophus
  ${EIGEN3_INCLUDE_DIR}
  ${Pangolin_INCLUDE_DIRS}
  ${OpenCV_INCLUDE_DIRS}
  ${OPENGL_INCLUDE_DIR}          # ← added globally so all targets see it
)

# sse2neon shim is only needed on ARM (translates SSE intrinsics → NEON).
# On x86_64 the real <xmmintrin.h> family from the toolchain is used instead.
if(DSO_ARCH STREQUAL "ARM")
    include_directories(${PROJECT_SOURCE_DIR}/thirdparty/sse2neon)
endif()

# Core DSO library
add_library(dso ${dso_SOURCE_FILES})

if(OpenCV_FOUND AND Pangolin_FOUND)

    # Add display sources to the library
    target_sources(dso PRIVATE
        ${PROJECT_SOURCE_DIR}/src/IOWrapper/Pangolin/KeyFrameDisplay.cpp
        ${PROJECT_SOURCE_DIR}/src/IOWrapper/Pangolin/PangolinDSOViewer.cpp
        ${PROJECT_SOURCE_DIR}/src/IOWrapper/Pangolin/AnypointShader.cpp
        ${PROJECT_SOURCE_DIR}/src/IOWrapper/OpenCV/ImageDisplay_OpenCV.cpp
        ${PROJECT_SOURCE_DIR}/src/IOWrapper/OpenCV/ImageRW_OpenCV.cpp
    )

    # ── dso_dataset (original, unchanged) ────────────────────────────────────
    add_executable(dso_dataset ${PROJECT_SOURCE_DIR}/src/main_dso_pangolin.cpp)
    target_link_libraries(dso_dataset
        dso
        Eigen3::Eigen
        ${Pangolin_LIBRARIES}
        ${OpenCV_LIBS}
        ${Boost_LIBRARIES}
        ${OPENGL_LIBRARIES}
        cxsparse
        cholmod
    )

    # ── dso_live (existing live target, unchanged) ────────────────────────────
    add_executable(dso_live ${PROJECT_SOURCE_DIR}/src/main_live.cpp)
    target_link_libraries(dso_live
        dso
        ${OpenCV_LIBS}
        ${Pangolin_LIBRARIES}
        ${Boost_LIBRARIES}
        ${OPENGL_LIBRARIES}
        Eigen3::Eigen
        cxsparse
        cholmod
        pthread
    )

    # ── dso_camera (real-time camera, fixed) ──────────────────────────────────
    add_executable(dso_camera ${PROJECT_SOURCE_DIR}/src/main_camera.cpp)
    target_link_libraries(dso_camera
        dso                      # core DSO static lib
        Eigen3::Eigen            # ← was missing
        ${Pangolin_LIBRARIES}    # ← FIXED: was ${PANGOLIN_LIBRARIES} (wrong case)
        ${OpenCV_LIBS}
        ${Boost_LIBRARIES}
        ${OPENGL_LIBRARIES}      # ← FIXED: was missing (caused all GL errors)
        cxsparse                 # ← was missing
        cholmod                  # ← was missing
        pthread                  # ← was missing
        dl
    )
    # No separate target_include_directories needed: the global include_directories
    # above already covers everything dso_camera needs.

endif()

