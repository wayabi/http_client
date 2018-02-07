COMPILER  = g++
CFLAGS    = -g -MMD -MP -Wall -Wextra -Winit-self -Wno-missing-field-initializers -std=c++0x -Wno-unused-function -DBOOST_LOG_DYN_LINK
ifeq "$(shell getconf LONG_BIT)" "64"
  LDFLAGS = 
else
  LDFLAGS =
endif
LIBS      = -lboost_system -lboost_thread -lboost_math_c99 -lboost_program_options -lboost_log -lboost_log_setup -lpthread -lcurl
INCLUDE   = -I./include
TARGET    = ./a.out
SRCDIR    = ./src
ifeq "$(strip $(SRCDIR))" ""
  SRCDIR  = .
endif
SOURCES   = $(wildcard $(SRCDIR)/*.cpp)
OBJDIR    = ./obj
ifeq "$(strip $(OBJDIR))" ""
  OBJDIR  = .
endif
OBJECTS   = $(addprefix $(OBJDIR)/, $(notdir $(SOURCES:.cpp=.o)))
DEPENDS   = $(OBJECTS:.o=.d)

$(TARGET): $(OBJECTS)
	$(COMPILER) -o $@ $^ $(LDFLAGS) $(LIBS)

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	-mkdir -p $(OBJDIR)
	$(COMPILER) $(CFLAGS) $(INCLUDE) -o $@ -c $<

TEST_TARGET=./gtest
LIB_DIR=./lib
TEST_DIR=./test
TEST_SRC= $(wildcard $(TEST_DIR)/*.cpp)
TEST_OBJS  = $(addprefix $(OBJDIR)/, $(notdir $(TEST_SRC:.cpp=.o)))
GTEST_DIR=../googletest/googletest
GTEST_INC=$(GTEST_DIR)/include
GTEST_HEADERS = $(GTEST_DIR)/include/gtest/*.h \
                $(GTEST_DIR)/include/gtest/internal/*.h
GTEST_LIBS = $(GTEST_DIR)/lib/libgtest.a $(GTEST_DIR)/lib/libgtest_main.a

$(OBJDIR)/gtest-all.o : $(TEST_SRC)
	$(COMPILER) $(CFLAGS) -I$(GTEST_INC) -I$(GTEST_DIR) $(CFLAGS) -c \
	-o $@ $(GTEST_DIR)/src/gtest-all.cc
$(OBJDIR)/gtest_main.o : $(GTEST_SRCS_)
	$(COMPILER) $(CFLAGS) -I$(GTEST_INC) -I$(GTEST_DIR) $(CFLAGS) -c \
	-o $@ $(GTEST_DIR)/src/gtest_main.cc
$(LIB_DIR)/gtest.a : $(OBJDIR)/gtest-all.o
	$(AR) $(ARFLAGS) $@ $^
$(LIB_DIR)/gtest_main.a : $(OBJDIR)/gtest-all.o $(OBJDIR)/gtest_main.o
	@[ -d $(LIB_DIR) ] || mkdir -p $(LIB_DIR)
	$(AR) $(ARFLAGS) $@ $^

.PHONY: test
test: $(TEST_TARGET)
$(TEST_TARGET) : $(TARGET) $(TEST_OBJS) $(LIB_DIR)/gtest_main.a
	$(COMPILER) $(LDFLAGS) -o $@ $(TEST_OBJS) \
	$(LIB_DIR)/gtest_main.a $(LIBS) 

$(OBJDIR)/%.o: $(TEST_DIR)/%.cpp $(GTEST_HEADERS)
	@[ -d $(OBJDIR) ] || mkdir -p $(OBJDIR)
	$(COMPILER) $(CFLAGS) -I$(GTEST_INC) -I$(SRCDIR) -I$(INCLUDE) -o $@ -c $<

all: clean $(TARGET)

clean:
	-rm -f $(OBJECTS) $(DEPENDS) $(TARGET)

-include $(DEPENDS)
#	@[ -d $(BIN_DIR) ] || mkdir -p $(BIN_DIR)
