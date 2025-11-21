CC = g++

COMPILE_DEBUG_FLAGS = -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wnon-virtual-dtor -Wnull-dereference -Wdouble-promotion -Wformat=2 -pedantic -ggdb -O0

COMPILE_INSTALL_FLAGS = -O3

SRC_FILES = src/*.cpp

LINKERS = -lX11

OBJ_NAME = jxwm

build:
	$(CC) $(COMPILE_DEBUG_FLAGS) $(SRC_FILES) $(LINKERS) -o $(OBJ_NAME)

run:
	./run.sh

install:
	$(CC) $(COMPILE_INSTALL_FLAGS) $(SRC_FILES) $(LINKERS) -o $(OBJ_NAME) && sudo cp $(OBJ_NAME) /usr/bin

debug:
	gdb $(OBJ_NAME)

clean:
	rm $(OBJ_NAME)

