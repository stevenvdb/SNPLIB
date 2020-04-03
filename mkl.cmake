if (${CMAKE_SYSTEM_NAME} MATCHES Windows)
    if (NOT MKL_ROOT)
        set(MKL_ROOT "C:/Program Files (x86)/IntelSWTools/compilers_and_libraries/windows/mkl")
    endif ()
    find_library(MKLCORE NAMES mkl_core.lib PATHS ${MKL_ROOT}/lib/intel64)
    find_library(MKLSEQ NAMES mkl_sequential.lib PATHS ${MKL_ROOT}/lib/intel64)
    find_library(MKLLP64 NAMES mkl_intel_lp64.lib PATHS ${MKL_ROOT}/lib/intel64)
    set(CMAKE_CXX_FLAGS "/O2yi /EHs /MD")
endif ()
if (${CMAKE_SYSTEM_NAME} MATCHES Darwin)
    if (NOT MKL_ROOT)
        set(MKL_ROOT /opt/intel/mkl)
    endif ()
    find_library(MKLCORE NAMES libmkl_core.a PATHS ${MKL_ROOT}/lib/)
    find_library(MKLSEQ NAMES libmkl_sequential.a PATHS ${MKL_ROOT}/lib/)
    find_library(MKLLP64 NAMES libmkl_intel_lp64.a PATHS ${MKL_ROOT}/lib/)
    set(CMAKE_CXX_FLAGS "-march=native -O3 -fPIC -flto -fvisibility=hidden")
endif ()
if (${CMAKE_SYSTEM_NAME} MATCHES Linux)
    if (NOT MKL_ROOT)
        set(MKL_ROOT /opt/intel/mkl)
    endif ()
    find_library(MKLCORE NAMES libmkl_core.a PATHS ${MKL_ROOT}/lib/intel64)
    find_library(MKLSEQ NAMES libmkl_sequential.a PATHS ${MKL_ROOT}/lib/intel64)
    find_library(MKLLP64 NAMES libmkl_intel_lp64.a PATHS ${MKL_ROOT}/lib/intel64)
    set(CMAKE_CXX_FLAGS "-march=native -O3 -fPIC -flto -fvisibility=hidden")
endif ()
include_directories(${MKL_ROOT}/include)