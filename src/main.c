#define STBI_NO_JPEG
#define STBI_NO_BMP
#define STBI_NO_PSD
#define STBI_NO_TGA
#define STBI_NO_GIF
#define STBI_NO_HDR
#define STBI_NO_PIC
#define STBI_NO_PNM
#define STB_IMAGE_IMPLEMENTATION

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

#include "../lib/stb_image.h"
#include "../lib/microui.h"

#include "../res/built/shaders.h"
#include "../res/built/fonts.h"

#define WINDOW_WIDTH  1920
#define WINDOW_HEIGHT 1080
#define OPENGL_FAIL 9999

#define MAX_ZOOM 7.6

#define RECTS_AMOUNT 255
#define GLYPHS_AMOUNT 255

static Display* display = NULL;
static Screen* screen = NULL;
static Window current_window = {0};
static Window root_window = {0};

static int screen_width = WINDOW_WIDTH;
static int screen_height = WINDOW_HEIGHT;

static float scroll_amount = 1.0f;
static float scroll_sensitivity = 0.1f;

static float scroll_size_amount = 1.0f;
static float scroll_size_sensitivity = 2.f;

static float pos[] = {0.1f, 0.8f};
static float backgroundPos[] = {0.0f, 0.0f};
static float scalePivot[] = {0.0f, 0.0f};

static float unnormal_scale[] = {300.f, 300.f};
static float scale[] = {
    (300.f / WINDOW_WIDTH),
    (300.f / WINDOW_HEIGHT)
  };

static float vertices[] = {
    //   pos       tex
     1.f, -1.f,  1.f, 1.f, // Bottom right
     1.f,  1.f,  1.f, 0.f, // Top right
    -1.f,  1.f,  0.f, 0.f, // Top left
    -1.f, -1.f,  0.f, 1.f, // Bottom left
  };

static unsigned int indices[] = {
    1, 2, 3,
    0, 1, 3
  };

static int lens_mode = 1;

static int current_rectangle = 0;
static int current_glyph = 0;

struct ui_shader {
  unsigned int program,
               position_location,
               scale_location,
               color_location;
};

struct rectangle {
  unsigned int VAO, VBO, EBO;
  struct ui_shader shader;
  float position[2];
  float scale[2];
  float color[4];
};
struct rectangle ui_rectangles[RECTS_AMOUNT] = {0};
struct rectangle ui_glyphs[GLYPHS_AMOUNT] = {0};

static int text_size = 16;
static int text_width(mu_Font font, const char* text, int len) {
  return text_size;
}

static int text_height(mu_Font font) {
  return text_size;
}

bool string_includes(const char* restrict buf, const char* restrict sub) {
  for (size_t i = 0; buf[i] != 0; i++) {
    if (buf[i] == sub[0]) {
      bool match = true;
      for (size_t j = 0; buf[i + j] != 0 && sub[j] != 0; j++) {
        if (buf[i + j] != sub[j]) {
          match = false; 
          break;
        }
      }
      if (match) return true;
    }
  }
  return false;
}
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_C &&  action == GLFW_RELEASE) {
      if (lens_mode == -1) lens_mode = 1;
      else lens_mode = -1;
    }
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
  if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
    scroll_size_amount += (float)yoffset * scroll_size_sensitivity;
    unnormal_scale[0] += scroll_size_amount;
    unnormal_scale[1] += scroll_size_amount;

    if (unnormal_scale[0] < 100 ||
        unnormal_scale[1] < 100) {
      unnormal_scale[0] = 100;
      unnormal_scale[1] = 100;
    }
    scale[0] = unnormal_scale[0] / WINDOW_WIDTH;
    scale[1] = unnormal_scale[1] / WINDOW_HEIGHT;

    scroll_size_amount = 0;
  } else {
    scroll_amount += (float)yoffset * scroll_sensitivity;
    if (scroll_amount < 0.f) scroll_amount = 0.f;
    // if (scroll_amount > MAX_ZOOM) scroll_amount = MAX_ZOOM;
  }

  // backgroundPos[0] -= pos[0];
  // backgroundPos[1] -= pos[1];

  scalePivot[0] += pos[0] - scalePivot[0];
  scalePivot[1] += pos[1] - scalePivot[1];
}

struct FileData {
  uint8_t* data;
  size_t len;
};

struct FileData readFile(const char* filepath) {
  FILE* fd = fopen(filepath, "r");
  if (fd == NULL) {
    perror("Unable to open fd");
    return (struct FileData){0};
  }
   
	fseek(fd, 0, SEEK_END);
	size_t fdSize = ftell(fd);
	fseek(fd, 0, SEEK_SET);

	uint8_t* fdData = malloc(fdSize + 1);
	if (fdData == NULL)
	{
		perror("Unable to allocate byte array for file");
		fclose(fd);
    return (struct FileData){0};
	}

	size_t bytesRead = fread(fdData, 1, fdSize, fd);
	if ((long)bytesRead != fdSize)
	{
		perror("Error reading from file");
		free(fdData);
		fclose(fd);
    return (struct FileData){0};
	}

  fclose(fd);
  return (struct FileData){.data = fdData, .len = fdSize};
}

unsigned int createShader(const char* source, unsigned int shaderType) {
  unsigned int shader = glCreateShader(shaderType);
  glShaderSource(shader, 1, &source, NULL);
  glCompileShader(shader);

  int success;
  char infoLog[512];
  glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
  if (!success)
  {
    glGetShaderInfoLog(shader, 512, NULL, infoLog);
    fprintf(stderr, "ERROR compiling %s shader\n",
      ((shaderType == GL_VERTEX_SHADER) ? "vertex" : "fragment"));
    fprintf(stderr, "%s\n", infoLog);
  }

  return shader;
}

unsigned int createShaderProgram(const char* vs, const char* fs) {
  struct FileData vertexSource = {0};
  struct FileData fragmentSource = {0};
#ifdef RELEASE
  if (string_includes(vs, "background.vs")) {
    vertexSource.data = res_shaders_background_vs;
    vertexSource.len = res_shaders_background_vs_len;
    fragmentSource.data = res_shaders_background_fs;
    fragmentSource.len = res_shaders_background_fs_len;
  }
  if (string_includes(vs, "glass.vs")) {
    vertexSource.data = res_shaders_glass_vs;
    vertexSource.len = res_shaders_glass_vs_len;
    fragmentSource.data = res_shaders_glass_fs;
    fragmentSource.len = res_shaders_glass_fs_len;
  }
  if (string_includes(vs, "rect.vs")) {
    vertexSource.data = res_shaders_ui_rect_vs;
    vertexSource.len = res_shaders_ui_rect_vs_len;
    fragmentSource.data = res_shaders_ui_rect_fs;
    fragmentSource.len = res_shaders_ui_rect_fs_len;
  }
  if (string_includes(vs, "font.vs")) {
    vertexSource.data = res_shaders_ui_font_vs;
    vertexSource.len = res_shaders_ui_font_vs_len;
    fragmentSource.data = res_shaders_ui_font_fs;
    fragmentSource.len = res_shaders_ui_font_fs_len;
  }
#else
  vertexSource = readFile(vs);
  fragmentSource = readFile(fs);
#endif

  if (vertexSource.len == 0 || fragmentSource.len == 0) return OPENGL_FAIL;

  vertexSource.data[vertexSource.len-1] = 0;
  fragmentSource.data[fragmentSource.len-1] = 0;

  unsigned int vertexShader = createShader((char*)vertexSource.data, GL_VERTEX_SHADER);
  unsigned int fragmentShader = createShader((char*)fragmentSource.data, GL_FRAGMENT_SHADER);

  unsigned int shader = glCreateProgram();
  glAttachShader(shader, vertexShader);
  glAttachShader(shader, fragmentShader);
  glLinkProgram(shader);
  glDeleteShader(vertexShader);
  glDeleteShader(fragmentShader);
  glUseProgram(shader);
  return shader;
}

void x_init() {
  display = XOpenDisplay(NULL);
  screen = DefaultScreenOfDisplay(display);

  screen_width = DisplayWidth(display, DefaultScreen(display));
  screen_height = DisplayHeight(display, DefaultScreen(display));

  root_window = DefaultRootWindow(display);
}

unsigned char* screenshot() {
  XImage *image = XGetImage(display, RootWindow(display, DefaultScreen(display)), 
                               0, 0, screen_width, screen_height, AllPlanes, ZPixmap);

  if (!image) {
    fprintf(stderr, "Failed to capture screen image\n");
    return NULL;
  }

  unsigned char *buffer = (unsigned char *)malloc(screen_width * screen_height * 3); // RGB
  if (!buffer) {
    fprintf(stderr, "Failed to allocate memory for the pixel buffer\n");
    XDestroyImage(image);
    return NULL;
  }

  for (int y = 0; y < screen_height; ++y) {
    for (int x = 0; x < screen_width; ++x) {
      long pixel = XGetPixel(image, x, y);
      buffer[(y * screen_width + x) * 3 + 0] = (pixel & image->red_mask) >> 16; // R
      buffer[(y * screen_width + x) * 3 + 1] = (pixel & image->green_mask) >> 8; // G
      buffer[(y * screen_width + x) * 3 + 2] = (pixel & image->blue_mask); // B
    }
  }

  XDestroyImage(image);
  return buffer;
}

void clampf(float* restrict value, float min, float max) {
  if (*value < min) *value = min;
  if (*value > max) *value = max;
}

void init_rectangle(struct rectangle* restrict rect) {
  if (rect->scale[0] == 0 || rect->scale[1] == 0) {
    rect->scale[0] = 50;
    rect->scale[1] = 50;
    printf("Provide non 0 width and height\n");
  }
  // clampf(&rect->position[0], 0.0f, WINDOW_WIDTH);
  // clampf(&rect->position[1], 0.0f, WINDOW_HEIGHT);

  glGenVertexArrays(1, &rect->VAO);
  glBindVertexArray(rect->VAO);

  glGenBuffers(1, &rect->VBO);
  glBindBuffer(GL_ARRAY_BUFFER, rect->VBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

  glGenBuffers(1, &rect->EBO);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, rect->EBO);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);

  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
}

void draw_rectangle(struct rectangle* restrict rect) {
  glUseProgram(rect->shader.program);
  glUniform2f(rect->shader.position_location,
      (((float)rect->position[0] - WINDOW_WIDTH / 2.f) / WINDOW_WIDTH) * 2,
      ((WINDOW_HEIGHT / 2.f - (float)rect->position[1]) / WINDOW_HEIGHT) * 2
      );
  glUniform2f(rect->shader.scale_location,
      rect->scale[0] / WINDOW_WIDTH,
      rect->scale[1] / WINDOW_HEIGHT
      );
  glUniform4fv(rect->shader.color_location, 1, rect->color);
  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}

void init_shader(struct ui_shader* restrict shader) {
  shader->position_location = glGetUniformLocation(shader->program, "position"); 
  shader->scale_location = glGetUniformLocation(shader->program, "scale"); 
  shader->color_location = glGetUniformLocation(shader->program, "color"); 
}

struct rectangle* add_glyph(float x, float y, float w, float h) {
  current_glyph++;
  if (current_glyph > GLYPHS_AMOUNT) {
    return NULL;
  }
  ui_glyphs[current_glyph].position[0] = x;
  ui_glyphs[current_glyph].position[1] = y;
  ui_glyphs[current_glyph].scale[0] = w;
  ui_glyphs[current_glyph].scale[1] = h;
  return &(ui_glyphs[current_glyph]);
}

struct rectangle* add_rectangle(float x, float y, float w, float h) {
  current_rectangle++;
  if (current_rectangle > RECTS_AMOUNT) {
    return NULL;
  }
  ui_rectangles[current_rectangle].position[0] = x;
  ui_rectangles[current_rectangle].position[1] = y;
  ui_rectangles[current_rectangle].scale[0] = w;
  ui_rectangles[current_rectangle].scale[1] = h;
  return &(ui_rectangles[current_rectangle]);
}

int main() {
  int image_width, image_height, image_nrChannels;

  // sleep(4);
  x_init();
  unsigned char* img = screenshot();
  
  // unsigned char* image_textureData = stbi_load(
  //   "res/image.png",
  //   &image_width,
  //   &image_height,
  //   &image_nrChannels,
  //   0
  // );

  printf("Image.png: %d %d %d\nFirst pixel rgb values: ", image_width, image_height, image_nrChannels);
  for (size_t i = 0; i < 10; i++) {
    printf("%02X ", img[i]);
  }
  printf("\n\n");

  GLFWwindow* window;

  if (!glfwInit())
    return EXIT_FAILURE;

  glfwWindowHint(GLFW_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_RESIZABLE, 1);
  glfwWindowHint(GLFW_DOUBLEBUFFER, true);
  glfwWindowHint(GLFW_DECORATED, false);
  window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Zoom", glfwGetPrimaryMonitor(), NULL);
  glfwSwapInterval(1);
  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
  glfwSetScrollCallback(window, scroll_callback);
  glfwSetKeyCallback(window, key_callback);
  if (!window) {
    glfwTerminate();
    return EXIT_FAILURE;
  }

  Window x11Window = glfwGetX11Window(window);
  XRaiseWindow(display, x11Window);
  Atom atom = XInternAtom(display, "_NET_WM_STATE", True);
  Atom state = XInternAtom(display, "_NET_WM_STATE_ABOVE", True);
  XChangeProperty(display, x11Window, atom, XA_ATOM, 32, PropModeReplace, (unsigned char*)&state, 1);

  glfwMakeContextCurrent(window);
  glewInit();
  glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  unsigned int background_shader = createShaderProgram("res/shaders/background.vs", "res/shaders/background.fs");
  unsigned int glass_shader = createShaderProgram("res/shaders/glass.vs", "res/shaders/glass.fs");
  unsigned int ui_shader_program = createShaderProgram("res/shaders/ui/rect.vs", "res/shaders/ui/rect.fs");
  unsigned int font_shader_program = createShaderProgram("res/shaders/ui/font.vs", "res/shaders/ui/font.fs");

  unsigned int glass_VAO, glass_VBO, glass_EBO;

  glGenVertexArrays(1, &glass_VAO);
  glBindVertexArray(glass_VAO);

  glGenBuffers(1, &glass_VBO);
  glBindBuffer(GL_ARRAY_BUFFER, glass_VBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

  glGenBuffers(1, &glass_EBO);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, glass_EBO);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
  
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

  glUseProgram(glass_shader);
  unsigned int glass_shader_position_location = glGetUniformLocation(glass_shader, "pos");
  unsigned int glass_shader_scale_location = glGetUniformLocation(glass_shader, "scale");
  unsigned int glass_shader_borderColor = glGetUniformLocation(glass_shader, "borderColor");
  unsigned int glass_shader_zoom = glGetUniformLocation(glass_shader, "zoom");
  unsigned int glass_shader_screen_size = glGetUniformLocation(glass_shader, "screenSize");
  unsigned int glass_shader_aspect_ratio = glGetUniformLocation(glass_shader, "aspectRatio");
  unsigned int glass_shader_lens_mode = glGetUniformLocation(glass_shader, "lensMode");
  unsigned int background_shader_zoom = glGetUniformLocation(background_shader, "zoom");
  unsigned int background_shader_mousepos = glGetUniformLocation(background_shader, "mousepos");
  unsigned int background_shader_scalePivot = glGetUniformLocation(background_shader, "scalePivot");


  glUniform2f(glass_shader_position_location, pos[0], pos[1]);
  glUniform2f(glass_shader_scale_location, scale[0], scale[1]);

  float color[] = {0.1, 0.3, 0.8};

  glUniform3fv(glass_shader_borderColor, 1, color);
  // glUniform3f(glass_shader_borderColor, color[0], color[1], color[2]);
  printf("Shader index %d\n", glass_shader);
  printf("Shader position location %d\n", glass_shader_position_location);

  unsigned int VAO, VBO, EBO, texture;

  glUseProgram(background_shader);
  glGenVertexArrays(1, &VAO);
  glBindVertexArray(VAO);

  glGenBuffers(1, &VBO);
  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

  glGenBuffers(1, &EBO);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);

  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

  unsigned int glyph_atlas_texture;
  glGenTextures(1, &glyph_atlas_texture);
  glBindTexture(GL_TEXTURE_2D, glyph_atlas_texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);	
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  // glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, screen_width, screen_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, img);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, font_width, font_height, 0, GL_RED, GL_UNSIGNED_BYTE, font_data);
  glGenerateMipmap(GL_TEXTURE_2D);
  // stbi_image_free(image_textureData);

  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);	
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, screen_width, screen_height, 0, GL_RGB, GL_UNSIGNED_BYTE, img);
  // glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image_width, image_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image_textureData);
  glGenerateMipmap(GL_TEXTURE_2D);

  glBindVertexArray(VAO);

  bool is_debug = false;
  double prevTime = 0, time = 0;
  double timer = 0;

  struct ui_shader uishader = {0};
  uishader.program = ui_shader_program;
  init_shader(&uishader);

  struct ui_shader fontshader = {0};
  fontshader.program = font_shader_program;
  init_shader(&fontshader);

  for (int i = 0; i < GLYPHS_AMOUNT; i++) {
    struct rectangle* glyph = &ui_glyphs[i];
    glyph->shader = fontshader;
    glyph->position[0] = 99999.f;
    glyph->position[1] = 99999.f;
    glyph->scale[0] = 160.f;
    glyph->scale[1] = 100.f;
    glyph->color[0] = 0.f;
    glyph->color[1] = 0.f;
    glyph->color[2] = 0.f;
    glyph->color[3] = 1.f;
    init_rectangle(glyph);
  }

  for (int i = 0; i < RECTS_AMOUNT; i++) {
    struct rectangle* ui_rect = &ui_rectangles[i];
    ui_rect->shader = uishader;
    ui_rect->position[0] = 99999.f;
    ui_rect->position[1] = 99999.f;
    ui_rect->scale[0] = 100.f;
    ui_rect->scale[1] = 160.f;
    ui_rect->color[0] = 0.f;
    ui_rect->color[1] = 0.f;
    ui_rect->color[2] = 0.f;
    ui_rect->color[3] = 1.f;
    init_rectangle(ui_rect);
  }

  printf("Screen W: %d H: %d\n", screen_width, screen_height);

  mu_Context ctx = {0};
  mu_init(&ctx);

  ctx.text_width = text_width;
  ctx.text_height = text_height;

  while (!glfwWindowShouldClose(window))
  {
    prevTime = time;
    time = glfwGetTime();
    double mx, my;
    glfwGetCursorPos(window, &mx, &my);
    pos[0] = (((float)mx - WINDOW_WIDTH / 2.f) / WINDOW_WIDTH) * 2;
    pos[1] = ((WINDOW_HEIGHT / 2.f - (float)my) / WINDOW_HEIGHT) * 2;


    glClearColor(0.0f, 1.0f, 1.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glBindTexture(GL_TEXTURE_2D, texture);
    glBindVertexArray(VAO);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glUseProgram(background_shader);
    // glUniform1f(background_shader_zoom, scroll_amount + 1);
    glUniform2fv(background_shader_mousepos, 1, backgroundPos);
    glUniform2fv(background_shader_scalePivot, 1, scalePivot);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);



    float colorpos[] = {(pos[0] + 1) * 0.5, (pos[1] + 1) * 0.5};
    color[0] = colorpos[0];
    color[1] = colorpos[1];
    color[2] = colorpos[1] + colorpos[0] * 0.9;

    int screensize[] = {screen_width, screen_height};
    float aspect = (float)screensize[0] / (float)screensize[1];

    // printf("X: %02f, Y: %02f\n", pos[0], pos[1]);
    glBindVertexArray(glass_VAO);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glUseProgram(glass_shader);
    glUniform2fv(glass_shader_position_location, 1, pos);
    glUniform2iv(glass_shader_screen_size, 1, screensize);
    glUniform1f(glass_shader_aspect_ratio, aspect);
    glUniform3fv(glass_shader_borderColor, 1, color);
    glUniform1f(glass_shader_zoom, scroll_amount);
    glUniform2f(glass_shader_scale_location, scale[0], scale[1]);
    glUniform1i(glass_shader_lens_mode, lens_mode);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    // struct rectangle* rect = add_rectangle(x, y, 40, 40);

    add_glyph(mx, my, font_width, font_height);

    if (glfwGetKey(window, GLFW_KEY_PERIOD) == GLFW_PRESS && timer > 0.2) {
      timer = 0;
      is_debug = !is_debug;
      glPolygonMode(GL_FRONT_AND_BACK, is_debug ? GL_LINE : GL_FILL); 
    }

    mu_input_mousemove(&ctx, mx, my);
    mu_begin(&ctx);

    if (mu_begin_window(&ctx, "Hello sailor", mu_rect(0, 0, 300, 400))) {
      if (mu_button(&ctx, "Click me")) {
        printf("Button clicked\n");
      }

      mu_end_window(&ctx);
    }
    mu_end(&ctx);

    mu_Command* cmd = NULL;
    while (mu_next_command(&ctx, &cmd)) {
      if (cmd->type == MU_COMMAND_TEXT) {
      }
      if (cmd->type == MU_COMMAND_RECT) {
        float rw = (float)cmd->rect.rect.w;
        float rh = (float)cmd->rect.rect.h;
        float rx = (float)cmd->rect.rect.x + rw / 2;
        float ry = (float)cmd->rect.rect.y + rh / 2;
        struct rectangle* rect = add_rectangle(rx, ry, rw, rh);
        rect->color[0] = (float)cmd->rect.color.r / 255;
        rect->color[1] = (float)cmd->rect.color.g / 255;
        rect->color[2] = (float)cmd->rect.color.b / 255;
        // rect->color[3] = (float)cmd->rect.color.a / 255;
        rect->color[3] = 0.8f;
      }
      if (cmd->type == MU_COMMAND_ICON) {
      }
      if (cmd->type == MU_COMMAND_CLIP) {
      }
    }

    timer += time - prevTime;

    for (int i = 0; i < current_rectangle+1; i++) {
      draw_rectangle(&ui_rectangles[i]);
    }
    current_rectangle = 0;

    glBindTexture(GL_TEXTURE_2D, glyph_atlas_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    for (int i = 0; i < current_glyph+1; i++) {
      draw_rectangle(&ui_glyphs[i]);
    }
    current_glyph = 0;

    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  printf("\n");
  glDeleteProgram(background_shader);
  glDeleteProgram(glass_shader);
  glDeleteProgram(ui_shader_program);

  glDeleteVertexArrays(1, &VAO);
  glDeleteVertexArrays(1, &glass_VAO);

  for (int i = 0; i < RECTS_AMOUNT; i++) {
    glDeleteVertexArrays(1, &ui_rectangles[i].VAO);
  }

  glDeleteTextures(1, &texture);
  glfwTerminate();

  free(img);
  return EXIT_SUCCESS;
}
