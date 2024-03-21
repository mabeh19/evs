OBJECTS=evs.o main.o
FLAGS=-Wall -g3
TARGET=evs

phony: all

%.o: %.c
	gcc $(FLAGS) -c $< -o $@

$(TARGET): $(OBJECTS)
	gcc -fsanitize=address $^ -o $@

all: $(TARGET)

run: all
	./$(TARGET)

r: run

d: all
	gdb $(TARGET)
