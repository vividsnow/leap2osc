OS := $(shell uname)
ARCH := $(shell uname -m)

ifeq ($(OS), Linux)
  ifeq ($(ARCH), x86_64)
    LEAP_LIBRARY := ./LeapSDK/lib/x64/libLeap.so -Wl,-rpath,./LeapSDK/lib/x64
  else
    LEAP_LIBRARY := ./LeapSDK/lib/x86/libLeap.so -Wl,-rpath,./LeapSDK/lib/x86
  endif
else
  # OS X
  LEAP_LIBRARY := ../LeapSDK/lib/libLeap.dylib
endif

leap2osc: leap2osc.cpp
	$(CXX) -Wall -g -I./LeapSDK/include -I./oscpack leap2osc.cpp -o leap2osc $(LEAP_LIBRARY) ./oscpack/build/liboscpack.a
ifeq ($(OS), Darwin)
	install_name_tool -change @loader_path/libLeap.dylib ./LeapSDK/lib/libLeap.dylib leap2osc
endif

clean:
	rm -rf leap2osc leap2osc.dSYM
