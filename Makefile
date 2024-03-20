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

d: all
	gdb $(TARGET)
