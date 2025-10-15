TARGET=liq

SRCS=src/main.c
SHADERS=res/shaders/background.fs res/shaders/background.vs res/shaders/glass.fs res/shaders/glass.vs

all: shaders $(TARGET) run

shaders: $(SHADERS)
	xxd -i res/shaders/background.fs > res/built/shaders.h
	xxd -i res/shaders/background.vs >> res/built/shaders.h
	xxd -i res/shaders/glass.fs >> res/built/shaders.h
	xxd -i res/shaders/glass.vs >> res/built/shaders.h

$(TARGET): $(SRCS)
	# gcc -o $(TARGET) $(SRCS) -lglfw -lGLEW -lEGL -lGL -lGLU -lOpenGL -lX11 -lm -g
	gcc -o $(TARGET) $(SRCS) -lglfw \
		-lGLEW \
		-lOpenGL \
		-lX11 \
		-lm \
		-g \
		-DRELEASE

run: $(TARGET)
	./$(TARGET)

.PHONY: clean

clean:
	rm -v res/build/*
