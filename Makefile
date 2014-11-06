THIS_DIR := $(dir $(lastword $(MAKEFILE_LIST)))

CC=gcc
CPP=g++

# OVERRIDES: Set these to non-zero if data generation or code generation are
# being triggered when what you really want is an incremental build on existing
# data/code; this can happen, for example, if core C++ files have been touched
# but you are simply working with priv2 or demo
SKIP_DATA_COLLECTION ?= 0
SKIP_CODE_GENERATION ?= 0

# Default to low debug level
DEBUG_LEVEL ?= 1
# Default to Linux-like uid scheme
UIDS ?= -1_0_1000_1001_1002_1003_1004_1005

PRIV_FLAGS += -DSET_UIDS_ONLY

CFLAGS += -Wall -I. -I$(THIS_DIR)src -I$(THIS_DIR)c/priv -I$(THIS_DIR)c/priv2 -I$(THIS_DIR)test -I/usr/local/include -DLIVING_ON_THE_EDGE

SIMULATE_NO_SETRESUID ?= 0
ifneq ($(SIMULATE_NO_SETRESUID),0)
CFLAGS += -DDEBUG_SETRESUID -DHAS_SETRESUID=0
endif

ENABLE_OPTIMIZED ?= 0
ifeq ($(ENABLE_OPTIMIZED),0)
CFLAGS += -g -O0 -DDEBUG -DDEBUG_ASSERT -DDEBUG_LEVEL=$(DEBUG_LEVEL)
else
CFLAGS += -O2
endif

DISABLE_ASSERTIONS ?= 0
ifeq ($(DISABLE_ASSERTIONS),0)
CFLAGS += -DDISABLE_ASSERTIONS=0
else
CFLAGS += -DDISABLE_ASSERTIONS=1
endif

MULTITHREADED ?= 0
ifneq ($(MULTITHREADED),0)
CFLAGS += -DMULTITHREADED
LDFLAGS += -lpthread
endif

DATA_LOCATION ?= $(THIS_DIR)data

ifeq ($(ENABLE_OPTIMIZED),0)
ifeq ($(DISABLE_ASSERTIONS),0)
BUILD_LOCATION ?= $(THIS_DIR)Debug+Asserts
else
BUILD_LOCATION ?= $(THIS_DIR)Debug
endif
else
ifeq ($(DISABLE_ASSERTIONS),0)
BUILD_LOCATION ?= $(THIS_DIR)Release+Asserts
else
BUILD_LOCATION ?= $(THIS_DIR)Release
endif
endif

OBJ_LOCATION=$(BUILD_LOCATION)/obj
SRC_OBJ_LOCATION=$(OBJ_LOCATION)/src
PRIV_OBJ_LOCATION=$(OBJ_LOCATION)/c/priv
PRIV2_OBJ_LOCATION=$(OBJ_LOCATION)/c/priv2
TEST_OBJ_LOCATION=$(OBJ_LOCATION)/test
DEMO_OBJ_LOCATION=$(OBJ_LOCATION)/c/demo

BIN_LOCATION=$(BUILD_LOCATION)/bin
DEMO_BIN_LOCATION=$(BUILD_LOCATION)/demo

LIB_LOCATION ?= /usr/local/lib
LDFLAGS += $(LIB_LOCATION)/libboost_serialization.a $(LIB_LOCATION)/libboost_iostreams.a $(LIB_LOCATION)/libboost_regex.a

# SOURCES=$(wildcard *.cpp)
# OBJECTS=$(patsubst %.cpp,$(OBJ_LOCATION)/%.o,$(wildcard *.cpp))

SRC_LOCATION ?= $(THIS_DIR)src
SRC_SOURCES=$(wildcard $(SRC_LOCATION)/*.cpp)
SRC_HEADERS=$(wildcard $(SRC_LOCATION)/*.h)
SRC_OBJECTS=$(patsubst %.cpp,$(SRC_OBJ_LOCATION)/%.o,$(notdir $(SRC_SOURCES)))

PRIV_LOCATION ?= $(THIS_DIR)c/priv
PRIV_SOURCES=$(wildcard $(PRIV_LOCATION)/*.c)
PRIV_HEADERS=$(wildcard $(PRIV_LOCATION)/*.h)
PRIV_OBJECTS=$(patsubst %.c,$(PRIV_OBJ_LOCATION)/%.o,$(notdir $(PRIV_SOURCES)))

PRIV2_LOCATION ?= $(THIS_DIR)c/priv2
PRIV2_SOURCES=$(wildcard $(PRIV2_LOCATION)/*.c)
PRIV2_HEADERS=$(wildcard $(PRIV2_LOCATION)/*.h)
PRIV2_OBJECTS=$(patsubst %.c,$(PRIV2_OBJ_LOCATION)/%.o,$(notdir $(PRIV2_SOURCES)))

TEST_LOCATION ?= $(THIS_DIR)test
TEST_SOURCES=$(wildcard $(TEST_LOCATION)/*.cpp)
TEST_HEADERS=$(wildcard $(TEST_LOCATION)/*.h)
TEST_OBJECTS=$(patsubst %.cpp,$(TEST_OBJ_LOCATION)/%.o,$(notdir $(TEST_SOURCES)))

DEMO_LOCATION ?= $(THIS_DIR)c/demo
DEMO_SOURCES=$(wildcard $(DEMO_LOCATION)/*.c)
DEMO_HEADERS=$(wildcard $(DEMO_LOCATION)/*.h)
DEMO_OBJECTS=$(patsubst %.c,$(DEMO_OBJ_LOCATION)/%.o,$(notdir $(DEMO_SOURCES)))

BIN_DIR=$(BIN_LOCATION)
ALL_TEST_EXE=$(patsubst %.cpp,$(BIN_LOCATION)/%.bin,$(notdir $(TEST_SOURCES)))
PRIV2_TEST_EXE=$(BIN_LOCATION)/Priv2ScratchTest.bin $(BIN_LOCATION)/RunPriv2Verification.bin
TEST_EXE=$(filter-out $(PRIV2_TEST_EXE),$(ALL_TEST_EXE))
DEMO_EXE=$(patsubst %.c,$(DEMO_BIN_LOCATION)/%.bin,$(notdir $(DEMO_SOURCES)))
EXECUTABLES=$(ALL_TEST_EXE) $(DEMO_EXE)

DIRECTORIES=$(BUILD_LOCATION) $(OBJ_LOCATION) $(SRC_OBJ_LOCATION) $(PRIV_OBJ_LOCATION) $(PRIV2_OBJ_LOCATION) $(TEST_OBJ_LOCATION) $(DEMO_OBJ_LOCATION) $(BIN_LOCATION) $(DEMO_BIN_LOCATION) $(DATA_LOCATION)

UNAME_S = $(shell uname -s)
DATA_UNAME_S ?= $(UNAME_S)

MY_DATA_BASENAME=$(DATA_UNAME_S)__u_$(UIDS)__p_-1
MY_NORMALIZED_DATA_BASENAME=$(DATA_UNAME_S)__Normalized__u_$(UIDS)__p_-1

MY_TMP_DATA_ARCHIVE=$(THIS_DIR)$(MY_DATA_BASENAME).archive
MY_TMP_NORMALIZED_DATA_ARCHIVE=$(THIS_DIR)$(MY_NORMALIZED_DATA_BASENAME).archive
MY_DATA_ARCHIVE=$(DATA_LOCATION)/$(MY_DATA_BASENAME).archive
MY_NORMALIZED_DATA_ARCHIVE=$(DATA_LOCATION)/$(MY_NORMALIZED_DATA_BASENAME).archive
MY_NORMALIZED_DATA_SRC=$(DATA_LOCATION)/$(MY_NORMALIZED_DATA_BASENAME).c
MY_NORMALIZED_DATA_HDR=$(DATA_LOCATION)/$(MY_NORMALIZED_DATA_BASENAME).h
MY_NORMALIZED_DATA_TARGET_SRC=$(PRIV2_LOCATION)/priv2_generated.c
MY_NORMALIZED_DATA_TARGET_HDR=$(PRIV2_LOCATION)/priv2_generated.h


ifeq ($(UNAME_S),Linux)
CFLAGS += -Wl,-rpath=$(LIB_LOCATION)
LDFLAGS += -Wl,-rpath=$(LIB_LOCATION) -lcap
endif

.PRECIOUS: $(SRC_OBJECTS) $(PRIV_OBJECTS) $(PRIV2_OBJECTS) $(TEST_OBJECTS) $(DEMO_OBJECTS) $(EXECUTABLES)

all: directories exe data norm code
	echo "ALL DONE"

# Create needed directories according to current configuration.
directories:
	mkdir -p $(DIRECTORIES)
	echo "DIRECTORIES CREATED"

# Create all executables (data collection, data analysis, code generation, and
# demo application).
exe: directories $(EXECUTABLES)
	echo "EXECUTABLES BUILT"

# Create executable for demo program that uses alternative-to-setuid interface.
demo_exe: directories $(DEMO_EXE)
	echo "DEMO EXECUTABLES BUILT"

# Create executables that can be used to generate setuid graph data,
# platform-specific code based on data, and analyze graph data according to
# setuid standards.
test_exe: directories $(TEST_EXE)
	echo "TEST EXECUTABLES BUILT"

# Create executables that can be used to generate and analyze priv2 graph data.
priv2_test_exe: directories $(PRIV2_TEST_EXE)
	echo "TEST EXECUTABLES BUILT"

# Generate code for $(DATA_UNAME_S) kernel. Uses $(MY_DATA_ARCHIVE) to
# generate platform-specific source and headers for "change identity"
# interface a.k.a. "priv2"
code: directories $(MY_NORMALIZED_DATA_TARGET_SRC) $(MY_NORMALIZED_DATA_TARGET_HDR)
	echo "GEN COMPLETE"

# Normalize code UIDs in archive generated with "make data" or similar.
norm: directories $(MY_NORMALIZED_DATA_ARCHIVE)
	echo "NORM COMPLETE"

# Alias for build of $(MY_DATA_ARCHIVE) Generate setuid graph data for
# current kernel. Follow up with "make code" to generate platform-specific
# code, or copy $(MY_DATA_ARCHIVE) to another machine and "make code" with
# overriding $(DATA_UNAME_S) value to point to correct archive.
data: $(MY_NORMALIZED_DATA_ARCHIVE)
	echo "DATA COMPLETE"

$(MY_NORMALIZED_DATA_TARGET_SRC): $(MY_NORMALIZED_DATA_SRC)
	if [ $(SKIP_CODE_GENERATION) = 0 ]; then cp $(MY_NORMALIZED_DATA_SRC) $(MY_NORMALIZED_DATA_TARGET_SRC); fi

$(MY_NORMALIZED_DATA_TARGET_HDR): $(MY_NORMALIZED_DATA_HDR)
	if [ $(SKIP_CODE_GENERATION) = 0 ]; then cp $(MY_NORMALIZED_DATA_HDR) $(MY_NORMALIZED_DATA_TARGET_HDR); fi

$(MY_NORMALIZED_DATA_SRC): $(MY_NORMALIZED_DATA_ARCHIVE)
	if [ $(SKIP_CODE_GENERATION) = 0 ]; then $(BIN_LOCATION)/GenerateCode.bin $(DATA_LOCATION)/$(MY_NORMALIZED_DATA_BASENAME); fi

$(MY_NORMALIZED_DATA_HDR): $(MY_NORMALIZED_DATA_ARCHIVE)
	if [ $(SKIP_CODE_GENERATION) = 0 ]; then $(BIN_LOCATION)/GenerateCode.bin $(DATA_LOCATION)/$(MY_NORMALIZED_DATA_BASENAME); fi

$(MY_NORMALIZED_DATA_ARCHIVE): $(MY_TMP_NORMALIZED_DATA_ARCHIVE)
	if [ $(SKIP_DATA_COLLECTION) = 0 ]; then cp $(MY_TMP_NORMALIZED_DATA_ARCHIVE) $(MY_NORMALIZED_DATA_ARCHIVE); fi

$(MY_TMP_NORMALIZED_DATA_ARCHIVE): $(MY_TMP_DATA_ARCHIVE) $(MY_DATA_ARCHIVE) $(BIN_LOCATION)/NormalizeGraph.bin
	if [ $(SKIP_DATA_COLLECTION) = 0 ]; then echo $(UIDS) | sed 's/_/\ /g' | xargs $(BIN_LOCATION)/NormalizeGraph.bin $(DATA_UNAME_S); fi

$(MY_DATA_ARCHIVE): $(MY_TMP_DATA_ARCHIVE)
	if [ $(SKIP_DATA_COLLECTION) = 0 ]; then cp $(MY_TMP_DATA_ARCHIVE) $(MY_DATA_ARCHIVE); fi

$(MY_TMP_DATA_ARCHIVE): $(BIN_LOCATION)/CollectSetuidData.bin
	if [  $(SKIP_DATA_COLLECTION) = 0 ]; then echo $(UIDS) | sed 's/_/\ /g' | xargs sudo $(BIN_LOCATION)/CollectSetuidData.bin $(UNAME_S); fi

$(SRC_OBJ_LOCATION)/%.o: $(SRC_LOCATION)/%.cpp $(SRC_HEADERS)
	$(CPP) -c $(CFLAGS) $< -o $@

$(PRIV_OBJ_LOCATION)/%.o: $(PRIV_LOCATION)/%.c $(PRIV_HEADERS)
	$(CC) -std=c99 -c $(CFLAGS) $(PRIV_FLAGS) $< -o $@

$(PRIV2_OBJ_LOCATION)/priv2_generated.o: $(PRIV2_LOCATION)/priv2_generated.c $(PRIV2_HEADERS)
	$(CC) -std=c99 -c $(CFLAGS) $(PRIV2_FLAGS) $< -o $@

$(PRIV2_OBJ_LOCATION)/%.o: $(PRIV2_LOCATION)/%.c $(PRIV2_HEADERS)
	$(CC) -std=c99 -c $(CFLAGS) $(PRIV2_FLAGS) $< -o $@

$(TEST_OBJ_LOCATION)/%.o: $(TEST_LOCATION)/%.cpp $(TEST_HEADERS) $(SRC_HEADERS)
	$(CPP) -c $(CFLAGS) $< -o $@

$(DEMO_OBJ_LOCATION)/%.o: $(DEMO_LOCATION)/%.c $(PRIV_HEADERS) $(PRIV2_HEADERS)
	$(CC) -std=c99 -c $(CFLAGS) $(PRIV2_FLAGS) $< -o $@

$(BIN_LOCATION)/Priv2ScratchTest.bin: $(TEST_OBJ_LOCATION)/Priv2ScratchTest.o $(SRC_OBJECTS) $(PRIV_OBJECTS) $(PRIV2_OBJECTS)
	$(CPP) $^ -o $@ $(LDFLAGS)

$(BIN_LOCATION)/RunPriv2Verification.bin: $(TEST_OBJ_LOCATION)/RunPriv2Verification.o $(SRC_OBJECTS) $(PRIV_OBJECTS) $(PRIV2_OBJECTS)
	$(CPP) $^ -o $@ $(LDFLAGS)

$(BIN_LOCATION)/%.bin: $(TEST_OBJ_LOCATION)/%.o $(SRC_OBJECTS) $(PRIV_OBJECTS)
	$(CPP) $^ -o $@ $(LDFLAGS)

$(DEMO_BIN_LOCATION)/%.bin: $(DEMO_OBJ_LOCATION)/%.o $(PRIV_OBJECTS) $(PRIV2_OBJECTS)
	$(CPP) $^ -o $@ $(LDFLAGS)

clean:
	rm -rf $(THIS_DIR)Debug $(THIS_DIR)Debug+Asserts $(THIS_DIR)Release $(THIS_DIR)Release+Asserts a.out core

dataclean:
	rm -rf *.archive *.dot
