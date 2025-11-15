# See LICENSE.txt for license details.

CC ?= cc
CXX ?= g++

CXX_FLAGS += -std=c++11 -O3 -Wall
CC_FLAGS += -std=c11 -O3 -Wall -Isrcc
LDLIBS += -lm

KERNELS = bc bfs cc cc_sv pr pr_spmv sssp tc
SUITE = $(KERNELS) converter

SRCC_DIR = srcc
SRC_DIR = src

C_TARGETS := $(foreach bin,$(SUITE),$(if $(wildcard $(SRCC_DIR)/$(bin).c),$(bin),))
CPP_TARGETS := $(filter-out $(C_TARGETS),$(SUITE))

.PHONY: all
all: $(SUITE)

$(C_TARGETS): % : $(SRCC_DIR)/%.c $(SRCC_DIR)/*.h
	$(CC) $(CC_FLAGS) $< -o $@ $(LDLIBS)

# $(CPP_TARGETS): % : $(SRC_DIR)/%.cc $(SRC_DIR)/*.h
# 	$(CXX) $(CXX_FLAGS) $< -o $@ $(LDLIBS)

# Testing
# include test/test.mk

# Benchmark Automation
# include benchmark/bench.mk


.PHONY: clean
clean:
	rm -f $(SUITE) test/out/*
