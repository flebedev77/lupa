#!/usr/bin/tcc -run -lfreetype -I/usr/include/freetype2 -I/usr/include/libpng16 -I/usr/include/harfbuzz -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include -I/usr/include/sysprof-6 -pthread
#include <ft2build.h>
#include FT_FREETYPE_H
#include <stdio.h>
#include <stdint.h>
#include <string.h>

// #define RELEASE
#define TEXT_SIZE 30
#ifndef RELEASE
#define ATLAS_WIDTH 256
#define ATLAS_HEIGHT 256
#endif
#ifdef RELEASE
#define ATLAS_WIDTH 1024
#define ATLAS_HEIGHT 1024
#endif
#define BITMAP_SIZE (ATLAS_WIDTH * ATLAS_HEIGHT)

size_t get_file_size(const char* path) {
  FILE* file = fopen(path, "r");
  if (file == NULL) {
    perror("Failed to open file");
    return -1;
  }

  fseek(file, 0, SEEK_END);
  size_t file_size = ftell(file);
  fseek(file, 0, SEEK_SET);

  fclose(file);
  return file_size;
}

void rect_copy(unsigned char* dst, unsigned char* src, int x, int y, int src_width, int src_height, int dst_width, int dst_height) {
  for (int iy = 0; iy < src_height; iy++) {
    for (int ix = 0; ix < src_width; ix++) {
      int fx = ix + x;
      int fy = iy + y;
      dst[fy * dst_width + fx] = src[iy * src_width + ix];
    }
  }
}

uint32_t pack_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
  return (((uint32_t)a << (4*7)) | ((uint32_t)b << (4*5)) | ((uint32_t)g << (4*3)) | (uint32_t)r);
}

int main(int argc, char** argv) {
  if (argc != 2) {
    fprintf(stderr, "Invalid usage\n");
    return 1;
  }
  unsigned char* final_bitmap = (unsigned char*)malloc(BITMAP_SIZE);
  memset(final_bitmap, 0, BITMAP_SIZE);

  unsigned char* bitmap_pixels = (unsigned char*)final_bitmap;

  char* face_path = argv[1];

  FT_Library library;
  FT_Face face;

  int error = FT_Init_FreeType(&library);
  if (error) {
    fprintf(stderr, "Error initializing FreeType library\n");
    return 1;
  }
  
  error = FT_New_Face(library, face_path, 0, &face);
  if (error) {
    fprintf(stderr, "Error initializing face %s\n", face_path);
    return 1;
  }
  // error = FT_Set_Char_Size(
  //     face,    /* handle to face object         */
  //     0,       /* char_width in 1/64 of points  */
  //     16*64,   /* char_height in 1/64 of points */
  //     ATLAS_WIDTH,     /* horizontal device resolution  */
  //     ATLAS_HEIGHT);   /* vertical device resolution    */
  // if (error) {
  //   fprintf(stderr, "Error changing face char size %s\n", face_path);
  //   return 1;
  // }

  FT_Set_Pixel_Sizes(face, 0, TEXT_SIZE);

  const char asciioffset = 33;
  const char* text = "!\"#$%&'()*+,-./0123456789;:<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~";
  size_t textLen = strlen(text);

  printf("#pragma once\n\n");

  printf("typedef struct {\n");
  printf("  int x, y, w, h, leftoff, topoff;\n");
  printf("  char code;\n");
  printf("} GlyphRect_t;\n\n");
  printf("GlyphRect_t glyph_lookup[%zd] = {\n", textLen);
  
  int widest_char = 0;
  int tallest_char = 0;
  int biggest_bitmap_top = 0;
  for (char ccode = 0; ccode < 127; ccode++) {
    int glyph_index = FT_Get_Char_Index(face, ccode);
    if ((char)glyph_index == ccode || glyph_index == 0) {
      continue;
    }
    error = FT_Load_Glyph(face, glyph_index, 0);
    if (error) {
      continue;
    }

    error = FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL);
    if (error) {
      continue;
    }

    if (face->glyph->bitmap.width > widest_char) {
      widest_char = face->glyph->bitmap.width;
    }
    if (face->glyph->bitmap.rows > tallest_char) {
      tallest_char = face->glyph->bitmap.rows;
    }
    if (face->glyph->bitmap_top > biggest_bitmap_top) {
      biggest_bitmap_top = face->glyph->bitmap_top;
    }
  }
  int max_char_width = ATLAS_WIDTH;
  int max_char_height = ATLAS_HEIGHT;
  int penx = 0, peny = biggest_bitmap_top;
  for (int i = 0; i < strlen(text); i++) {
    char charcode = text[i];
    int glyph_index = FT_Get_Char_Index(face, charcode);
    if ((char)glyph_index == charcode || glyph_index == 0) {
      printf("Char %c not contained in face %s\n", charcode, face_path);
      continue;
    }

    error = FT_Load_Glyph(face, glyph_index, 0);
    if (error) {
      printf("Error loading glyph %d for face %s\n", glyph_index, face_path);
      continue;
    }

    error = FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL);

    FT_GlyphSlot glyph = face->glyph;

    if (charcode == '\'' || charcode == '\\') 
      printf("  { .x = %d, .y = %d, .w = %d, .h = %d, .leftoff = %d, .topoff = %d, .code = '\\%c'},\n",
        penx, peny, glyph->bitmap.width, glyph->bitmap.rows, glyph->bitmap_left, glyph->bitmap_top, charcode);
    else 
      printf("  { .x = %d, .y = %d, .w = %d, .h = %d, .leftoff = %d, .topoff = %d, .code = '%c'},\n",
        penx, peny, glyph->bitmap.width, glyph->bitmap.rows, glyph->bitmap_left, glyph->bitmap_top, charcode);

    rect_copy(bitmap_pixels, glyph->bitmap.buffer,
        penx + glyph->bitmap_left,
        (peny == 0 && glyph->bitmap_top > 0) ? peny : peny - glyph->bitmap_top,
        glyph->bitmap.width, glyph->bitmap.rows, ATLAS_WIDTH, ATLAS_HEIGHT);
    penx += glyph->advance.x >> 6;
    // penx += glyph->bitmap.width;
    // penx += widest_char;
    if (penx + glyph->bitmap.width > ATLAS_WIDTH) {
      penx = 0;
      // peny += glyph->bitmap.rows;
      peny += tallest_char;
      // peny += glyph->advance.y >> 6;
      if (peny + tallest_char > ATLAS_HEIGHT) {
        peny = 0;
      }
    }
  }

  printf("};\n");
  printf("unsigned int font_length = %zd;\n", BITMAP_SIZE);
  printf("unsigned int font_width = %zd;\n", ATLAS_WIDTH);
  printf("unsigned int font_height = %zd;\n", ATLAS_HEIGHT);
  printf("unsigned int glyph_width = %zd;\n", widest_char);
  printf("unsigned int glyph_height = %zd;\n\n", tallest_char);
  // printf("%s\n", face->family_name);
  
  printf("unsigned char font_data[] = {");
  for (size_t i = 0; i < BITMAP_SIZE; i++) {
    if (i % 20 == 0) {
      printf("\n");
    }
    printf("0x%02X, ", final_bitmap[i]);
  }
  printf("};\n");

  free(final_bitmap);
}
