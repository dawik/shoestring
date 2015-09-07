#define NO_SDL_GLEXT
#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <SDL2/SDL_opengl.h>
#include <GL/gl.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <memory>
#include <vector>

#include <iostream>
#include <stdio.h>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "btBulletDynamicsCommon.h"
#include "LinearMath/btSerializer.h"
#include "LinearMath/btIDebugDraw.h"
#include "btBulletFile.h"
#include "btBulletWorldImporter.h"

#define STBI_NO_HDR
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"


#ifndef GL_GENERATE_MIPMAP
#define GL_GENERATE_MIPMAP 0x8191
#endif

#ifndef GL_MULTISAMPLE
#define GL_MULTISAMPLE 0x809D
#endif

#ifndef GL_SAMPLE_ALPHA_TO_COVERAGE
#define GL_SAMPLE_ALPHA_TO_COVERAGE 0x809E
#endif

const float twopi = 6.28318530718;
const float player_mass = 1.0;
const glm::vec3 up(0,0,-1);
int o = 0; 
struct draw_opts
{
        bool textured = true;
        bool selected = false;
        btTransform camera;
};

// An array of 3 vectors which represents 3 vertices
static const GLfloat g_vertex_buffer_data[] = {
   1.0f, -.33f, 0.0f,
   -1.0f, -.33f, 0.0f,
   -1.0f, -1.f, 0.0f,
   1.0f, -1.f, 0.0f,
   1.0f, -.33f, 0.0f,
};

GLuint vertexbuffer, triangle_shader;
void setup_triangle()
{
        // This will identify our vertex buffer

        // Generate 1 buffer, put the resulting identifier in vertexbuffer
        glGenBuffers(1, &vertexbuffer);

        // The following commands will talk about our 'vertexbuffer' buffer
        glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);

        // Give our vertices to OpenGL.
        glBufferData(GL_ARRAY_BUFFER, sizeof(g_vertex_buffer_data), g_vertex_buffer_data, GL_STATIC_DRAW);
}

void draw_triangle()
{
        // 1rst attribute buffer : vertices
        glEnableVertexAttribArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
        glVertexAttribPointer(
                        0,                  // attribute 0. No particular reason for 0, but must match the layout in the shader.
                        3,                  // size
                        GL_FLOAT,           // type
                        GL_FALSE,           // normalized?
                        0,                  // stride
                        (void*)0            // array buffer offset
                        );

        // Draw the triangle !
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 5); // Starting from vertex 0; 3 vertices total -> 1 triangle

        glDisableVertexAttribArray(0);
}

class Material
{
        public:
                char name[128];
                int texture;
                char bitmap_file[256];
                float shininess = 0.1;
                float diffuse[4] = {1,1,1,1};
                float specular[4] = {1,1,1,1};
};

class Mesh
{
        public:
                std::vector<float> vertexdata;
                std::vector<unsigned int> elements;

                unsigned int numVerts, numElements, vao, vbo, ebo;

                Mesh() {
                        hasTexture = false;
                };
                ~Mesh() {};

                void init_buffer(GLuint);

                Material *material;

                GLuint shader;

                char name[128];
                bool hasTexture;
                unsigned int texture;
};

class Camera
{
        public:
                char name[256];
                float fov;
                float near;
                float far;
                float aspect;
                glm::mat4 world;
};

class Object
{
        public:
                char name[128];

                float rgba[4];

                Mesh *mesh;

                float transform[16];

                btCollisionShape *shape;

                std::vector<btRigidBody*> body_instances;

                Object(const char *_name, btRigidBody* _body, Mesh* _mesh)
                {
                        rgba[0] = 1.0;
                        rgba[1] = 1.0;
                        rgba[2] = 1.0;
                        rgba[3] = 1.0;
                        strncpy(name, _name, 128);
                        body = _body;
                        mesh = _mesh;
                }

                Object(const char *_name, btRigidBody* _body, Mesh* _mesh, float tint[4])
                {
                        rgba[0] = tint[0];
                        rgba[1] = tint[1];
                        rgba[2] = tint[2];
                        rgba[3] = tint[3];
                        strncpy(name, _name, 128);
                        shape = _body->getCollisionShape();
                        body = _body;
                        mesh = _mesh;
                }

                void new_instance(std::shared_ptr<btDiscreteDynamicsWorld> world, btTransform t, btScalar mass, btRigidBody *b)
                {
                        btMotionState* motion=new btDefaultMotionState(t);
                        btVector3 inertia(0,0,0);
                        shape->calculateLocalInertia(mass,inertia);
                        btRigidBody::btRigidBodyConstructionInfo info(0,motion,shape,inertia);

                        btRigidBody *body = b ? b : new btRigidBody(info);

                        body->setMassProps(mass, inertia);
                        body->setMotionState(motion);
                        //body->setActivationState(WANTS_DEACTIVATION);

                        body_instances.push_back(body);
                        world->addRigidBody(body);
                }

                ~Object()
                {
                };

                btRigidBody *body;

                void draw_buffer(struct draw_opts opt);
};

void gl_error()
{
        GLenum error = glGetError (); 
        if (error != GL_NO_ERROR)
        {
                printf("\nglError 0x%04X\n", error);
                throw std::runtime_error("GL error");
        }
}


static void compile_shader_src(GLuint shader, const char* src)
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
                throw std::runtime_error("GLSL Compilation error");
        }
}


GLuint compile_shader(const char* vertex, const char* fragment)
{
        // Attempts to compile and link., links and returns a 
        GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
        GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        GLuint programShader = glCreateProgram();

        glAttachShader(programShader, vertexShader);
        glAttachShader(programShader, fragmentShader);

        compile_shader_src(vertexShader, vertex);

        compile_shader_src(fragmentShader, fragment);

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

char* read_file(const char* filename)
{
        FILE* f = fopen(filename, "rb");
        fseek(f, 0, SEEK_END);
        long len = ftell(f);
        fseek(f, 0, SEEK_SET);

        char* buffer = (char*) calloc(1, len + 1); 
        assert(buffer);

        size_t ret = fread(buffer, len, 1, f); 
        assert(ret);

        fclose(f);

        return buffer;
}

void Mesh::init_buffer(GLuint activeShader)
{
        shader = activeShader;
        size_t stride = hasTexture ? sizeof(float) * 8 : sizeof(float) * 6;
        GLint vertexAttrib = glGetAttribLocation (activeShader, "vertex");
        GLint normalAttrib = glGetAttribLocation (activeShader, "normal");
        GLint uvAttrib = glGetAttribLocation (activeShader, "uv");
        glGenBuffers (1, &vbo);
        glBindBuffer (GL_ARRAY_BUFFER, vbo);
        glBufferData (GL_ARRAY_BUFFER, sizeof (float) * vertexdata.size(), &vertexdata[0], GL_STREAM_DRAW);

        glGenVertexArrays (1, &vao);
        glBindVertexArray (vao);

        glEnableVertexAttribArray (vertexAttrib);
        glVertexAttribPointer (vertexAttrib, 3, GL_FLOAT, GL_FALSE, stride, NULL);

        glEnableVertexAttribArray (normalAttrib);
        glVertexAttribPointer (normalAttrib, 3, GL_FLOAT, GL_FALSE, stride, (const GLvoid*)(3 * sizeof(GLfloat)));

        if (hasTexture)
        {
                glEnableVertexAttribArray (uvAttrib);
                glVertexAttribPointer (uvAttrib, 2, GL_FLOAT, GL_TRUE, stride, (const GLvoid*)(6 * sizeof(GLfloat)));
        }
        gl_error();


        glGenBuffers(1, &ebo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, (elements.size()) * sizeof(unsigned int), &elements[0], GL_STATIC_DRAW);

        glBindVertexArray (0);

}

struct Instance {
        public:
                Instance(const char *_name)
                {
                        strcpy(name, _name);
                }
                float transform[16];
                char name[256];
};

void transposematrix(float m[16], aiMatrix4x4 *p)
{
        m[0] = p->a1; m[4] = p->a2; m[8] = p->a3; m[12] = p->a4;
        m[1] = p->b1; m[5] = p->b2; m[9] = p->b3; m[13] = p->b4;
        m[2] = p->c1; m[6] = p->c2; m[10] = p->c3; m[14] = p->c4;
        m[3] = p->d1; m[7] = p->d2; m[11] = p->d3; m[15] = p->d4;
}

class Context
{
        std::shared_ptr<btDiscreteDynamicsWorld> world;
        std::shared_ptr<btCollisionDispatcher> dispatcher;
        std::shared_ptr<btCollisionConfiguration> collisionConfig;
        std::shared_ptr<btDbvtBroadphase> broadphase;
        std::shared_ptr<btSequentialImpulseConstraintSolver> solver;

        std::vector<std::shared_ptr<Object>> objects;
        std::vector<Mesh> meshes;
        std::vector<Material> materials;
        std::vector<Camera> cameras;

        std::vector<Instance> instances;

        Object *player;
        glm::mat4 look, perspective;

        GLuint shader, grapple_shader;

        int width;
        int height;
        int mode;
        int new_resolution;
        int input[8];
        char path[256];
        float pitch; 
        float yaw;
        float near; 
        float far; 
        float fov; 
        float aspect;

        bool player_grounded = false;

        SDL_Window *window;
        SDL_DisplayMode video_mode = { SDL_PIXELFORMAT_UNKNOWN, 0, 0, 0, 0 };

        glm::vec3 forward;
        glm::mat4 camera_rotation;

        btRigidBody *held_object = NULL;
        bool grapple_target = false;
        btVector3 grapple_position;

        btRigidBody *FirstCollisionRayTrace(int, int);
        void AssetsFromScene(const struct aiScene*);
        void AddMesh(const struct aiMesh*);
        void AddMaterial(const aiMaterial *_material);
        void instancesFromGraph(struct aiNode *node, aiMatrix4x4 _transform)
        {
                if (node)
                {
                        aiMatrix4x4 _t = node->mTransformation * _transform;
                        Instance i(node->mName.data);
                        transposematrix(i.transform, &_t);
                        for (int i = 0; i < node->mNumChildren; i++) {
                                instancesFromGraph(node->mChildren[i], _t);
                        }
                        instances.push_back(i);
                }
        }

        void Init_SDL(void)
        {
                SDL_Init( SDL_INIT_VIDEO );
                //SDL_WM_GrabInput(SDL_GRAB_OFF);
                SDL_SetRelativeMouseMode(SDL_TRUE);
                SDL_ShowCursor(0);
                mode = 1;

                if (SDL_GetDisplayMode(0, 0, &video_mode) != 0) {
                        SDL_Log("SDL_GetDisplayMode failed: %s", SDL_GetError());
                        exit(1);       
                }

                printf("Numdisplaymodes %d\n", SDL_GetNumDisplayModes(1));

                width = video_mode.w;
                height = video_mode.h;
                width = 1920;
                height = 1080;
                window = SDL_CreateWindow("Shoestring Game Engine", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, SDL_WINDOW_OPENGL | SDL_WINDOW_MAXIMIZED);
        }

        void Init_GL(void)
        {
                SDL_GL_CreateContext(window);
                glewExperimental = GL_TRUE;
                glewInit(); 
                shader = compile_shader(read_file("src/standard.vert.glsl"), read_file("src/standard.frag.glsl"));
                triangle_shader = compile_shader(read_file("src/ui.vs.glsl"), read_file("src/ui.fs.glsl"));

                glUseProgram(shader);
                glEnable(GL_BLEND);
                //glBlendFunc(GL_SRC_COLOR, GL_ZERO);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

                gl_error();
        }

        void Init_Bullet()
        {
                collisionConfig.reset(new btDefaultCollisionConfiguration());
                dispatcher.reset(new btCollisionDispatcher(&*collisionConfig));
                broadphase.reset(new btDbvtBroadphase());
                solver.reset(new btSequentialImpulseConstraintSolver());
                world.reset(new btDiscreteDynamicsWorld(&*dispatcher,&*broadphase,&*solver,&*collisionConfig));
        }

        void Load_Scene(const char *scene_file)
        {
                Assimp::Importer importer;
                const struct aiScene *scene = importer.ReadFile(scene_file, aiProcessPreset_TargetRealtime_Fast);
                printf("Loading scene from %s\n\t%s\n", scene_file, importer.GetErrorString());

                aiMatrix4x4 root_transform;

                instancesFromGraph(scene->mRootNode, root_transform);

                AssetsFromScene(scene);

                for (Mesh &mesh : meshes)
                {
                        mesh.init_buffer(shader);
                }

        }

        void Load_World(const char *bullet_file)
        {
                btBulletWorldImporter*		m_fileLoader;
                m_fileLoader = new btBulletWorldImporter(&*world);

                printf("Physics file %s\n", bullet_file);
                m_fileLoader->setVerboseMode(false);

                if (m_fileLoader->loadFile(bullet_file))
                {
                        printf("Loaded %s....\n%d\tconstraints\n%d\trigid bodies\n", 
                                        bullet_file, m_fileLoader->getNumConstraints(), m_fileLoader->getNumRigidBodies());
                        for(int i=0; i < m_fileLoader->getNumRigidBodies(); i++)
                        {
                                btCollisionObject* obj = m_fileLoader->getRigidBodyByIndex(i);
                                btRigidBody* body = btRigidBody::upcast(obj);      
                                const char *name = m_fileLoader->getNameForPointer(body);
                                if (body)
                                {
                                        if (MatchBodyWithInstanceAndMesh(body, name) == false)
                                        {
                                                printf("Failed to assign body %s\n", name);
                                                exit(4);
                                        }
                                }

                        }
                        for (Instance &i: instances)
                        {
                                if (strcmp("Sky", i.name) == 0)
                                {
                                        float sky_h = 0.1;
                                        float sky_r = 0.1;

                                        btCapsuleShape *sky_shape=new btCapsuleShape(sky_h, sky_r);

                                        btVector3 inertia(0,0,0);
                                        sky_shape->calculateLocalInertia(player_mass,inertia);

                                        btTransform t;  
                                        t.setFromOpenGLMatrix(i.transform);
                                        btMotionState* motion=new btDefaultMotionState(t);

                                        btRigidBody::btRigidBodyConstructionInfo info(0.0,motion,sky_shape,inertia);
                                        btRigidBody *_b = new btRigidBody(info);
                                        for (Mesh &m : meshes)
                                        {
                                                if (strcmp("Sky", m.name) == 0)
                                                {
                                                        printf("FOUND THE SKY\n");
                                                        float color[4] = { 1.0, 1.0, 1.0, 1.0 };
                                                        std::shared_ptr<Object> object(new Object("Sky", _b, &m, color));
                                                        object->new_instance(world, t, 1.0 / _b->getInvMass(), _b);
                                                        objects.push_back(object);
                                                }
                                        }
                                }
                        }

                        if (cameras.size())
                        {
                                Camera cam = cameras.at(0);
                                pitch = twopi;
                                yaw = twopi;
                                fov = cam.fov;
                                aspect = cam.aspect;
                                near = 0.01;
                                far = 10000.0;

                                float player_height = 2.0;
                                float player_radius = 2.0;

                                btCapsuleShape *player_shape=new btCapsuleShape(player_radius, player_height);

                                btVector3 inertia(0,0,0);
                                player_shape->calculateLocalInertia(player_mass,inertia);

                                btTransform t;  

                                t.setFromOpenGLMatrix(glm::value_ptr(glm::transpose(cam.world)));
                                btMotionState* motion=new btDefaultMotionState(t);

                                btRigidBody::btRigidBodyConstructionInfo info(player_mass,motion,player_shape,inertia);
                                btRigidBody* player_body=new btRigidBody(info);

                                player_body->setCollisionFlags( btCollisionObject::CF_CHARACTER_OBJECT);
                                player_body->setSleepingThresholds(0.0, 0.0);
                                player_body->setAngularFactor(0.0);

                                world->addRigidBody(player_body);

                                player = new Object("Player", player_body, NULL);
                        } 
                }
                else 
                {
                        throw std::runtime_error("Failed to physics data");
                }
        }
        void CollisionRoutine(void);

        void FreezeInstances();

        public: 
        Context(int argc, char **)
        {
                if (argc != 1)
                {
                        throw std::invalid_argument("No command line interface yet\n");
                } 

                srand(time(NULL));

                Init_SDL();

                Init_GL();

                Init_Bullet();

                setup_triangle();

                const char *scene = "assets/sandbox.fbx", *physics = "assets/sandbox.bullet";
                // Note: Mesh needs to be -Y forward +Z up, 
                // to align properly with collision shapes from Bullet

                for (unsigned long i = 0, sep = 0; i < strlen(scene); i++)
                {
                        if (scene[i] == '/')
                                sep = i;
                        if (i == strlen(scene) - 1)
                        {
                                sep = sep ? sep + 1 : 0;
                                strncat(path, scene, sizeof(char) * (sep));
                        }
                }

                Load_Scene(scene);

                Load_World(physics);
        }
        ~Context()
        {
                SDL_Quit();
        }
        void PollInput();
        void Draw();
        void UpdatePosition();
        void PollWindow();
        void Loop();
        bool MatchBodyWithInstanceAndMesh(btRigidBody *body, const char *name);
        void AddObject(const char *name, btRigidBody *body, Mesh *mesh, float transform[16]);
};


unsigned int loadtexture(char *filename)
{
        unsigned int texture;
        unsigned char *image;
        int w, h, n, intfmt = 0, fmt = 0;

        image = stbi_load(filename, &w, &h, &n, 0); 

        if (!image) {
                fprintf(stderr, "cannot load texture '%s'\n", filename);
                return 0;
        } else {
                printf("%s w:%d h:%d comp:%d\n", filename, w, h, n);
        }

        if (n == 1) { intfmt = fmt = GL_LUMINANCE; }
        if (n == 2) { intfmt = fmt = GL_LUMINANCE_ALPHA; }
        if (n == 3) { intfmt = fmt = GL_RGB; }
        if (n == 4) { intfmt = fmt = GL_RGBA; }

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
}

struct aiNode *findnode(struct aiNode *node, const char *name)
{
        if (node)
        {
                unsigned int i;
                if (!strcmp(name, node->mName.data))
                        return node;
                for (i = 0; i < node->mNumChildren; i++) {
                        struct aiNode *found = findnode(node->mChildren[i], name);
                        if (found)
                                return found;
                }
        }
        return NULL;
}

void Context::AddMesh(const aiMesh *_mesh)
{
        Mesh mesh;
        strcpy(mesh.name, _mesh->mName.C_Str());
        mesh.numElements = _mesh->mNumFaces * 3;
        for (unsigned int j = 0; j < _mesh->mNumVertices; j++)
        {
                mesh.vertexdata.push_back(_mesh->mVertices[j].x);
                mesh.vertexdata.push_back(_mesh->mVertices[j].y);
                mesh.vertexdata.push_back(_mesh->mVertices[j].z);

                if (_mesh->mNormals)
                {
                        mesh.vertexdata.push_back(_mesh->mNormals[j].x);
                        mesh.vertexdata.push_back(_mesh->mNormals[j].y);
                        mesh.vertexdata.push_back(_mesh->mNormals[j].z);
                }

                if (_mesh->mTextureCoords[0])
                {
                        mesh.vertexdata.push_back(_mesh->mTextureCoords[0][j].x);
                        mesh.vertexdata.push_back(_mesh->mTextureCoords[0][j].y);
                        mesh.hasTexture = true;
                }
        }

        for (unsigned int j = 0; j < _mesh->mNumFaces; j++)
        {
                mesh.elements.push_back(_mesh->mFaces[j].mIndices[0]);
                mesh.elements.push_back(_mesh->mFaces[j].mIndices[1]);
                mesh.elements.push_back(_mesh->mFaces[j].mIndices[2]);
        }

        if (mesh.hasTexture)
        {
                mesh.material = &materials.at(_mesh->mMaterialIndex);
                mesh.texture = materials.at(_mesh->mMaterialIndex).texture;
        }

        meshes.push_back(mesh);
};

void Context::AddMaterial(const struct aiMaterial *_material)
{
        Material material;
        char filename[2000];
        struct aiString str;
        if (!aiGetMaterialString(_material, AI_MATKEY_TEXTURE_DIFFUSE(0), &str)) 
        {
                char *s = strrchr(str.data, '/');
                if (!s) s = strrchr(str.data, '\\');
                if (!s) s = str.data; else s++;
                strcpy(filename, path);
                strcat(filename, str.data);
                strcpy(material.bitmap_file, filename);
                material.texture = -1;
                for(Material &m : materials)
                {
                        if (strcmp(filename, m.bitmap_file) == 0)
                        {
                                material.texture = m.texture;
                                printf("Found existing texture\n");
                                break;
                        }
                }
                if (material.texture < 0)
                {
                        material.texture = loadtexture(filename);
                }
        }
        else
                material.texture = -1;
        aiString _n;
        _material->Get(AI_MATKEY_NAME, _n);
        strcpy(material.name, _n.data);
        _material->Get(AI_MATKEY_COLOR_DIFFUSE,material.diffuse);
        _material->Get(AI_MATKEY_COLOR_SPECULAR,material.specular);
        aiColor3D color (0.f,0.f,0.f);
        _material->Get(AI_MATKEY_COLOR_DIFFUSE,color);
        material.diffuse[0] = color.r;
        material.diffuse[1] = color.g;
        material.diffuse[2] = color.b;
        material.diffuse[3] = 1.0;
        _material->Get(AI_MATKEY_COLOR_SPECULAR,color);
        material.specular[0] = color.r;
        material.specular[1] = color.g;
        material.specular[2] = color.b;
        material.specular[3] = 1.0;
        //printf("Material color %f %f %f\n", color.r,color.g,color.b);
        _material->Get(AI_MATKEY_SHININESS,material.shininess);
        //printf("Material %s diffuse color %f %f %f\n", material.name, material.specular[0],material.specular[1],material.specular[2]);
        materials.push_back(material);
}

void Context::AssetsFromScene(const struct aiScene *scene)
{
        fprintf(stderr, "\nReading scene...\n%d\tmeshes\n%d\tmaterials\n%d\tcameras\n", 
                        scene->mNumMeshes, scene->mNumMaterials, scene->mNumCameras);
        for (unsigned int i = 0; i < scene->mNumMaterials; i++)
        {
                AddMaterial(scene->mMaterials[i]);
        }
        for (unsigned int i = 0; i < scene->mNumMeshes; i++)
        {
                AddMesh(scene->mMeshes[i]);
        }

        for (unsigned int i = 0; i < scene->mNumCameras; i++)
        {
                Camera camera;
                camera.fov = scene->mCameras[i]->mHorizontalFOV * 57.2957795;
                camera.near = scene->mCameras[i]->mClipPlaneNear;
                camera.far = scene->mCameras[i]->mClipPlaneFar;
                camera.aspect = scene->mCameras[i]->mAspect;
                strcpy(camera.name, scene->mCameras[i]->mName.C_Str());
                struct aiNode *node = findnode(scene->mRootNode, scene->mCameras[i]->mName.C_Str());
                if (node)
                {
                        aiMatrix4x4 *mat = &node->mTransformation;
                        camera.world = glm::mat4(glm::vec4(mat->a1, mat->a2, mat->a3, mat->a4),
                                        glm::vec4(mat->b1, mat->b2, mat->b3, mat->b4),
                                        glm::vec4(mat->c1, mat->c2, mat->c3, mat->c4),
                                        glm::vec4(mat->d1, mat->d2, mat->d3, mat->d4));
                        cameras.push_back(camera);
                } 
                else 
                {
                        fprintf(stderr, "Could not find camera transformation for %s in graph\n", camera.name);
                } 
        }
};

void Context::FreezeInstances()
{
        for (std::shared_ptr<Object> object : objects)
        {
                for (btRigidBody *instance : object->body_instances)
                {
                        instance->setGravity(btVector3(0,0,0));
                        instance->setActivationState(ISLAND_SLEEPING);
                }
        }
}

void Context::Draw()
{
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glClearColor(0,0,1,0.5);
        float mat[16];

        btTransform t = player->body->getWorldTransform();
        t.getOpenGLMatrix(mat);

        btVector3 origin = t.getOrigin();
        glm::vec3 eye = glm::vec3(origin.x(), origin.y(), origin.z());

        look =  glm::lookAt(eye, eye + (forward * float(5)), up);
        perspective =  glm::perspective(fov, aspect, near, far);

        glUseProgram(shader);
        glUniformMatrix4fv (glGetUniformLocation (shader, "camera"), 1, GL_FALSE, glm::value_ptr(look));
        glUniformMatrix4fv (glGetUniformLocation (shader, "perspective"), 1, GL_FALSE, glm::value_ptr(perspective));
        glUniform3f (glGetUniformLocation (shader, "cameraPosition"), t.getOrigin().x(), 
                        t.getOrigin().y(), 
                        t.getOrigin().z()); 

        //glEnable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        struct draw_opts opt;
        const float distance = 10.0;
        t.setIdentity();
        t.setOrigin(player->body->getWorldTransform().getOrigin());
        t.setOrigin(t.getOrigin() + btVector3(forward.x * distance, forward.y * distance, forward.z * distance));
        t.setOrigin(btVector3(round(t.getOrigin().x()),round(t.getOrigin().y()), round(t.getOrigin().z())));
        opt.camera = t;
        for (std::shared_ptr<Object> object : objects)
        {

                opt.selected = object == objects[o] ? true : false;
                object->draw_buffer(opt);
        }

        glUseProgram(triangle_shader);
        draw_triangle();

        //glDisable(GL_CULL_FACE);
        glDisable(GL_TEXTURE_2D);
        glDisable(GL_DEPTH_TEST);
}

enum {LEFT = 0, RIGHT = 1, FORWARD = 2, BACK = 3, LEFT_CLICK = 4, RIGHT_CLICK = 5, MIDDLE_CLICK = 6, JUMP = 7};

void Context::PollInput()
{
        SDL_Event event;
        while (SDL_PollEvent (&event))
        {
                const Uint8 *keystate = SDL_GetKeyboardState(NULL);
                switch (event.type) 
                {
                        case SDL_MOUSEWHEEL:
                                {
                                        btTransform t;
                                        t.setIdentity();
                                        t.setOrigin(player->body->getWorldTransform().getOrigin());

                                        const float radius = 10;
                                        t.setOrigin(t.getOrigin() + (btVector3(forward.x, forward.y, forward.z) * radius));
                                        t.setOrigin(btVector3(round(t.getOrigin().x()),round(t.getOrigin().y()), round(t.getOrigin().z())));
                                        //glm::vec3 eye = glm::vec3(origin.x(), origin.y(), origin.z());
                                        //t.setFromOpenGLMatrix(objects[o]->transform);
                                        objects[o]->new_instance(world, t, 0.0, NULL);
                                        //printf("WEE MOUSEWHEEL %d\n", event.motion.x);
                                        /*
                                           for (auto o : objects) {
                                           btTransform t;
                                           t.setFromOpenGLMatrix(o->transform);
                                           o->new_instance(world, t, 10, NULL);
                                           }
                                           */
                                        break;
                                }
                        case SDL_MOUSEBUTTONDOWN:
                                switch (event.button.button)
                                {
                                        case SDL_BUTTON_LEFT:
                                                {
                                                        input[LEFT_CLICK] = 1;
                                                        break;
                                                }
                                        case SDL_BUTTON_MIDDLE:
                                                {
                                                        input[MIDDLE_CLICK] = 1;
                                                        break;
                                                }

                                        case SDL_BUTTON_RIGHT:
                                                //if (player_grounded)
                                                {
                                                        btVector3 velocity = player->body->getLinearVelocity();
                                                        velocity.setZ(10);
                                                        player->body->setLinearVelocity(velocity);
                                                        //player_grounded = false;
                                                        break;
                                                }
                                }
                                break;

                        case SDL_MOUSEMOTION:
                                {
                                        float sensitivity = 0.001;
                                        float pitch_motion = sensitivity * event.motion.xrel;
                                        float yaw_motion = sensitivity * event.motion.yrel;
                                        yaw += yaw_motion;
                                        if (yaw < 4.75 || yaw > 7.8)
                                                //if (yaw < 0.75 * twopi || yaw > twopi + (0.25 * twopi))
                                                yaw-=yaw_motion;
                                        if (yaw > 2 * twopi) 
                                                yaw = yaw - twopi;
                                        else if (yaw < 0) 
                                                yaw = twopi + yaw;
                                        if (pitch > 2 * twopi) 
                                                pitch = pitch - twopi;
                                        else if (pitch < 0)
                                                pitch = twopi + pitch;
                                        else pitch += pitch_motion;
                                        break;
                                }
                        case SDL_KEYDOWN:
                                {
                                        if (keystate[SDLK_PLUS])
                                        {
                                                if (mode < SDL_GetNumDisplayModes(0))
                                                        mode++;
                                                else
                                                        mode = 0;
                                                new_resolution = 1;
                                                break;
                                        }
                                        if (keystate[SDLK_MINUS])
                                        {
                                                if (mode)
                                                        mode--;
                                                else
                                                        mode = SDL_GetNumDisplayModes(0);
                                                new_resolution = 1;
                                                break;
                                        }
                                        if (keystate[SDL_SCANCODE_O])
                                        {
                                                FreezeInstances();
                                        }
                                        if (keystate[SDL_SCANCODE_SPACE])
                                        {
                                                SDL_SetRelativeMouseMode((SDL_bool) !SDL_GetRelativeMouseMode());
                                        }
                                }
                        case SDL_MOUSEBUTTONUP: 
                                if (event.button.button == SDL_BUTTON_LEFT)
                                {
                                        input[LEFT_CLICK] = 0;
                                        break;
                                }
                                if (event.button.button == SDL_BUTTON_MIDDLE)
                                {
                                        input[MIDDLE_CLICK] = 0;
                                        break;
                                }
                        case SDL_KEYUP:
                                {
                                        input[FORWARD] = keystate[SDL_SCANCODE_W] ? 1 : 0;
                                        input[BACK] = keystate[SDL_SCANCODE_S] ? 1 : 0;
                                        input[RIGHT] = keystate[SDL_SCANCODE_D] ? 1 : 0;
                                        input[LEFT] = keystate[SDL_SCANCODE_A] ? 1 : 0;
                                        if (keystate[SDL_SCANCODE_ESCAPE] || keystate[SDL_SCANCODE_Q])
                                                throw std::logic_error("User quit");
                                        break;
                                }
                        case SDL_QUIT:
                                exit(0);
                }
        }

}

void Context::UpdatePosition()
{
        forward = glm::vec3( cos(yaw)*sin(pitch), cos(yaw) * cos(pitch), sin(yaw) );
        btTransform object_transform, player_transform;
        glm::vec3 left = glm::cross(forward, up);
        const float walkspeed = 8.0;
        btVector3 velocity = player->body->getLinearVelocity(), _v(0,0,0);
        if (input[LEFT])
        {
                _v += btVector3(left.x, left.y, 0.f);
        }
        if (input[FORWARD])
        {
                _v += btVector3(forward.x, forward.y, 0.f);
        }
        if (input[RIGHT])
        {
                _v -= btVector3(left.x, left.y, 0.f);
        }
        if (input[BACK])
        {
                _v -= btVector3(forward.x, forward.y, 0.f);
        }
        if (_v.length() == 0)
        {
                btTransform t;
                player->body->getMotionState()->getWorldTransform(t);
                //                if (player_grounded)
                player->body->applyForce(btVector3(velocity.x() * -25, velocity.y() * -25, 0),t.getOrigin());
        }
        else
        {
                const btVector3 normalized = _v.normalized();
                //if (player_grounded)
                velocity = btVector3(normalized.x() * walkspeed, normalized.y() * walkspeed, velocity.z());
        }
        if (input[LEFT_CLICK])
        {
                if (held_object)
                {
                        held_object->setGravity(btVector3(0,0,0));
                        player->body->getMotionState()->getWorldTransform(player_transform);
                        held_object->getMotionState()->getWorldTransform(object_transform);
                        btVector3 summon_to(forward.x, forward.y, forward.z);
                        summon_to *= 3;
                        btVector3 _vb = player_transform.getOrigin() - object_transform.getOrigin() + summon_to;
                        held_object->setLinearVelocity(_vb * 2.5);
                }
        }
        else 
        {
                if (held_object)
                {
                        held_object->setGravity(btVector3(0,0,-9.82));
                        btVector3 direction(forward.x, forward.y, forward.z);
                        direction.normalize();
                        held_object->setLinearVelocity(direction * 20.0);
                        held_object = NULL;
                }
        }
        if (input[MIDDLE_CLICK])
        {
                if (grapple_target)
                {
                        player->body->getMotionState()->getWorldTransform(player_transform);
                        btVector3 _vb = (grapple_position - player_transform.getOrigin());
                        _vb *= 2.0;
                        if (_vb.length() > 10)
                                player->body->setLinearVelocity(_vb);
                        return;
                }
                else
                        return;

        } 
        else 
                grapple_target = false;
        player->body->setLinearVelocity(velocity);
}

void Context::CollisionRoutine(void)
{
        int numManifolds = world->getDispatcher()->getNumManifolds();
        for (int i=0;i<numManifolds;i++)
        {
                btPersistentManifold* contactManifold =  world->getDispatcher()->getManifoldByIndexInternal(i);
                //const btCollisionObject* obA = contactManifold->getBody0();
                btRigidBody *_p = (btRigidBody*) contactManifold->getBody1();
                if (_p == player->body)
                {
                        btVector3 velocity = player->body->getLinearVelocity();
                        if (fabs(velocity.z()) < 0.1)
                        {
                                //printf("Player landed, velocity [%f,%f,%f]\n", velocity.x(), velocity.y(), velocity.z());
                                //player_grounded = true;
                        }

                        //if (fabs(velocity.z()) > 0.5)
                        //player_grounded = false;
                        //player_grounded = true;
                }

                int numContacts = contactManifold->getNumContacts();
                for (int j=0;j<numContacts;j++)
                {
                        btManifoldPoint& pt = contactManifold->getContactPoint(j);
                        if (pt.getDistance()<0.f)
                        {
                                /*
                                   const btVector3& ptA = pt.getPositionWorldOnA();
                                   const btVector3& ptB = pt.getPositionWorldOnB();
                                   const btVector3& normalOnB = pt.m_normalWorldOnB;
                                   */
                        }
                }
        }
}

void Context::Loop()
{
        unsigned int tick;
        while (1)
        {
                tick = SDL_GetTicks();
                world->stepSimulation(1/60.0);
                for (std::shared_ptr<Object> o : objects)
                {
                        if (strcmp(o->name, "ui_cube") == 0)
                        {
                                btTransform t;
                                player->body->getMotionState()->getWorldTransform(t);
                                o->body->setWorldTransform(t);
                                printf("GOT CUBE!#\n");
                        }
                }
                CollisionRoutine();
                PollInput();
                UpdatePosition();
                Draw();
                btRigidBody *collisionBody = FirstCollisionRayTrace(width/2, height/2);
                if (collisionBody && !held_object)
                {
                        for (std::shared_ptr<Object> o : objects)
                                for (btRigidBody *bodyInstance : o->body_instances)
                                {
                                        if (collisionBody == bodyInstance)
                                        {
                                                if (input[LEFT_CLICK] && collisionBody->getInvMass() != 0)
                                                        held_object = collisionBody;
                                                if (input[MIDDLE_CLICK])
                                                        grapple_target = collisionBody;
                                        }
                                }
                }
                if (new_resolution)
                {
                        SDL_GetDisplayMode(0, mode, &video_mode);
                        width = video_mode.w;
                        height = video_mode.h;
                        SDL_SetWindowDisplayMode(window, (const SDL_DisplayMode*) &video_mode);
                        SDL_SetWindowSize(window, width, height);
                        glViewport(0, 0, width, height);
                        printf("Changed resolution to mode %d res %d*%d\n", mode, width, height);
                        new_resolution = 0;
                }

                SDL_GL_SwapWindow(window);

                if(1000/60>=SDL_GetTicks()-tick)
                {
                        SDL_Delay(1000.0/60-(SDL_GetTicks()-tick));
                }
        }
}

int main( int argc, char *argv[] )
{
        try
        {
                Context *ctx = new Context(argc, argv);
                ctx->Loop();
        }
        catch (std::exception& e)
        {
                printf("%s\n", e.what());
        }
        return 0;
}



void renderSphere(btRigidBody* sphere);

btRigidBody *Context::FirstCollisionRayTrace(int x, int y)
{
        glm::vec4 ray_start_NDC( ((float)x/(float)width  - 0.5f) * 2.0f, ((float)y/(float)height - 0.5f) * 2.0f, -1.0, 1.0f);
        glm::vec4 ray_end_NDC( ((float)x/(float)width  - 0.5f) * 2.0f, ((float)y/(float)height - 0.5f) * 2.0f, 0.0, 1.0f);

        glm::mat4 InverseProjectionMatrix = glm::inverse(perspective);

        glm::mat4 InverseViewMatrix = glm::inverse(look);

        glm::vec4 ray_start_camera = InverseProjectionMatrix * ray_start_NDC; ray_start_camera/=ray_start_camera.w;
        glm::vec4 ray_start_world = InverseViewMatrix * ray_start_camera; ray_start_world /=ray_start_world.w;
        glm::vec4 ray_end_camera = InverseProjectionMatrix * ray_end_NDC; ray_end_camera /=ray_end_camera.w;
        glm::vec4 ray_end_world = InverseViewMatrix * ray_end_camera; ray_end_world /=ray_end_world.w;

        const float ray_travel_length = 10000.0;
        glm::vec3 out_direction(glm::normalize(ray_end_world - ray_start_world) * ray_travel_length);

        btCollisionWorld::ClosestRayResultCallback RayCallback(
                        btVector3(ray_start_world.x, ray_start_world.y, ray_start_world.z), 
                        btVector3(out_direction.x, out_direction.y, out_direction.z)
                        );
        world->rayTest(
                        btVector3(ray_start_world.x, ray_start_world.y, ray_start_world.z), 
                        btVector3(out_direction.x, out_direction.y, out_direction.z), 
                        RayCallback
                      );

        if(RayCallback.hasHit()) 
        {
                //printf("Myeuw %f %f %f\n", RayCallback.m_hitPointWorld.x(), RayCallback.m_hitPointWorld.y(), RayCallback.m_hitPointWorld.z()); 
                if (!grapple_target)
                {
                        grapple_position.setX(RayCallback.m_hitPointWorld.x());
                        grapple_position.setY(RayCallback.m_hitPointWorld.y());
                        grapple_position.setZ(RayCallback.m_hitPointWorld.z());
                }
                btRigidBody *obj = (btRigidBody*) RayCallback.m_collisionObject;
                return obj;
        }
        return NULL;
}

void Object::draw_buffer(struct draw_opts opt)
{
        if (mesh)
        {
                //btQuaternion rot = t.getRotation();
                //printf("Orientation %f %f %f\n", rot.x(), rot.y(), rot.z());
                glBindVertexArray (mesh->vao);
                glUniform4f (glGetUniformLocation(mesh->shader, "color"), (GLfloat) rgba[0], (GLfloat) rgba[1], (GLfloat) rgba[2], (GLfloat) rgba[3]);
                glUniform3f(glGetUniformLocation (mesh->shader, "light.position"), 10000, 10, 1000000);
                glUniform3f(glGetUniformLocation (mesh->shader, "light.intensities"), mesh->material->diffuse[0],mesh->material->diffuse[0],mesh->material->diffuse[0]); 
                glUniform1i(glGetUniformLocation (mesh->shader, "sky"), strcmp(name, "Sky"));
                GLint texid = mesh->hasTexture ? mesh->texture : -1;
                glUniform1i(glGetUniformLocation (mesh->shader, "texid"), texid);
                glUniform1i(glGetUniformLocation (mesh->shader, "isHighlighted"), 0);
                glUniform1f(glGetUniformLocation (mesh->shader, "light.ambientCoefficient"), 0.01);
                glUniform1f(glGetUniformLocation (mesh->shader, "materialShininess"), mesh->material->shininess);
                glUniform3f(glGetUniformLocation (mesh->shader, "materialSpecularColor"), mesh->material->specular[0],mesh->material->specular[0],mesh->material->specular[0]); 

                if (mesh->hasTexture) {
                        glEnable(GL_TEXTURE_2D);
                        glActiveTexture(GL_TEXTURE0);
                        glBindTexture(GL_TEXTURE_2D, mesh->texture);
                } else 
                {
                        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
                        glDisable(GL_TEXTURE_2D);
                }

                for (btRigidBody *b : body_instances)
                {
                        btTransform t;
                        float mat[16];
                        b->getMotionState()->getWorldTransform(t);
                        t.getOpenGLMatrix(mat);

                        glUniformMatrix4fv (glGetUniformLocation (mesh->shader, "model"), 1, GL_FALSE, mat);

                        glDrawElements(
                                        GL_TRIANGLES,
                                        (mesh->elements.size()),
                                        GL_UNSIGNED_INT,
                                        (void*)0
                                      );
                }

                if (opt.selected)
                {
                        btTransform t;
                        float mat[16];
                        opt.camera.getOpenGLMatrix(mat);

                        glUniformMatrix4fv (glGetUniformLocation (mesh->shader, "model"), 1, GL_FALSE, mat);

                        glUniform1i(glGetUniformLocation (mesh->shader, "isHighlighted"), 1);

                        glUniform4f (glGetUniformLocation(mesh->shader, "color"), 0.0, 0.0, 1.0, 1.0);

                        glDrawElements( GL_TRIANGLES, (mesh->elements.size()), GL_UNSIGNED_INT, (void*)0);
                }
                glBindVertexArray (0);
        } else {
                printf("No mesh for %s\n", name); 
        }
}

bool Context::MatchBodyWithInstanceAndMesh(btRigidBody *body, const char *name)
{
        for(Instance &i : instances)
        {
                if (strcmp(name, i.name) == 0)
                {
                        for(Mesh &m : meshes)
                        {
                                char _copyName[256];
                                strcpy(_copyName, name);
                                _copyName[strlen(name) - 4] = '\0';
                                if (strcmp(name, m.name) == 0 || strcmp(_copyName, m.name) == 0)
                                {
                                        btTransform t;
                                        t.setFromOpenGLMatrix(i.transform);

                                        float random_color[4] = { (float) rand() / RAND_MAX, (float) rand() / RAND_MAX, (float) rand() / RAND_MAX, 1.0 };
                                        std::shared_ptr<Object> object(new Object(name, body, &m, random_color));

                                        object->new_instance(world, t, 1.0 / body->getInvMass(), body);

                                        objects.push_back(object);
                                        return true;
                                }
                        }

                }
        }
        return false;
}

