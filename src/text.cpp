#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <algorithm>

#include <GL/glew.h>
#include <GL/freeglut.h>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <freetype2/ft2build.h>
#include FT_FREETYPE_H

#include <glstuff.h>
#include <text.h>

struct point {
  GLfloat x;
  GLfloat y;
  GLfloat s;
  GLfloat t;
};

static GLuint vbo;

static FT_Library ft;
static FT_Face face;

const char *fontfilename = "assets/terminus.ttf";

atlas *a;

int initFreetype() {
  if (FT_Init_FreeType(&ft)) {
    fprintf(stderr, "Could not init freetype library\n");
    return 0;
  }

  /* Load a font */
  if (FT_New_Face(ft, fontfilename, 0, &face)) {
    fprintf(stderr, "Could not open font %s\n", fontfilename);
    return 0;
  }

  program = compile_shader("src/text.vs", "src/text.fs");
  if(program == 0)
    return 0;
  else {
    printf("success\n");
  }

  attribute_coord = get_attrib(program, "coord");
  uniform_tex = get_uniform(program, "tex");
  uniform_color = get_uniform(program, "color");

  if(attribute_coord == -1 || uniform_tex == -1 || uniform_color == -1)
    return 0;

  glGenBuffers(1, &vbo);

  a = new atlas(face, 64);

  return 1;
}

void renderText(const char *text, atlas * a, float x, float y, float sx, float sy) {
  const uint8_t *p;

  glBindTexture(GL_TEXTURE_2D, a->tex);
  glUniform1i(uniform_tex, 0);

  glEnableVertexAttribArray(attribute_coord);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glVertexAttribPointer(attribute_coord, 4, GL_FLOAT, GL_FALSE, 0, 0);

  point coords[6 * strlen(text)];
  int c = 0;

  for (p = (const uint8_t *)text; *p; p++) {
    float x2 = x + a->c[*p].bl * sx;
    float y2 = -y - a->c[*p].bt * sy;
    float w = a->c[*p].bw * sx;
    float h = a->c[*p].bh * sy;

    x += a->c[*p].ax * sx;
    y += a->c[*p].ay * sy;

    if (!w || !h)
      continue;

    coords[c++] = (point) {
      x2, -y2, a->c[*p].tx, a->c[*p].ty};
    coords[c++] = (point) {
      x2 + w, -y2, a->c[*p].tx + a->c[*p].bw / a->w, a->c[*p].ty};
    coords[c++] = (point) {
      x2, -y2 - h, a->c[*p].tx, a->c[*p].ty + a->c[*p].bh / a->h};
    coords[c++] = (point) {
      x2 + w, -y2, a->c[*p].tx + a->c[*p].bw / a->w, a->c[*p].ty};
    coords[c++] = (point) {
      x2, -y2 - h, a->c[*p].tx, a->c[*p].ty + a->c[*p].bh / a->h};
    coords[c++] = (point) {
      x2 + w, -y2 - h, a->c[*p].tx + a->c[*p].bw / a->w, a->c[*p].ty + a->c[*p].bh / a->h};
  }

  glBufferData(GL_ARRAY_BUFFER, sizeof coords, coords, GL_DYNAMIC_DRAW);
  glDrawArrays(GL_TRIANGLES, 0, c);

  glDisableVertexAttribArray(attribute_coord);
}

void uiPosition(float wx, float wy, float x, float y, float z) {
  float sx = 2.0 / wx;
  float sy = 2.0 / wy;

  glUseProgram(program);
  glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  GLfloat black[4] = { 0, 0, 0, 1 };
  GLfloat red[4] = { 1, 0, 0, 1 };

  glUniform4fv(uniform_color, 1, black);

  char buff[128];
  snprintf(buff, sizeof(buff), "[%f %f %f]", x, y, z);
  glUniform4fv(uniform_color, 1, red);
  renderText(buff, a, -1 + 8 * sx, 1 - 50 * sy, sx, sy);
  glDisable(GL_BLEND);
}

void uiObject(float wx, float wy, const char *str) {
  float sx = 2.0 / wx;
  float sy = 2.0 / wy;

  glUseProgram(program);
  glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  GLfloat black[4] = { 0, 0, 0, 1 };
  GLfloat red[4] = { 1, 0, 0, 1 };

  glUniform4fv(uniform_color, 1, black);

  char buff[128];
  snprintf(buff, sizeof(buff), "%s", str);
  glUniform4fv(uniform_color, 1, red);
  renderText(buff, a, -1 + 8 * sx, 1 - 150 * sy, sx, sy);
  glDisable(GL_BLEND);
}


void destroyFreetype() {
  glDeleteProgram(program);
}
