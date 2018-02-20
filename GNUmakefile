ifeq ($(DEBUG), 1)
    BUILD_DIR := debug
    CXXFLAGS  += -O0 -g3
else
    BUILD_DIR := release
    CXXFLAGS  += -O2
endif

PATH_BIN  := $(BUILD_DIR)/hraw

SRC_DIR   := src
INCLUDES  :=

include syscpp/posix.mk
