HDRS=$(shell find . -name "*.h")
SRCS=$(shell find . -name "*.c")

all : 
	clang-format -i $(HDRS) $(SRCS)