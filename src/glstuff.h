void gl_error();
GLuint load_texture(char *file);
GLuint compile_shader(const char* vs, const char* fs);
GLint get_attrib(GLuint program, const char *name);
GLint get_uniform(GLuint program, const char *name);
