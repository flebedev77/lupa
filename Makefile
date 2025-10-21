TARGET=liq

SRCS=src/main.c lib/microui.c
SHADERS=res/shaders/background.fs res/shaders/background.vs res/shaders/glass.fs res/shaders/glass.vs
FONTS=res/NotoSerif-Regular.ttf

all: fonts shaders $(TARGET) run

shaders: $(SHADERS)
	xxd -i res/shaders/background.fs > res/built/shaders.h
	xxd -i res/shaders/background.vs >> res/built/shaders.h
	xxd -i res/shaders/glass.fs >> res/built/shaders.h
	xxd -i res/shaders/glass.vs >> res/built/shaders.h
	xxd -i res/shaders/ui/rect.fs >> res/built/shaders.h
	xxd -i res/shaders/ui/rect.vs >> res/built/shaders.h
	xxd -i res/shaders/ui/font.fs >> res/built/shaders.h
	xxd -i res/shaders/ui/font.vs >> res/built/shaders.h

fonts: $(FONTS)
	./scripts/ttf2c.c res/NotoSerif-Regular.ttf > res/built/fonts.h
	# ./scripts/ttf2c.c /usr/share/fonts/TTF/JetBrainsMono-Regular.ttf > res/built/fonts.h

$(TARGET): $(SRCS)
	# gcc -o $(TARGET) $(SRCS) -lglfw -lGLEW -lEGL -lGL -lGLU -lOpenGL -lX11 -lm -g
	gcc -o $(TARGET) $(SRCS) -lglfw \
		-lGLEW \
		-lOpenGL \
		-lX11 \
		-lm \
		-g
		# -g \
		# -DRELEASE

run: $(TARGET)
	./$(TARGET)

.PHONY: clean

clean:
	rm -v res/build/*
