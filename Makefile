# Broforce-lite - build inside the miyoomini-toolchain container:
#   ARM game binary:  make            (needs `source /root/setup-env.sh`)
#   native headless test:  make headless && ./build/headless out/
CXXFLAGS_COMMON := -std=c++17 -O2 -Wall -Wextra
SRC_CORE := src/gfx.cpp src/data.cpp src/world.cpp src/boss.cpp src/logic.cpp src/render.cpp src/audio.cpp src/scenery.cpp
BUILD := build

ARM_FLAGS := -DPLATFORM_MIYOO_MINI=1 -marm -mtune=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard -march=armv7ve+simd -Wno-psabi

all: $(BUILD)/broyoo

$(BUILD)/broyoo: $(SRC_CORE) src/main.cpp src/game.h src/gfx.h
	@mkdir -p $(BUILD)
	$(CROSS_COMPILE)c++ $(CXXFLAGS_COMMON) $(ARM_FLAGS) $(SRC_CORE) src/main.cpp -o $@ -lSDL -static-libstdc++ -static-libgcc

headless: $(BUILD)/headless

$(BUILD)/headless: $(SRC_CORE) src/headless.cpp src/game.h src/gfx.h
	@mkdir -p $(BUILD)
	g++ $(CXXFLAGS_COMMON) -g $(SRC_CORE) src/headless.cpp -o $@

clean:
	rm -rf $(BUILD)

.PHONY: all headless clean
