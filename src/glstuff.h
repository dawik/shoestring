GLuint compile_shader(const char* vs, const char* fs);
unsigned int load_texture(char *file);
void gl_error();
GLint get_attrib(GLuint program, const char *name);
GLint get_uniform(GLuint program, const char *name);
