#define STBI_NO_JPEG
#define STBI_NO_BMP
#define STBI_NO_PSD
#define STBI_NO_TGA
#define STBI_NO_GIF
#define STBI_NO_HDR
#define STBI_NO_PIC
#define STBI_NO_PNM
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

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

#define WINDOW_WIDTH  1920
#define WINDOW_HEIGHT 1080
#define OPENGL_FAIL 9999

static Display* display = NULL;
static Screen* screen = NULL;
static Window current_window = {0};
static Window root_window = {0};

static int screen_width = 0;
static int screen_height = 0;

static float scroll_amount = 0.0f;
static float scroll_sensitivity = 0.1f;


static float pos[] = {0.1f, 0.8f};
static float backgroundPos[] = {0.0f, 0.0f};
static float scalePivot[] = {0.0f, 0.0f};


void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
  scroll_amount += (float)yoffset * scroll_sensitivity;
  printf("Scroll received %0.2f\n", scroll_amount);

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

void print_matrix(float mat[16]) {
  for (int y = 0; y < 4; y++) {
    for (int x = 0; x < 4; x++) {
      printf("%0.2f ", mat[(y * 4) + x]);
    }
    printf("\n");
  }
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
  struct FileData vertexSource = readFile(vs);
  struct FileData fragmentSource = readFile(fs);

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

  unsigned char *buffer = (unsigned char *)malloc(screen_width * screen_height * 4); // RGBA
  if (!buffer) {
    fprintf(stderr, "Failed to allocate memory for the pixel buffer\n");
    XDestroyImage(image);
    return NULL;
  }

  for (int y = 0; y < screen_height; ++y) {
    for (int x = 0; x < screen_width; ++x) {
      long pixel = XGetPixel(image, x, y);
      buffer[(y * screen_width + x) * 4 + 0] = (pixel & image->red_mask) >> 16; // R
      buffer[(y * screen_width + x) * 4 + 1] = (pixel & image->green_mask) >> 8; // G
      buffer[(y * screen_width + x) * 4 + 2] = (pixel & image->blue_mask); // B
      buffer[(y * screen_width + x) * 4 + 3] = 255; // A
    }
  }

  XDestroyImage(image);
  return buffer;
}

int main() {
  int image_width, image_height, image_nrChannels;

  // sleep(4);
  x_init();
  unsigned char* img = screenshot();
  
  unsigned char* image_textureData = stbi_load(
    "res/image.png",
    &image_width,
    &image_height,
    &image_nrChannels,
    0
  );

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

  float vertices[] = {
    //   pos       tex
     1.f, -1.f,  1.f, 1.f, // Bottom right
     1.f,  1.f,  1.f, 0.f, // Top right
    -1.f,  1.f,  0.f, 0.f, // Top left
    -1.f, -1.f,  0.f, 1.f, // Bottom left
  };

  unsigned int indices[] = {
    1, 2, 3,
    0, 1, 3
  };

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
  unsigned int background_shader_zoom = glGetUniformLocation(background_shader, "zoom");
  unsigned int background_shader_mousepos = glGetUniformLocation(background_shader, "mousepos");
  unsigned int background_shader_scalePivot = glGetUniformLocation(background_shader, "scalePivot");

  float unnormal_scale[] = {300.f, 300.f};
  float scale[] = {
    (unnormal_scale[0] / WINDOW_WIDTH),
    (unnormal_scale[1] / WINDOW_HEIGHT)
  };


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

  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);	
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
  // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, screen_width, screen_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, img);
  // glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image_width, image_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image_textureData);
  glGenerateMipmap(GL_TEXTURE_2D);
  stbi_image_free(image_textureData);

  glBindVertexArray(VAO);

  bool is_debug = false;
  double prevTime = 0, time = 0;
  double timer = 0;

  while (!glfwWindowShouldClose(window))
  {
    prevTime = time;
    time = glfwGetTime();
    double x, y;
    glfwGetCursorPos(window, &x, &y);
    pos[0] = (((float)x - WINDOW_WIDTH / 2.f) / WINDOW_WIDTH) * 2;
    pos[1] = ((WINDOW_HEIGHT / 2.f - (float)y) / WINDOW_HEIGHT) * 2;



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
    // printf("X: %02f, Y: %02f\n", pos[0], pos[1]);
    glBindVertexArray(glass_VAO);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glUseProgram(glass_shader);
    glUniform2fv(glass_shader_position_location, 1, pos);
    glUniform3fv(glass_shader_borderColor, 1, color);
    glUniform1f(glass_shader_zoom, scroll_amount + 1.f);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    if (glfwGetKey(window, GLFW_KEY_PERIOD) == GLFW_PRESS && timer > 0.2) {
      timer = 0;
      is_debug = !is_debug;
      glPolygonMode(GL_FRONT_AND_BACK, is_debug ? GL_LINE : GL_FILL); 
    }

    timer += time - prevTime;

    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  printf("\n");
  glDeleteProgram(background_shader);
  glDeleteProgram(glass_shader);
  glfwTerminate();

  free(img);
  return EXIT_SUCCESS;
}
