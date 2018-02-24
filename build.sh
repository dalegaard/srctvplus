#!/bin/bash
set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

SDKPATH="${SDKPATH:-${DIR}/vendor/sdk}"
TFTRUEPATH="${TFTRUEPATH:-${DIR}/vendor/tftrue}"

CXXFLAGS="$CXXFLAGS -march=pentium3 -mmmx -msse -m32 -Wall -fvisibility=hidden"
CXXFLAGS="$CXXFLAGS -fvisibility-inlines-hidden -fno-strict-aliasing"
CXXFLAGS="$CXXFLAGS -Wno-delete-non-virtual-dtor -Wno-unused -Wno-reorder"
CXXFLAGS="$CXXFLAGS -Wno-overloaded-virtual -Wno-unknown-pragmas -Wno-invalid-offsetof"
CXXFLAGS="$CXXFLAGS -Wno-sign-compare -std=c++11 -Dtypeof=decltype"
CXXFLAGS="$CXXFLAGS -O3 -pthread -lpthread"

CXXFLAGS="$CXXFLAGS -DDEBUG"
CXXFLAGS="$CXXFLAGS -DGNUC -DRAD_TELEMETRY_DISABLED -DLINUX -D_LINUX -DPOSIX"
CXXFLAGS="$CXXFLAGS -DNO_MALLOC_OVERRIDE -D_FORTIFY_SOURCE=0"
CXXFLAGS="$CXXFLAGS -DVPROF_LEVEL=1 -DSWDS -D_finite=finite -Dstricmp=strcasecmp"
CXXFLAGS="$CXXFLAGS -D_stricmp=strcasecmp -D_strnicmp=strncasecmp -Dstrnicmp=strncasecmp"
CXXFLAGS="$CXXFLAGS -D_vsnprintf=vsnprintf -D_alloca=alloca -Dstrcmpi=strcasecmp"

CXXFLAGS="$CXXFLAGS -I${SDKPATH}/common"
CXXFLAGS="$CXXFLAGS -I${SDKPATH}/public"
CXXFLAGS="$CXXFLAGS -I${SDKPATH}/public/tier0"
CXXFLAGS="$CXXFLAGS -I${SDKPATH}/public/tier1"
CXXFLAGS="$CXXFLAGS -I${SDKPATH}/game/shared"
CXXFLAGS="$CXXFLAGS -I${SDKPATH}/game/server"
CXXFLAGS="$CXXFLAGS -I${TFTRUEPATH}/FunctionRoute/"

LDFLAGS="$LDFLAGS -lrt -lm -ldl -m32 -flto -shared -fPIC -static-libgcc -static-libstdc++"
LDFLAGS="$LDFLAGS -Wl,--version-script=version-script"

OBJS="$OBJS -L${SDKPATH}/lib/linux/ -ltier0_srv -lvstdlib_srv"
OBJS="$OBJS ${SDKPATH}/lib/linux/tier1_i486.a"
OBJS="$OBJS ${SDKPATH}/lib/linux/tier2_i486.a"
OBJS="$OBJS ${SDKPATH}/lib/linux/mathlib_i486.a"
OBJS="$OBJS ${TFTRUEPATH}/FunctionRoute/FunctionRoute.a"

set -x
g++ $CXXFLAGS -c -o srctvplus.o srctvplus.cpp
gcc $CXXFLAGS $LDFLAGS -o srctvplus.so srctvplus.o $OBJS
