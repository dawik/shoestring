#define STBI_NO_HDR
#define STB_IMAGE_IMPLEMENTATION

#include "stb_image.h"

#include <iostream>
#include <fstream>
#include <GL/glew.h>
#include <GL/gl.h>

using namespace std;

static string read_file(const char *);
static void compile_source(GLuint, const char*);

void gl_error()
{
  GLenum error = glGetError ();
  if (error != GL_NO_ERROR)
    {
      printf("\nglError 0x%04X\n", error);
      throw runtime_error("GL error");
    }
}

GLuint load_texture(char *file)
{
  unsigned int texture;
  unsigned char *image;
  int w, h, n, intfmt = 0, fmt = 0;

  image = stbi_load(file, &w, &h, &n, 0);

  if (!image) {
    fprintf(stderr, "cannot load texture '%s'\n %s\n", file, stbi_failure_reason());
    return 0;
  } else {
    printf("%s w:%d h:%d comp:%d\n", file, w, h, n);
  }

  if (n == 1) { intfmt = fmt = GL_LUMINANCE; }
  if (n == 2) { intfmt = fmt = GL_LUMINANCE_ALPHA; }
  if (n == 3) { intfmt = fmt = GL_RGB; }
  if (n == 4) { intfmt = fmt = GL_RGBA; }

  glActiveTexture(GL_TEXTURE2);
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);
  glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexImage2D(GL_TEXTURE_2D, 0, intfmt, w, h, 0, fmt, GL_UNSIGNED_BYTE, image);
  glGenerateMipmap(GL_TEXTURE_2D);

  free(image);

  return texture;
};


GLuint compile_shader(const char* vs, const char* fs)
{
  string vertex = read_file(vs);
  string frag = read_file(fs);

  GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
  GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);

  GLuint programShader = glCreateProgram();

  glAttachShader(programShader, vertexShader);
  glAttachShader(programShader, fragmentShader);

  compile_source(vertexShader, vertex.c_str());
  compile_source(fragmentShader, frag.c_str());

  glDeleteShader(vertexShader);
  glDeleteShader(fragmentShader);

  glLinkProgram(programShader);

  GLint status;
  GLint length;
  char log[4096] = {0};

  glGetProgramiv(programShader, GL_LINK_STATUS, &status);
  glGetProgramInfoLog(programShader, 4096, &length, log);

  if(status == GL_FALSE){
    printf("link failed %s\n", log);
  }


  return programShader;
}

static string read_file(const char *filename)
{
  string shader;
  ifstream ifs;
  ifs.open(filename);
  while(!ifs.eof())
    {
      string tempholder;
      getline(ifs, tempholder);
      shader.append(tempholder);
      shader.append("\n");
    }
  ifs.close();

  return shader;
}

static void compile_source(GLuint shader, const char* src)
{
  glShaderSource(shader, 1, &src, NULL);
  glCompileShader(shader);

  GLint status;
  GLint length;
  char log[4096] = {0};

  glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
  glGetShaderInfoLog(shader, 4096, &length, log);
  if(status == GL_FALSE) {
    fprintf(stderr, "compile failed %s\n", log);
    throw runtime_error("GLSL Compilation error");
  }
}



GLint get_attrib(GLuint program, const char *name) {
  GLint attribute = glGetAttribLocation(program, name);
  if(attribute == -1)
    fprintf(stderr, "Could not bind attribute %s\n", name);
  return attribute;
}

GLint get_uniform(GLuint program, const char *name) {
  GLint uniform = glGetUniformLocation(program, name);
  if(uniform == -1)
    fprintf(stderr, "Could not bind uniform %s\n", name);
  return uniform;
}
