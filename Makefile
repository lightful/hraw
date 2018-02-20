# Windows Makefile (for nmake)

BUILD_DIR = build
PATH_BIN  = $(BUILD_DIR)/hraw.exe

SRC_DIR   = src
INCLUDES  =

include syscpp/nmake.mk
