TARGET=liq

SRCS=src/main.c src/mathc.c

all: $(TARGET) run

$(TARGET): $(SRCS)
	gcc -o $(TARGET) $(SRCS) -lglfw -lGLEW -lEGL -lGL -lGLU -lOpenGL -lX11 -lm -g

run: $(TARGET)
	./$(TARGET)
