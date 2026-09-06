# compiler ------------------------------------------------------------------@/
CXX	:= clang++
CC	:= clang
AR	:= ar

CFLAGS := -Wall -Wshadow -Iinclude --std=c++23
CFLAGS += --write-user-dependencies -MP
CFLAGS += -O2
CFLAGS += -g

# output --------------------------------------------------------------------@/
OBJ_DIR := build
SRC_DIR := source
OUTPUT  := bin/libscl.a

SRCS_C   := $(shell find $(SRC_DIR) -name *.c)
SRCS_CPP := $(shell find $(SRC_DIR) -name *.cpp)

OBJS := $(subst $(SRC_DIR),$(OBJ_DIR),$(SRCS_C:.c=.o))
OBJS += $(subst $(SRC_DIR),$(OBJ_DIR),$(SRCS_CPP:.cpp=.o))
DEPS := $(OBJS:.o=.d)

-include $(DEPS)

# building ------------------------------------------------------------------@/
all: $(OUTPUT)

$(OUTPUT): $(OBJS)
	$(AR) rcs $@ $^

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	ccache $(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	ccache $(CXX) $(CFLAGS) -c $< -o $@

soft_clean:
	rm -rf $(OUTPUT)

clean:
	rm -rf $(OBJS) $(DEPS) $(OUTPUT)

rebuild: clean .WAIT all
soft_rebuild: soft_clean .WAIT all
