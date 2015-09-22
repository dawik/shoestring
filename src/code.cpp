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
#include <fstream>
#include <iostream>
#include <unordered_map>
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

using namespace std;

char* read_file(const char* filename);
GLuint compile_shader(const char* vertex, const char* fragment);
void gl_error();

const float twopi = 6.28318530718;
const float actor_mass = 1.0;
const glm::vec3 up(0,0,-1);

enum {LEFT = 0, RIGHT = 1, FORWARD = 2, BACK = 3, LEFT_CLICK = 4, RIGHT_CLICK = 5, MIDDLE_CLICK = 6, JUMP = 7};

struct draw_opts
{
        bool textured = true;
        bool selected = false;
        btTransform camera;
};

class UI
{
        private:
                const GLfloat ui_vertex_data[18] = {
                        -1.0f, -.33f, 0.0f,
                        -1.0f, -1.0f, 0.0f,
                        1.0f, -1.0f, 0.0f,

                        1.0f, -1.0f, 0.0f,
                        1.0f, -.33f, 0.0f,
                        -1.0f, -.33f, 0.0f,

                };

                GLuint vertexbuffer, shader;
        public:
                void init()
                {
                        shader = compile_shader(read_file("src/ui.vs"), read_file("src/ui.fs"));
                        glGenBuffers(1, &vertexbuffer);
                        glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
                        glBufferData(GL_ARRAY_BUFFER, sizeof(ui_vertex_data), ui_vertex_data, GL_STATIC_DRAW);
                }

                void draw()
                {
                        glUseProgram(shader);
                        glEnableVertexAttribArray(0);
                        glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
                        glVertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);

                        glDrawArrays(GL_TRIANGLES, 0, 6);

                        glDisableVertexAttribArray(0);
                }
};

class Material
{
        public:
                string name;
                int texture;
                string bitmap_file;
                float shininess = 0.1;
                float diffuse[4] = {1,1,1,1};
                float specular[4] = {1,1,1,1};
};

class Mesh
{
        public:
                vector<float> vertexdata;
                vector<unsigned int> elements;
                unsigned int numElements, vao, vbo, ebo, material_idx;
                GLuint shader;
                string name;
                bool hasTexture;
                unsigned int texture;
                Mesh() {
                        hasTexture = false;
                };

                ~Mesh() {
                };

                void init(GLuint _shader)
                {
                        shader = _shader;
                        size_t stride = hasTexture ? sizeof(float) * 8 : sizeof(float) * 6;
                        GLint vertexAttrib = glGetAttribLocation (shader, "vertex");
                        GLint normalAttrib = glGetAttribLocation (shader, "normal");
                        GLint uvAttrib = glGetAttribLocation (shader, "uv");
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

                        glGenBuffers(1, &ebo);
                        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
                        glBufferData(GL_ELEMENT_ARRAY_BUFFER, (elements.size()) * sizeof(unsigned int), &elements[0], GL_STATIC_DRAW);

                        glBindVertexArray (0);

                };
};

struct Instance {
        public:
                Instance(const char *_name)
                        : name(string(_name))
                {
                }
                Instance(string _name)
                        : name(_name)
                {
                }
                Instance() {};
                float transform[16];
                string name;
};


class Camera
{
        public:
                Camera(string _name, 
                                float _fov, 
                                float _near, 
                                float _far, 
                                float _aspect)
                        : name(_name),
                        fov(_fov),
                        near(_near),
                        far(_far),
                        aspect(_aspect) {}
                Camera() {};
                string name = "Camera";
                float fov = 49.134342;
                float near = 0.01;
                float far = 10000.f;
                float aspect = 1.77778;
                glm::mat4 world = glm::mat4(1.0);
};

class Object
{
        public:
                string name;
                float tint[4];
                float transform[16];
                btCollisionShape *shape;
                btRigidBody *body;
                vector<btRigidBody*> bodies;
                shared_ptr<Mesh> mesh;

                Object(const char *_name, btRigidBody* _body, shared_ptr<Mesh>_mesh)
                {
                        tint[0] = 1.0;
                        tint[1] = 1.0;
                        tint[2] = 1.0;
                        tint[3] = 1.0;
                        name = string(_name);
                        shape = _body->getCollisionShape();
                        body = _body;
                        mesh = _mesh;
                }

                Object(const char *_name, btRigidBody* _body, shared_ptr<Mesh> _mesh, float _tint[4])
                {
                        tint[0] = _tint[0];
                        tint[1] = _tint[1];
                        tint[2] = _tint[2];
                        tint[3] = _tint[3];
                        name = string(_name);
                        shape = _body->getCollisionShape();
                        body = _body;
                        mesh = _mesh;
                }

                ~Object()
                {
                        delete body;
                        for (btRigidBody *b : bodies)
                        {
                                delete b;
                        }
                };

                Instance new_instance(shared_ptr<btDiscreteDynamicsWorld> world, btTransform t, btScalar mass, btRigidBody *b)
                {
                        btMotionState* motion=new btDefaultMotionState(t);
                        btVector3 inertia(0,0,0);
                        if (mass >= 0.0)
                                shape->calculateLocalInertia(mass,inertia);
                        else
                                shape->calculateLocalInertia(1.0 / body->getInvMass(),inertia);
                        btRigidBody::btRigidBodyConstructionInfo info(0,motion,shape,inertia);

                        btRigidBody *body = b ? b : new btRigidBody(info);

                        body->setMassProps(mass, inertia);
                        body->setMotionState(motion);
                        //body->setActivationState(WANTS_DEACTIVATION);

                        if (!b)
                                world->addRigidBody(body);
                        bodies.push_back(body);
                        Instance instance(name);
                        t.getOpenGLMatrix(instance.transform);
                        return instance;
                }

                void draw_buffer(struct draw_opts opt, Material *material)
                {
                        if (mesh)
                        {

                                glBindVertexArray (mesh->vao);
                                glUniform4f (glGetUniformLocation(mesh->shader, "color"), (GLfloat) tint[0], (GLfloat) tint[1], (GLfloat) tint[2], (GLfloat) tint[3]);
                                glUniform3f(glGetUniformLocation (mesh->shader, "light.position"), 10000, 10, 1000000);
                                glUniform3f(glGetUniformLocation (mesh->shader, "light.intensities"), material->diffuse[0],material->diffuse[0],material->diffuse[0]); 
                                glUniform1i(glGetUniformLocation (mesh->shader, "sky"), name == "Sky");
                                GLint texid = mesh->hasTexture ? mesh->texture : -1;
                                glUniform1i(glGetUniformLocation (mesh->shader, "texid"), texid);
                                glUniform1i(glGetUniformLocation (mesh->shader, "isHighlighted"), 0);
                                glUniform1f(glGetUniformLocation (mesh->shader, "light.ambientCoefficient"), 0.01);
                                glUniform1f(glGetUniformLocation (mesh->shader, "materialShininess"), material->shininess);
                                glUniform3f(glGetUniformLocation (mesh->shader, "materialSpecularColor"), material->specular[0],material->specular[0],material->specular[0]); 

                                if (mesh->hasTexture) {
                                        glEnable(GL_TEXTURE_2D);
                                        glActiveTexture(GL_TEXTURE0);
                                        glBindTexture(GL_TEXTURE_2D, mesh->texture);
                                } else 
                                {
                                        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
                                        glDisable(GL_TEXTURE_2D);
                                }

                                for (btRigidBody *b : bodies)
                                {
                                        btTransform t;
                                        float mat[16];
                                        b->getMotionState()->getWorldTransform(t);
                                        t.getOpenGLMatrix(mat);

                                        glUniformMatrix4fv (glGetUniformLocation (mesh->shader, "model"), 1, GL_FALSE, mat);

                                        if (name == "Sky")
                                        {
                                                glDisable(GL_CULL_FACE);
                                        }
                                        glDrawElements(
                                                        GL_TRIANGLES,
                                                        (mesh->elements.size()),
                                                        GL_UNSIGNED_INT,
                                                        (void*)0
                                                      );
                                        if (name == "Sky" == 0)
                                        {
                                                glEnable(GL_CULL_FACE);
                                        }
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
                                printf("No mesh for %s\n", name.c_str()); 
                        }
                };
};

void gl_error()
{
        GLenum error = glGetError (); 
        if (error != GL_NO_ERROR)
        {
                printf("\nglError 0x%04X\n", error);
                throw runtime_error("GL error");
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
                throw runtime_error("GLSL Compilation error");
        }
}


GLuint compile_shader(const char* vertex, const char* fragment)
{
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


class Context
{
        shared_ptr<btDiscreteDynamicsWorld> world;
        shared_ptr<btCollisionDispatcher> dispatcher;
        shared_ptr<btCollisionConfiguration> collisionConfig;
        shared_ptr<btDbvtBroadphase> broadphase;
        shared_ptr<btSequentialImpulseConstraintSolver> solver;
        vector<shared_ptr<Object>> objects;
        vector<Material> materials;
        vector<Camera> cameras;
        unordered_map<string, shared_ptr<Object>> object_map;
        unordered_map<string, shared_ptr<Mesh>> mesh_map;

        vector<Instance> instances;

        Object *actor;
        UI ui;
        glm::mat4 look, perspective;

        GLuint shader;

        int width;
        int height;
        int mode;
        int input[8];

        int current_object_idx = 0; 
        shared_ptr<Object> current_object;

        char path[256];
        float pitch; 
        float yaw;
        float near; 
        float far; 
        float fov; 
        float aspect;

        bool actor_grounded = false;

        SDL_Window *window;
        SDL_DisplayMode video_mode = { SDL_PIXELFORMAT_UNKNOWN, 0, 0, 0, 0 };

        glm::vec3 forward;
        glm::mat4 camera_rotation;

        btRigidBody *held_object = NULL;
        bool grapple_target = false;
        btVector3 grapple_position;

        btRigidBody *RayTrace(int x, int y)
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
        };

        void instancesFromGraph(struct aiNode *node, aiMatrix4x4 _transform)
        {
                if (node)
                {
                        // Transpose left-handed assimp transformation and store it along with node name
                        aiMatrix4x4 _t = node->mTransformation * _transform;
                        Instance i(node->mName.data);
                        i.transform[0] = _t.a1; i.transform[4] = _t.a2; i.transform[8] = _t.a3; i.transform[12] = _t.a4;
                        i.transform[1] = _t.b1; i.transform[5] = _t.b2; i.transform[9] = _t.b3; i.transform[13] = _t.b4;
                        i.transform[2] = _t.c1; i.transform[6] = _t.c2; i.transform[10] = _t.c3; i.transform[14] = _t.c4;
                        i.transform[3] = _t.d1; i.transform[7] = _t.d2; i.transform[11] = _t.d3; i.transform[15] = _t.d4;
                        for (int i = 0; i < node->mNumChildren; i++) {
                                instancesFromGraph(node->mChildren[i], _t);
                        }
                        instances.push_back(i);
                }
        }
        void instancesFromFile(const char *filename)
        {
                fstream file(filename,ios::binary|ios::in|ios::ate );
                if (file.is_open())
                {
                        int size = file.tellg();
                        for(int i = 0; i<=size/sizeof(Instance); i++)
                        { 
                                Instance instance;
                                file.seekg(i*sizeof(Instance));
                                file.read(reinterpret_cast<char *>(&instance),sizeof(Instance));
                                instances.push_back(instance);
                        }
                        printf("Loaded %lu instances from file\n", instances.size());
                        file.close();
                }
        }
        void instancesToFile(const char *filename)
        {
                remove(filename);
                fstream file(filename,ios::out|ios::binary|ios::app);
                for (Instance i : instances)
                {
                        file.write(reinterpret_cast<char *>(&i),sizeof(Instance));
                        printf("Wrote instance %s\n", i.name.c_str());
                }
                file.close();
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
                shader = compile_shader(read_file("src/default.vs"), read_file("src/default.fs"));

                glUseProgram(shader);
                glEnable(GL_BLEND);
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

                for (auto const &mesh : mesh_map)
                {
                        mesh.second->init(shader);
                }

                importer.FreeScene();
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
                                if (body && name)
                                {
                                        char _copyName[256];
                                        strcpy(_copyName, name);
                                        _copyName[strlen(name) - 4] = '\0';
                                        if (string(name) == mesh_map.at(name)->name || string(_copyName) == mesh_map.at(name)->name)
                                        {
                                                float random_color[4] = { (float) rand() / RAND_MAX, (float) rand() / RAND_MAX, (float) rand() / RAND_MAX, 1.0 };
                                                shared_ptr<Object> object(new Object(name, body, mesh_map.at(name), random_color));
                                                if (string(name) == mesh_map.at(name)->name)
                                                {
                                                        object_map[name] = object;
                                                        printf("Added object %s\n", name);
                                                }
                                                else
                                                {
                                                        object_map[_copyName] = object;
                                                        printf("Added object %s as copy of %s\n", name, _copyName);
                                                }
                                        }
                                }
                        }

                        for(int i=0; i < m_fileLoader->getNumConstraints(); i++)
                        {
                                btTypedConstraint*   constraint=m_fileLoader->getConstraintByIndex(i);
                                printf("  constraint type = %i\n", constraint->getConstraintType());
                                btRigidBody* body = &constraint->getRigidBodyA();
                                const char *name = m_fileLoader->getNameForPointer(body);
                                printf("Body a name %s\n", name);
                        }

                        for(Instance &i : instances)
                        {
                                btTransform t;
                                t.setFromOpenGLMatrix(i.transform);

                                if (object_map.count(i.name))
                                {
                                        object_map[i.name]->new_instance(world, t, 1.0 / object_map[i.name]->body->getInvMass(), object_map[i.name]->body);
                                        //object_map[i.name]->new_instance(world, t, 1.0 / object_map[i.name]->body->getInvMass(), NULL);
                                        printf("Added instance %s\n", i.name.c_str());
                                }
                        }

                        instances.clear();

                        {
                                Camera cam = cameras.size() ? cameras.at(0) : Camera();
                                pitch = twopi;
                                yaw = twopi;
                                fov = cam.fov;
                                aspect = cam.aspect;
                                printf("fov %f\n", fov);
                                near = cam.near;
                                far = cam.far;

                                float actor_height = 2.0;
                                float actor_radius = 2.0;

                                btCapsuleShape *actor_shape=new btCapsuleShape(actor_radius, actor_height);

                                btVector3 inertia(0,0,0);
                                actor_shape->calculateLocalInertia(actor_mass,inertia);

                                btTransform t;  

                                t.setFromOpenGLMatrix(glm::value_ptr(glm::transpose(cam.world)));
                                btMotionState* motion=new btDefaultMotionState(t);

                                btRigidBody::btRigidBodyConstructionInfo info(actor_mass,motion,actor_shape,inertia);
                                btRigidBody* actor_body=new btRigidBody(info);

                                actor_body->setCollisionFlags( btCollisionObject::CF_CHARACTER_OBJECT);
                                actor_body->setSleepingThresholds(0.0, 0.0);
                                actor_body->setAngularFactor(0.0);

                                world->addRigidBody(actor_body);

                                actor = new Object("Player", actor_body, NULL);
                        } 
                        printf("Bodies in world %d constraints in world %d\n", world->getNumCollisionObjects(), world->getNumConstraints());
                }
                else 
                {
                        throw runtime_error("Failed to physics data");
                }
        }

        void clearInstances()
        {
                for (auto &obj : object_map)
                {
                        for(vector<btRigidBody*>::iterator it = obj.second->bodies.begin() + 1; 
                                        it != obj.second->bodies.end(); ++it) 
                        {
                                world->removeRigidBody(*it);
                        }
                        obj.second->bodies.erase(obj.second->bodies.begin() + 1, obj.second->bodies.end());
                }
                instances.clear();
                printf("Cleared instances, remaining bodies in world %d\n", world->getNumCollisionObjects());
        }

        void serializeBullet(const char *file)
        {
                btDefaultSerializer* serializer = new btDefaultSerializer();
                for (int i=0; i < world->getNumCollisionObjects(); i++)
                {
                        btCollisionObject* obj = world->getCollisionObjectArray()[i];
                        btRigidBody* body = btRigidBody::upcast(obj);
                        if (body && body->getMotionState())
                        {
                                for (shared_ptr<Object> object : objects)
                                {
                                        for (btRigidBody *instance : object->bodies)
                                        {
                                                if (instance == body)
                                                {
                                                        printf("Serialized object %d %s\n", i, object->name.c_str());
                                                        serializer->registerNameForPointer(obj, object->name.c_str());
                                                }
                                        }
                                }
                        }
                }
                world->serialize(serializer);

                FILE *fp = fopen(file,"wb");
                fwrite(serializer->getBufferPointer(),serializer->getCurrentBufferSize(), 1, fp);
                fclose(fp);
                delete serializer;
        }

        unsigned int loadtexture(char *file)
        {
                unsigned int texture;
                unsigned char *image;
                int w, h, n, intfmt = 0, fmt = 0;

                image = stbi_load(file, &w, &h, &n, 0); 

                if (!image) {
                        fprintf(stderr, "cannot load texture '%s'\n", file);
                        return 0;
                } else {
                        printf("%s w:%d h:%d comp:%d\n", file, w, h, n);
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
        };

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
        };

        void AddMesh(const aiMesh *_mesh)
        {
                shared_ptr<Mesh> mesh(new Mesh());
                mesh->name = string(_mesh->mName.C_Str());
                mesh->numElements = _mesh->mNumFaces * 3;
                for (unsigned int j = 0; j < _mesh->mNumVertices; j++)
                {
                        mesh->vertexdata.push_back(_mesh->mVertices[j].x);
                        mesh->vertexdata.push_back(_mesh->mVertices[j].y);
                        mesh->vertexdata.push_back(_mesh->mVertices[j].z);

                        if (_mesh->mNormals)
                        {
                                mesh->vertexdata.push_back(_mesh->mNormals[j].x);
                                mesh->vertexdata.push_back(_mesh->mNormals[j].y);
                                mesh->vertexdata.push_back(_mesh->mNormals[j].z);
                        }

                        if (_mesh->mTextureCoords[0])
                        {
                                mesh->vertexdata.push_back(_mesh->mTextureCoords[0][j].x);
                                mesh->vertexdata.push_back(_mesh->mTextureCoords[0][j].y);
                                mesh->hasTexture = true;
                        }
                }

                for (unsigned int j = 0; j < _mesh->mNumFaces; j++)
                {
                        mesh->elements.push_back(_mesh->mFaces[j].mIndices[0]);
                        mesh->elements.push_back(_mesh->mFaces[j].mIndices[1]);
                        mesh->elements.push_back(_mesh->mFaces[j].mIndices[2]);
                }

                if (mesh->hasTexture)
                {
                        mesh->material_idx = _mesh->mMaterialIndex;
                        mesh->texture = materials.at(_mesh->mMaterialIndex).texture;
                }

                mesh_map[string(_mesh->mName.C_Str())] = mesh;

                if (_mesh->HasBones())
                {
                        printf("Mesh %s has bones\n", _mesh->mName.C_Str());
                }
        };

        void AddMaterial(const struct aiMaterial *_material)
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
                        material.bitmap_file = string(filename);
                        bool duplicateTexture = false;
                        for(Material &m : materials)
                        {
                                if (strcmp(filename, m.bitmap_file.c_str()) == 0)
                                {
                                        material.texture = m.texture;
                                        duplicateTexture = true;
                                        break;
                                }
                        }
                        if (!duplicateTexture)
                        {
                                material.texture = loadtexture(filename);
                        }
                }
                else
                        material.texture = -1;
                aiString _n;
                _material->Get(AI_MATKEY_NAME, _n);
                _material->Get(AI_MATKEY_COLOR_DIFFUSE,material.diffuse);
                _material->Get(AI_MATKEY_COLOR_SPECULAR,material.specular);
                _material->Get(AI_MATKEY_SHININESS,material.shininess);
                material.name = string(_n.data);
                materials.push_back(material);
        }

        void AssetsFromScene(const struct aiScene *scene)
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
                        Camera camera(string(scene->mCameras[i]->mName.C_Str()),
                                        scene->mCameras[i]->mHorizontalFOV * 57.2957795,
                                        scene->mCameras[i]->mClipPlaneNear,
                                        scene->mCameras[i]->mClipPlaneFar,
                                        scene->mCameras[i]->mAspect);
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
                }
        };

        void FreezeInstances()
        {
                for (shared_ptr<Object> object : objects)
                {
                        for (btRigidBody *instance : object->bodies)
                        {
                                instance->setGravity(btVector3(0,0,0));
                                instance->setActivationState(ISLAND_SLEEPING);
                        }
                }
        }

        void Draw()
        {
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                glClearColor(0,0,1,0.5);
                float mat[16];

                btTransform t = actor->body->getWorldTransform();
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

                glEnable(GL_CULL_FACE);
                glEnable(GL_DEPTH_TEST);
                glDepthMask(GL_TRUE);
                struct draw_opts opt;
                const float distance = 10.0;
                t.setIdentity();
                t.setOrigin(actor->body->getWorldTransform().getOrigin());
                t.setOrigin(t.getOrigin() + btVector3(forward.x * distance, forward.y * distance, forward.z * distance));
                t.setOrigin(btVector3(round(t.getOrigin().x()),round(t.getOrigin().y()), round(t.getOrigin().z())));
                opt.camera = t;
                Material defaultMaterial;
                for (const auto &object : object_map)
                {
                        opt.selected = false;
                        if (object.second->mesh->hasTexture)
                                object.second->draw_buffer(opt, &materials.at(object.second->mesh->material_idx));
                        else
                                object.second->draw_buffer(opt, &defaultMaterial);
                }

                {
                        opt.selected = true;
                        if (current_object)
                                current_object->draw_buffer(opt, &defaultMaterial);
                }

                //ui.draw();

                glDisable(GL_CULL_FACE);
                glDisable(GL_DEPTH_TEST);
                glDisable(GL_TEXTURE_2D);
        }

        void PollInput()
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
                                                t.setOrigin(actor->body->getWorldTransform().getOrigin());

                                                const float radius = 10;
                                                t.setOrigin(t.getOrigin() + (btVector3(forward.x, forward.y, forward.z) * radius));
                                                t.setOrigin(btVector3(round(t.getOrigin().x()),round(t.getOrigin().y()), round(t.getOrigin().z())));
                                                Instance instance = current_object->new_instance(world, t, 1.0 / current_object->body->getInvMass(), NULL);
                                                instances.push_back(instance);
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
                                                        //if (actor_grounded)
                                                        {
                                                                btVector3 velocity = actor->body->getLinearVelocity();
                                                                velocity.setZ(10);
                                                                actor->body->setLinearVelocity(velocity);
                                                                //actor_grounded = false;
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
                                                // Limit yaw to avoid inverting up and down orientation
                                                if (yaw < 4.75 || yaw > 7.8)
                                                        yaw-=yaw_motion;
                                                if (pitch > 2 * twopi) 
                                                        pitch = pitch - twopi;
                                                else if (pitch < twopi)
                                                        pitch = twopi + pitch;
                                                else pitch += pitch_motion;
                                                break;
                                        }
                                case SDL_KEYDOWN:
                                        {
                                                if (keystate[SDLK_PLUS])
                                                {
                                                        current_object_idx++;
                                                        if (current_object_idx + 1 > object_map.size())
                                                                current_object_idx = 0;
                                                        break;
                                                }
                                                if (keystate[SDL_SCANCODE_MINUS])
                                                {
                                                        current_object_idx--;
                                                        if (current_object_idx < 0)
                                                                current_object_idx = object_map.size() - 1;
                                                        break;
                                                }
                                                if (keystate[SDL_SCANCODE_I])
                                                {
                                                        clearInstances();
                                                }
                                                if (keystate[SDL_SCANCODE_O])
                                                {
                                                        instancesToFile("instances.dat");
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
                                                        throw logic_error("User quit");
                                                break;
                                        }
                                case SDL_QUIT:
                                        exit(0);
                        }
                }

        }

        void UpdatePosition()
        {
                forward = glm::vec3( cos(yaw)*sin(pitch), cos(yaw) * cos(pitch), sin(yaw) );
                btTransform object_transform, actor_transform;
                glm::vec3 left = glm::cross(forward, up);
                const float walkspeed = 8.0;
                btVector3 velocity = actor->body->getLinearVelocity(), _v(0,0,0);
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
                        actor->body->getMotionState()->getWorldTransform(t);
                        actor->body->applyForce(btVector3(velocity.x() * -25, velocity.y() * -25, 0),t.getOrigin());
                }
                else
                {
                        const btVector3 normalized = _v.normalized();
                        velocity = btVector3(normalized.x() * walkspeed, normalized.y() * walkspeed, velocity.z());
                }
                if (input[LEFT_CLICK])
                {
                        if (held_object)
                        {
                                actor->body->getMotionState()->getWorldTransform(actor_transform);
                                held_object->getMotionState()->getWorldTransform(object_transform);
                                btVector3 summon_to(forward.x, forward.y, forward.z);
                                summon_to *= 3;
                                btVector3 _vb = actor_transform.getOrigin() - object_transform.getOrigin() + summon_to;
                                held_object->setLinearVelocity(_vb * 2.5);
                        }
                }
                else 
                {
                        if (held_object)
                        {
                                btVector3 direction(forward.x, forward.y, forward.z);
                                direction.normalize();
                                const float throwVelocity = 40.f;
                                held_object->setLinearVelocity(direction * throwVelocity);
                                held_object = NULL;
                        }
                }
                if (input[MIDDLE_CLICK])
                {
                        if (grapple_target)
                        {
                                actor->body->getMotionState()->getWorldTransform(actor_transform);
                                btVector3 _vb = (grapple_position - actor_transform.getOrigin());
                                _vb *= 2.0;
                                if (_vb.length() > 10)
                                        actor->body->setLinearVelocity(_vb);
                                return;
                        }
                        else
                                return;

                } 
                else 
                        grapple_target = false;
                actor->body->setLinearVelocity(velocity);
        }

        void CollisionRoutine(void)
        {
                int numManifolds = world->getDispatcher()->getNumManifolds();
                for (int i=0;i<numManifolds;i++)
                {
                        btPersistentManifold* contactManifold =  world->getDispatcher()->getManifoldByIndexInternal(i);
                        //const btCollisionObject* obA = contactManifold->getBody0();
                        btRigidBody *_p = (btRigidBody*) contactManifold->getBody1();
                        if (_p == actor->body)
                        {
                                btVector3 velocity = actor->body->getLinearVelocity();
                                if (fabs(velocity.z()) < 0.1)
                                {
                                        printf("Player landed, velocity [%f,%f,%f]\n", velocity.x(), velocity.y(), velocity.z());
                                        //actor_grounded = true;
                                }
                        }

                        int numContacts = contactManifold->getNumContacts();
                        for (int j=0;j<numContacts;j++)
                        {
                                btManifoldPoint& pt = contactManifold->getContactPoint(j);
                                if (pt.getDistance()<0.f)
                                {
                                        //const btVector3& ptA = pt.getPositionWorldOnA();
                                        //const btVector3& ptB = pt.getPositionWorldOnB();
                                        //const btVector3& normalOnB = pt.m_normalWorldOnB;
                                }
                        }
                }
        }

        public: 
        Context(int argc, char **)
        {
                srand(time(NULL));

                Init_SDL();

                Init_GL();

                Init_Bullet();

                ui.init();

                // Note: Mesh needs to be -Y forward +Z up, 
                // to align properly with collision shapes from Bullet
                const char *scene = "assets/sandbox.fbx", *physics = "assets/sandbox.bullet";

                // Save the path to use when loading textures
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

                instancesFromFile("instances.dat");

                for(Instance &i : instances)
                {
                        btTransform t;
                        t.setFromOpenGLMatrix(i.transform);

                        if (object_map.count(i.name))
                        {
                                object_map[i.name]->new_instance(world, t, 1.0 / object_map[i.name]->body->getInvMass(), NULL);
                                printf("Added instance %s\n", i.name.c_str());
                        }
                }
        }
        void Loop()
        {
                unsigned int tick;
                while (1)
                {
                        tick = SDL_GetTicks();
                        world->stepSimulation(1/60.0);
                        CollisionRoutine();
                        PollInput();
                        UpdatePosition();
                        Draw();
                        btRigidBody *collisionBody = RayTrace(width/2, height/2);
                        if (collisionBody && !held_object)
                        {
                                for (auto &o : object_map)
                                        for (btRigidBody *bodyInstance : o.second->bodies)
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
                        int i = 0;
                        for (auto &o : object_map)
                        {
                                if (i++ == current_object_idx)
                                {
                                        current_object = o.second;
                                        break;
                                }
                        }

                        SDL_GL_SwapWindow(window);

                        if(1000/60>=SDL_GetTicks()-tick)
                        {
                                SDL_Delay(1000.0/60-(SDL_GetTicks()-tick));
                        }
                }
        }

        ~Context()
        {
                delete actor;

                for (int i=world->getNumCollisionObjects()-1; i>=0 ;i--)
                {
                        btCollisionObject* obj = world->getCollisionObjectArray()[i];
                        btRigidBody* body = btRigidBody::upcast(obj);
                        if (body && body->getMotionState())
                        {
                                delete body->getMotionState();
                        }
                        world->removeCollisionObject( obj );
                        delete obj;
                }
                SDL_Quit();
        }

};

int main( int argc, char *argv[] )
{
        try
        {
                Context *ctx = new Context(argc, argv);
                ctx->Loop();
        }
        catch (exception &e)
        {
                printf("%s\n", e.what());
        }
        return 0;
}



