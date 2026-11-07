# native.mk - the native reference: the same SDL, codec, engine and core
# sources as guest.mk, built for the host, plus the two harnesses (run-native
# drives the exports directly; run-wbx drives core.wbx through the miniBox
# host exactly as the frontend does). Objects land in build/native.
#
# Usage: make -f native.mk -j$(nproc) [MB=<miniBox checkout>]

.DEFAULT_GOAL := all
include sources.mk

B := $(ROOT)/build/native
MBINCS := -Inative-shim -I$(MB)/source/guest/include -I$(MB)/extern/jsmn
# On aarch64 GCC fuses a multiply and an add into one instruction unless told
# not to, which rounds once where the guest (built with -ffp-contract=off)
# rounds twice: the mixed sound came out different. Both builds keep them apart.
ifeq ($(shell uname -m),aarch64)
NATIVE_CPU := -ffp-contract=off
endif

SDL_CFLAGS := $(SDL_CFLAGS_COMMON) $(SDL_WARN) $(NATIVE_CPU)
CODEC_CFLAGS := $(CODEC_CFLAGS_COMMON) $(NATIVE_CPU)
RSDK_CXXFLAGS := $(RSDK_CXXFLAGS_COMMON) -w $(NATIVE_CPU)
CORE_CFLAGS := $(CORE_CFLAGS_COMMON) $(MBINCS) -I. -Wall $(NATIVE_CPU)
CORE_CXXFLAGS := $(CORE_CXXFLAGS_COMMON) $(MBINCS) -I. -Wall -Wno-unused-function $(NATIVE_CPU)

$(call flags_stamp,$(B),$(SDL_CFLAGS) | $(CODEC_CFLAGS) | $(RSDK_CXXFLAGS) | $(CORE_CFLAGS) | $(CORE_CXXFLAGS))

SDL_OBJS := $(patsubst $(SDL)/%.c,$(B)/sdl/%.o,$(SDL_SRCS))
CODEC_OBJS := $(patsubst $(ROOT)/extern/%.c,$(B)/codec/%.o,$(OGG_SRCS) $(THEORA_SRCS) $(MINIZ_SRCS))
RSDK_OBJS := $(patsubst $(RSDK)/%.cpp,$(B)/rsdk/%.o,$(RSDK_SRCS))
CORE_OBJS := $(addprefix $(B)/core/,$(addsuffix .o,$(CORE_C_NAMES) $(CORE_CXX_NAMES)))

all: $(B)/run-native $(B)/run-wbx

$(B)/sdl/%.o: $(SDL)/%.c $(INC_DIR)/SDL2/.stamp $(B)/flags
	@mkdir -p $(dir $@)
	gcc $(SDL_CFLAGS) -c -o $@ $<

$(B)/codec/%.o: $(ROOT)/extern/%.c $(B)/flags
	@mkdir -p $(dir $@)
	gcc $(CODEC_CFLAGS) -c -o $@ $<

$(B)/rsdk/%.o: $(RSDK)/%.cpp $(PATCH_STAMP) $(INC_DIR)/SDL2/.stamp $(B)/flags
	@mkdir -p $(dir $@)
	g++ $(RSDK_CXXFLAGS) -c -o $@ $<

$(B)/core/%.o: %.c $(CORE_HDRS) $(PATCH_STAMP) $(INC_DIR)/SDL2/.stamp $(B)/flags
	@mkdir -p $(dir $@)
	gcc $(CORE_CFLAGS) -c -o $@ $<

$(B)/core/%.o: %.cpp $(CORE_HDRS) $(PATCH_STAMP) $(INC_DIR)/SDL2/.stamp $(B)/flags
	@mkdir -p $(dir $@)
	g++ $(CORE_CXXFLAGS) -c -o $@ $<

$(B)/core/run-native.o: run-native.c gate-harness.h rsdk-driver.h $(B)/flags
	@mkdir -p $(dir $@)
	gcc -O2 -Wall -DGATE_NATIVE -I. -c -o $@ $<

$(B)/run-native: $(CORE_OBJS) $(RSDK_OBJS) $(CODEC_OBJS) $(SDL_OBJS) $(B)/core/run-native.o
	g++ -o $@ $^ $(WRAP_FLAGS) -lm

# run-wbx links the miniBox host library
MBHOST := $(MB)/build/meson-linux/source/host
$(B)/run-wbx: run-wbx.c gate-harness.h rsdk-driver.h $(B)/flags
	gcc -O2 -Wall -I. -I$(MB)/source/host -o $@ run-wbx.c $(MBHOST)/libminiboxhost.so -Wl,-rpath,$(MBHOST)

clean:
	rm -rf $(B)

.PHONY: all clean
