BINDIR = bin
OBJDIR = bld
DEPDIR = deps
SRCDIR = src

INCLUDE = -I/usr/local/include

CXFLAGS := -std=c++11 -c -pedantic -Wall -Wextra $(INCLUDE)
LDFLAGS = -L/usr/local/lib/ -L/usr/lib \
		-lSDL2    \
		-lvpx     \
		-lnestegg

BUILD ?= release
ifeq ($(BUILD), debug)
	CXFLAGS += -O0 -g
	LDFLAGS += -O0 -g
else
	CXFLAGS += -O3
	LDFLAGS += -O3
endif

ifeq ($(shell uname), Darwin)
	XCODE = xcrun -sdk macosx
	CXX = $(XCODE) clang++

	#LDFLAGS += -framework Carbon -framework CoreAudio -framework AudioToolbox -framework AudioUnit -framework Cocoa
else
	CXX = g++

	#LDFLAGS += -lasound
endif

LD = $(CXX)
EXECFILE = webm-player

################################################################################
#############       DO NOT CHANGE ANYTHING BELOW THIS PART        ##############
################################################################################
EXEC := $(BINDIR)/$(EXECFILE)

# Get Only the Internal Structure of SRCDIR Recursively
STRUCTURE := $(shell find $(SRCDIR) -type d)
# Get All Files inside the STRUCTURE Variable for the code
CODE_PROJ := $(addsuffix /*,$(STRUCTURE))
CODE_PROJ := $(wildcard $(CODE_PROJ))

# Filter only specific files
SRC_PROJ := $(filter %.cpp,$(CODE_PROJ))
OBJ_PROJ += $(subst $(SRCDIR),$(OBJDIR),$(SRC_PROJ:%.cpp=%.o))

# Object compiling
$(OBJDIR)/%.o: $(addprefix $(SRCDIR)/,%.cpp)
	$(shell mkdir -p $(dir $@))
	$(CXX) $(CXFLAGS) -o $@ $<

# General rules
all: $(EXEC)

$(EXEC): $(OBJ_PROJ)
	$(LD) $(LDFLAGS) -o $@ $^

################################################################################
#############       DO NOT CHANGE ANYTHING ABOVE THIS PART        ##############
################################################################################

clean:
	rm -rf $(OBJDIR)/* $(BINDIR)/*

.PHONY: all install uninstall clean
