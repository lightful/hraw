BUILD_DIR  := build
CXXFLAGS   := -O3 -g3
OUTFILE    := hraw

SRC_DIR  := src
INCLUDES :=
CXXFLAGS += -std=c++11
CXXFLAGS += -Wfloat-equal -Wconversion -Wsign-conversion -Wsign-promo -Wnon-virtual-dtor -Woverloaded-virtual -Wshadow -Wcast-qual
BINOUT   := $(BUILD_DIR)/$(OUTFILE)
LIBS     :=
SYSLIBS  := -lm

CC            := g++
CXXFLAGS      += -Wall -Wextra -pedantic -march=native
SRC_SUBDIRS   := $(shell [ -d $(SRC_DIR) ] && find -L $(SRC_DIR) -type d -not \( -name .git -prune \) -not \( -name .svn -prune \))
BUILD_SUBDIRS := $(patsubst $(SRC_DIR)%,$(BUILD_DIR)%,$(SRC_SUBDIRS))
SRC_FILES     := $(sort $(foreach sdir,$(SRC_SUBDIRS),$(wildcard $(sdir)/*.cpp)) $(AUTOGEN))
OBJ_FILES     := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(SRC_FILES))
DEPS          := $(OBJ_FILES:.o=.d)

vpath %.cpp $(SRC_SUBDIRS) $(BUILD_SUBDIRS)

.PHONY: all checkdirs clean

all:: checkdirs $(LIBOUT) $(BINOUT)
	@install -C -D $(BUILD_DIR)/$(OUTFILE) bin/$(OUTFILE)

checkdirs: $(BUILD_SUBDIRS)

$(BUILD_SUBDIRS):
	@mkdir -p $@

clean::
	@rm -rf $(BUILD_SUBDIRS)

-include $(DEPS)

$(LIBOUT): $(OBJ_FILES)
	ar -r $(LIBOUT) $(OBJ_FILES)

$(BINOUT): $(OBJ_FILES) $(LIBS)
	$(CC) $(LDFLAGS) $(OBJ_FILES) $(CXXFLAGS) -MMD -o $@ $(LIBS) $(SYSLIBS)

define make-dir-goal
$1/%.o: %.cpp
	$(CC) $(CXXFLAGS) $(INCLUDES) -c -MMD $$< -o $$@
endef

$(foreach bdir,$(BUILD_SUBDIRS),$(eval $(call make-dir-goal,$(bdir))))
