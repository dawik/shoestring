#define NO_SDL_GLEXT
#include <SDL/SDL.h>
#include <GL/glew.h>
#include <SDL/SDL_opengl.h>
#include <GL/gl.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>

#include <iostream>
#include <stdio.h>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <bullet/btBulletDynamicsCommon.h>      //you may need to change this
#include "LinearMath/btSerializer.h"
#include "LinearMath/btIDebugDraw.h"
#include "btBulletFile.h"
#include "btBulletWorldImporter.h"


#ifndef GL_GENERATE_MIPMAP
#define GL_GENERATE_MIPMAP 0x8191
#endif

#ifndef GL_MULTISAMPLE
#define GL_MULTISAMPLE 0x809D
#endif

#ifndef GL_SAMPLE_ALPHA_TO_COVERAGE
#define GL_SAMPLE_ALPHA_TO_COVERAGE 0x809E
#endif

#define STBI_NO_HDR
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"


GLuint shader;

const float PI = 3.14159265359;

glm::mat4 look, perspective;

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

                char name[128];
                bool hasTexture;
                unsigned int texture;
};

class Material
{
        public:
                int index;
                int texture;
                char name[128];
};

class SimulationObject
{
        public:
                char name[128];

                float rgba[4];
                Mesh *mesh;

                btTransform transform;

                SimulationObject(const char *_name, btRigidBody* _body, Mesh* _mesh)
                {
                        rgba[0] = 1.0;
                        rgba[1] = 1.0;
                        rgba[2] = 1.0;
                        rgba[3] = 1.0;
                        strncpy(name, _name, 128);
                        body = _body;
                        mesh = _mesh;
                }

                SimulationObject(const char *n, btRigidBody *b, Mesh *m, float tint[4])
                {
                        rgba[0] = tint[0];
                        rgba[1] = tint[1];
                        rgba[2] = tint[2];
                        rgba[3] = tint[3];
                        strncpy(name, n, 128);
                        body = b;
                        mesh = m;
                }

                ~SimulationObject();

                btRigidBody *body;

                void draw_buffer();
};

SimulationObject *player;

void gl_error()
{
        GLenum error = glGetError (); 
        if (error != GL_NO_ERROR)
        {
                printf("\nglError 0x%04X\n", error);
                exit (1);
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
        if(status == GL_FALSE){
                printf("compile failed %s\n", log);
                exit(1);
        }
}


GLuint compile_shader(const char* vertex, const char* fragment)
{
        // Create the handels
        GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
        GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        GLuint programShader = glCreateProgram();

        // Attach the shaders to a program handel.
        glAttachShader(programShader, vertexShader);
        glAttachShader(programShader, fragmentShader);

        // Load and compile the Vertex Shader
        compile_shader_src(vertexShader, vertex);

        // Load and compile the Fragment Shader
        compile_shader_src(fragmentShader, fragment);

        // The shader objects are not needed any more,
        // the programShader is the complete shader to be used.
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

void Mesh::init_buffer(GLuint vertexApp)
{
        size_t stride = hasTexture ? sizeof(float) * 8 : sizeof(float) * 6;
        GLint vertexAttrib = glGetAttribLocation (vertexApp, "vertex");
        GLint normalAttrib = glGetAttribLocation (vertexApp, "normal");
        GLint uvAttrib = glGetAttribLocation (vertexApp, "uv");
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


class SimulationContext
{
        std::vector<Mesh> meshes;
        std::vector<Material> materials;

        void CollisionRoutine(void);

        unsigned int loadmaterial(struct aiMaterial *material);

        void Clone(SimulationObject *o, btVector3 position);

        void hold_object(int, int);

        float near, far, fov, aspect;

        bool player_grounded = false;

        btRigidBody *held_object = NULL;

        btRigidBody *RayTrace(int, int);

        int width, height;
        SDL_Surface *screen_surface;
        SDL_Rect **video_modes;
        int mode;
        int input[7];
        glm::vec3 forward;
        glm::mat4 camera_rotation;

        void ReadAssets(const struct aiScene*);

        char path[256];

        public: 
        float pitch, yaw;
        SimulationContext(int argc, char **argv)
        {
                if (argc != 1)
                {
                        throw std::invalid_argument("No command line interface yet\n");
                } 
                const char *scene_filename = "assets/fractured4.dae", *physics_file = "assets/fractured4.bullet";
                srand(time(NULL));
                collisionConfig=new btDefaultCollisionConfiguration();
                dispatcher=new btCollisionDispatcher(collisionConfig);
                broadphase=new btDbvtBroadphase();
                solver=new btSequentialImpulseConstraintSolver();
                world=new btDiscreteDynamicsWorld(dispatcher,broadphase,solver,collisionConfig);
                world->setGravity(btVector3(0,0,-9.82));

                SDL_Init( SDL_INIT_VIDEO );

                video_modes=SDL_ListModes(NULL, SDL_FULLSCREEN|SDL_HWSURFACE);

                mode = 1;
                width = video_modes[mode]->w; 
                height = video_modes[mode]->h; 

                screen_surface = SDL_SetVideoMode( width, height, 32, SDL_HWSURFACE | SDL_DOUBLEBUF | SDL_OPENGL);
                printf("Surface: w:%d h:%d bpp:%d\n", screen_surface->w, screen_surface->h, screen_surface->format->BitsPerPixel);
                SDL_WM_SetCaption( "", 0 );
                SDL_WM_GrabInput(SDL_GRAB_OFF);
                SDL_ShowCursor(0);
                glewExperimental = GL_TRUE;
                glewInit(); 
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_COLOR, GL_ZERO);

                for (unsigned long i = 0, sep = 0; i < strlen(scene_filename); i++)
                {
                        if (scene_filename[i] == '/')
                                sep = i;
                        if (i == strlen(scene_filename) - 1)
                        {
                                sep = sep ? sep + 1 : 0;
                                strncat(path, scene_filename, sizeof(char) * (sep));
                        }
                }

                Assimp::Importer importer;
                const struct aiScene *scene = importer.ReadFile(scene_filename,aiProcessPreset_TargetRealtime_Fast);
                printf("Loading scene from %s\nAssimp:\t%s\n", scene_filename, importer.GetErrorString());
                ReadAssets(scene);

                shader = compile_shader(read_file("src/standard.vert.glsl"), read_file("src/standard.frag.glsl"));
                glBindFragDataLocation (shader, 0, "outColor");
                glLinkProgram (shader);

                glUseProgram(shader);
                gl_error();


                for (Mesh &m : meshes)
                {
                        m.init_buffer(shader);
                }


                class btBulletWorldImporter*		m_fileLoader;
                m_fileLoader = new btBulletWorldImporter(world);

                printf("Constraint filename %s\n", physics_file);
                m_fileLoader->setVerboseMode(false);

                if (m_fileLoader->loadFile(physics_file))
                {
                        printf("deserializing bullet physics file %s\n%d\tconstraints\n%d\trigid bodies\n", 
                                        physics_file, m_fileLoader->getNumConstraints(), m_fileLoader->getNumRigidBodies());
                        for(int i=0; i < m_fileLoader->getNumRigidBodies(); i++)
                        {
                                btCollisionObject* obj = m_fileLoader->getRigidBodyByIndex(i);
                                btRigidBody* body = btRigidBody::upcast(obj);      
                                const char *name = m_fileLoader->getNameForPointer(body);
                                if (body)
                                {

                                        if (SetRigidBodyWithSceneNodeTransformation(body, name, scene) == false)
                                        {
                                                printf("Mesh %s not found\n", name);
                                        }
                                }
                        }
                        if (player)
                        {
                                float mat[16];
                                btTransform t = player->body->getWorldTransform();
                                t.getOpenGLMatrix(mat);
                                player->body->setCollisionFlags( btCollisionObject::CF_CHARACTER_OBJECT);
                                printf("Player pos adding to bullet? %f %f %f\n", t.getOrigin().getX(), t.getOrigin().getY(), t.getOrigin().getZ()); 
                                world->addRigidBody(player->body);
                                printf("Adding player\n");
                        }
                        printf("CONTEXT INITIALIZED\n");
                        for (unsigned long i = 0; i < objects.size(); i++)
                        {
                                if (strcmp(objects[i]->name, "Cube") == 0)
                                {
                                        Clone(objects[i], btVector3(0,0,10));
                                }
                        }
                }
        }
        ~SimulationContext()
        {
                SDL_Quit();
        }
        void PollInput();
        void Draw();
        void UpdatePosition();
        void PollWindow();
        void Loop();
        btDynamicsWorld *world;
        btCollisionDispatcher *dispatcher;
        btCollisionConfiguration *collisionConfig;
        btBroadphaseInterface *broadphase;
        btConstraintSolver *solver;
        std::vector<SimulationObject*> objects;
        bool SetRigidBodyWithSceneNodeTransformation(btRigidBody *body, const char *name, const struct aiScene *scene);
        void AddBodyToWorld(const char *name, btRigidBody *body, Mesh *mesh, float transform[16]);
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
        //glGenerateMipmap(GL_TEXTURE_2D);

        free(image);

        return texture;
}

unsigned int SimulationContext::loadmaterial(struct aiMaterial *material)
{
        char filename[2000];
        struct aiString str;
        if (!aiGetMaterialString(material, AI_MATKEY_TEXTURE_DIFFUSE(0), &str)) {
                char *s = strrchr(str.data, '/');
                if (!s) s = strrchr(str.data, '\\');
                if (!s) s = str.data; else s++;
                strcpy(filename, path);
                strcat(filename, str.data);
                return loadtexture(filename);
        }
        return 0;
}



void transposematrix(float m[16], aiMatrix4x4 *p)
{
        m[0] = p->a1; m[4] = p->a2; m[8] = p->a3; m[12] = p->a4;
        m[1] = p->b1; m[5] = p->b2; m[9] = p->b3; m[13] = p->b4;
        m[2] = p->c1; m[6] = p->c2; m[10] = p->c3; m[14] = p->c4;
        m[3] = p->d1; m[7] = p->d2; m[11] = p->d3; m[15] = p->d4;
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

bool contains(const char * subject, const char * pattern)
{
        unsigned int slen = strlen(subject), plen = strlen(pattern);
        for (unsigned int i = 0, j = plen; i < slen; i++)
        {
                if (subject[i] == pattern[plen - j])
                {
                        if (--j == 0)
                                return true;
                }
        }
        return false;
}

void SimulationContext::ReadAssets(const struct aiScene *scene)
{
        fprintf(stderr, "\nReading scene...\n%d\tmeshes\n%d\tmaterials\n%d\tcameras\n", scene->mNumMeshes, scene->mNumMaterials, scene->mNumCameras);
        for (unsigned int i = 0; i < scene->mNumMaterials; i++)
        {
                Material material;
                material.index = i;
                material.texture = loadmaterial(scene->mMaterials[i]);
                scene->mMaterials[i]->Get(AI_MATKEY_NAME, material.name);
                materials.push_back(material);
        }
        for (unsigned int i = 0; i < scene->mNumMeshes; i++)
        {
                Mesh mesh;
                strcpy(mesh.name, scene->mMeshes[i]->mName.C_Str());
                mesh.numElements = scene->mMeshes[i]->mNumFaces * 3;
                for (unsigned int j = 0; j < scene->mMeshes[i]->mNumVertices; j++)
                {
                        mesh.vertexdata.push_back(scene->mMeshes[i]->mVertices[j].x);
                        mesh.vertexdata.push_back(scene->mMeshes[i]->mVertices[j].y);
                        mesh.vertexdata.push_back(scene->mMeshes[i]->mVertices[j].z);

                        if (scene->mMeshes[i]->mNormals)
                        {
                                mesh.vertexdata.push_back(scene->mMeshes[i]->mNormals[j].x);
                                mesh.vertexdata.push_back(scene->mMeshes[i]->mNormals[j].y);
                                mesh.vertexdata.push_back(scene->mMeshes[i]->mNormals[j].z);
                        }

                        if (scene->mMeshes[i]->mTextureCoords[0])
                        {
                                mesh.hasTexture = true;
                                mesh.vertexdata.push_back(scene->mMeshes[i]->mTextureCoords[0][j].x);
                                mesh.vertexdata.push_back(scene->mMeshes[i]->mTextureCoords[0][j].y);
                        }
                }

                for (unsigned int j = 0; j < scene->mMeshes[i]->mNumFaces; j++)
                {
                        mesh.elements.push_back(scene->mMeshes[i]->mFaces[j].mIndices[0]);
                        mesh.elements.push_back(scene->mMeshes[i]->mFaces[j].mIndices[1]);
                        mesh.elements.push_back(scene->mMeshes[i]->mFaces[j].mIndices[2]);
                }

                if (mesh.hasTexture)
                {
                        mesh.texture = materials.at(scene->mMeshes[i]->mMaterialIndex).texture;
                }

                meshes.push_back(mesh);
        }

        assert(scene->mNumCameras == 1);
        for (unsigned int i = 0; i < scene->mNumCameras; i++)
        {
                aiCamera *camera = scene->mCameras[i];
                fov = camera->mHorizontalFOV * 57.2957795;
                near = camera->mClipPlaneNear;
                far = camera->mClipPlaneFar;
                printf("Camera near %f far %f fov %f", near, far, fov);
                far = 1000;
                aspect = camera->mAspect;
                printf("%f %f %f \n", fov, near, far);
                struct aiNode *node = findnode(scene->mRootNode, scene->mCameras[i]->mName.C_Str());
                if (node)
                {
                        float mat[16];
                        transposematrix(mat, &node->mTransformation);
                        {
                                btTransform t;  
                                t.setIdentity();
                                t.setFromOpenGLMatrix(mat);
                                pitch = 0;
                                yaw = 0;
                                float rad = 1.5, mass = 5.0;
                                btCapsuleShape *player_shape=new btCapsuleShape(rad, 2);
                                btVector3 inertia(0,0,0);
                                if(mass!=0.0)
                                        player_shape->calculateLocalInertia(mass,inertia);

                                btMotionState* motion=new btDefaultMotionState(t);
                                btRigidBody::btRigidBodyConstructionInfo info(mass,motion,player_shape,inertia);
                                btRigidBody* body=new btRigidBody(info);
                                body->forceActivationState(DISABLE_DEACTIVATION);
                                body->setSleepingThresholds(0.0, 0.0);
                                body->setAngularFactor(0.0);
                                world->addRigidBody(body);
                                player = new SimulationObject("_Sphere", body, NULL);
                        }
                } 
        }
};

void SimulationContext::Draw()
{
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glClearColor(1,0,0,1);
        float mat[16];
        btTransform t = player->body->getWorldTransform();
        t.getOpenGLMatrix(mat);

        glm::vec3 player_pos = glm::vec3(t.getOrigin().getX(), t.getOrigin().getY(), t.getOrigin().getZ());

        look =  glm::lookAt(player_pos, player_pos + forward, glm::vec3(0, 0, 1));
        perspective =  glm::perspective(fov, aspect, near, far);

        //glEnable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        for (SimulationObject *object : objects)
        {
                object->draw_buffer();
        }

        //glDisable(GL_CULL_FACE);
        glDisable(GL_TEXTURE_2D);
        glDisable(GL_DEPTH_TEST);
}

enum {LEFT = 0, RIGHT = 1, FORWARD = 2, BACK = 3, LEFT_CLICK = 4, RIGHT_CLICK = 5, JUMP = 6};

void SimulationContext::PollInput()
{
        SDL_Event event;
        while (SDL_PollEvent (&event))
        {
                Uint8 *keystate = SDL_GetKeyState(NULL);
                switch (event.type) 
                {
                        case SDL_MOUSEBUTTONDOWN:
                                switch (event.button.button)
                                {
                                        case SDL_BUTTON_WHEELUP:
                                                break;
                                        case SDL_BUTTON_WHEELDOWN:
                                                break;
                                        case SDL_BUTTON_LEFT:
                                                {
                                                        input[LEFT_CLICK] = 1;
                                                        break;
                                                }
                                        case SDL_BUTTON_MIDDLE:
                                                {
                                                        btTransform t;
                                                        player->body->getMotionState()->getWorldTransform(t);
                                                        btVector3 forward_bt = btVector3(forward.x,forward.y,forward.z);
                                                        t.setOrigin(t.getOrigin() + (forward_bt * 2.0));
                                                        float rad = 0.25, mass = 2.5;
                                                        btSphereShape* sphere=new btSphereShape(rad);
                                                        btVector3 inertia(0,0,0);
                                                        if(mass!=0.0)
                                                                sphere->calculateLocalInertia(mass,inertia);
                                                        btMotionState* motion=new btDefaultMotionState(t);
                                                        btRigidBody::btRigidBodyConstructionInfo info(mass,motion,sphere,inertia);
                                                        btRigidBody* body=new btRigidBody(info);
                                                        body->setLinearVelocity(forward_bt *= 30);
                                                        world->addRigidBody(body);
                                                        float sphere_color[4] = { 0, 1.0, 0, 1};
                                                        SimulationObject *object = new SimulationObject("_Sphere", body, NULL, sphere_color);
                                                        objects.push_back(object);
                                                        break;
                                                }

                                        case SDL_BUTTON_RIGHT:
                                                if (player_grounded)
                                                {
                                                        btVector3 velocity = player->body->getLinearVelocity();
                                                        velocity.setZ(10);
                                                        player->body->setLinearVelocity(velocity);
                                                        player_grounded = false;
                                                        break;
                                                }
                                }
                                break;

                        case SDL_MOUSEMOTION:
                                {
                                        float sensitivity = 0.0001;
                                        pitch -= sensitivity * (width/2.0 - event.motion.x);
                                        yaw += sensitivity * (height/2.0 - event.motion.y);
                                        if (pitch > 2 * PI) 
                                                pitch = pitch - (2 * PI);
                                        else if (pitch < 0)
                                                pitch = 2 * PI + pitch;
                                        if (yaw > 2 * PI) 
                                                yaw = yaw - (2 * PI);
                                        else if (yaw < 0)
                                                yaw = 2 * PI + yaw;
                                        if (event.motion.x < width && event.motion.y < height)
                                        {
                                                SDL_WarpMouse(width/2, height/2);
                                                SDL_WarpMouse(width/2, height/2);
                                        }
                                        break;
                                }
                        case SDL_MOUSEBUTTONUP: 
                                if (event.button.button == SDL_BUTTON_LEFT)
                                {
                                        input[LEFT_CLICK] = 0;
                                        break;
                                }
                        case SDL_KEYDOWN:
                                {
                                        if (keystate[SDLK_PLUS])
                                        {
                                                if (!mode)
                                                        while (video_modes[mode+1])
                                                                mode++;
                                                else
                                                        mode--;
                                        }
                                        if (keystate[SDLK_MINUS])
                                                mode = video_modes[mode+1] ? mode + 1 : 0;
                                }
                        case SDL_KEYUP:
                                {
                                        input[FORWARD] = keystate[SDLK_w] ? 1 : 0;
                                        input[BACK] = keystate[SDLK_s] ? 1 : 0;
                                        input[RIGHT] = keystate[SDLK_d] ? 1 : 0;
                                        input[LEFT] = keystate[SDLK_a] ? 1 : 0;
                                        if (keystate[SDLK_ESCAPE] || keystate[SDLK_q])
                                                throw std::runtime_error("Simulation ended");
                                        break;
                                }
                        case SDL_VIDEORESIZE:
                                {
                                        break;
                                }
                        case SDL_QUIT:
                                exit(0);
                }
        }

}

void SimulationContext::UpdatePosition()
{
        forward = glm::vec3( cos(yaw)*sin(pitch), cos(yaw) * cos(pitch), sin(yaw) );
        glm::vec3 right = glm::cross(forward, glm::vec3(0, 0, 1));
        const float walkspeed = 8.0;
        btVector3 velocity = player->body->getLinearVelocity(), _v(0,0,0);
        if (input[LEFT])
        {
                _v -= btVector3(right.x, right.y, 0.f);
        }
        if (input[FORWARD])
        {
                _v += btVector3(forward.x, forward.y, 0.f);
        }
        if (input[RIGHT])
        {
                _v += btVector3(right.x, right.y, 0.f);
        }
        if (input[BACK])
        {
                _v -= btVector3(forward.x, forward.y, 0.f);
        }
        if (_v.length() == 0)
        {
                btTransform t;
                player->body->getMotionState()->getWorldTransform(t);
                player->body->applyForce(btVector3(velocity.x() * -25, velocity.y() * -25, 0),t.getOrigin());
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
                        btTransform tb, tp;
                        player->body->getMotionState()->getWorldTransform(tp);
                        held_object->getMotionState()->getWorldTransform(tb);
                        btVector3 _vb = tp.getOrigin() - tb.getOrigin() + (btVector3(forward.x, forward.y, forward.z) * 4);
                        held_object->setLinearVelocity(_vb * 5.0);
                }
        }
        else {
                held_object = NULL;
        }

        player->body->applyCentralForce(velocity);
        player->body->setLinearVelocity(velocity);
}

void SimulationContext::CollisionRoutine(void)
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
                        if (fabs(velocity.z()) < 0.1 && !player_grounded)
                        {
                                //printf("Player landed, velocity [%f,%f,%f]\n", velocity.x(), velocity.y(), velocity.z());
                                player_grounded = true;
                        }

                        if (fabs(velocity.z()) > 0.5)
                                player_grounded = false;
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

void SimulationContext::Loop()
{
        while (1)
        {
                unsigned int start;
                start = SDL_GetTicks();
                world->stepSimulation(1/60.0);
                CollisionRoutine();
                PollInput();
                UpdatePosition();
                Draw();
                btRigidBody *collisionBody = RayTrace(width/2, height/2);
                if (collisionBody && !held_object)
                {
                        for (SimulationObject *o : objects)
                                if (collisionBody == o->body && collisionBody->getInvMass() != 0)
                                        held_object = collisionBody;

                }
                if (width != video_modes[mode]->w && height != video_modes[mode]->h)
                {
                        SDL_FreeSurface(screen_surface);
                        width = video_modes[mode]->w;
                        height = video_modes[mode]->h;
                        screen_surface = SDL_SetVideoMode( width, height, 32, SDL_HWSURFACE | SDL_DOUBLEBUF | SDL_OPENGL | SDL_VIDEORESIZE);
                        glViewport(0, 0, width, height);
                        printf("Changed resolution to %d*%d\n", width, height);
                }

                SDL_GL_SwapBuffers();

                if(1000/60>=SDL_GetTicks()-start)
                {
                        SDL_Delay(1000.0/60-(SDL_GetTicks()-start));
                }
        }
}

int main( int argc, char *argv[] )
{
        try
        {
                SimulationContext *ctx = new SimulationContext(argc, argv);
                ctx->Loop();
        }
        catch (std::exception& e)
        {
                printf("%s\n", e.what());
        }
        return 0;
}



void renderSphere(btRigidBody* sphere);

btRigidBody *SimulationContext::RayTrace(int x, int y)
{
        glm::vec4 lRayStart_NDC(
                        ((float)x/(float)width  - 0.5f) * 2.0f,
                        ((float)y/(float)height - 0.5f) * 2.0f,
                        -1.0,
                        1.0f
                        );
        glm::vec4 lRayEnd_NDC(
                        ((float)x/(float)width  - 0.5f) * 2.0f,
                        ((float)y/(float)height - 0.5f) * 2.0f,
                        0.0,
                        1.0f
                        );

        glm::mat4 InverseProjectionMatrix = glm::inverse(perspective);

        glm::mat4 InverseViewMatrix = glm::inverse(look);

        glm::vec4 lRayStart_camera = InverseProjectionMatrix * lRayStart_NDC;    lRayStart_camera/=lRayStart_camera.w;
        glm::vec4 lRayStart_world  = InverseViewMatrix       * lRayStart_camera; lRayStart_world /=lRayStart_world .w;
        glm::vec4 lRayEnd_camera   = InverseProjectionMatrix * lRayEnd_NDC;      lRayEnd_camera  /=lRayEnd_camera  .w;
        glm::vec4 lRayEnd_world    = InverseViewMatrix       * lRayEnd_camera;   lRayEnd_world   /=lRayEnd_world   .w;

        glm::vec3 lRayDir_world(lRayEnd_world - lRayStart_world);
        lRayDir_world = glm::normalize(lRayDir_world);

        glm::vec3 out_direction = lRayDir_world * 10000.f;

        btCollisionWorld::ClosestRayResultCallback RayCallback(
                        btVector3(lRayStart_world.x, lRayStart_world.y, lRayStart_world.z), 
                        btVector3(out_direction.x, out_direction.y, out_direction.z)
                        );
        world->rayTest(
                        btVector3(lRayStart_world.x, lRayStart_world.y, lRayStart_world.z), 
                        btVector3(out_direction.x, out_direction.y, out_direction.z), 
                        RayCallback
                      );

        if(RayCallback.hasHit()) 
        {
                btRigidBody *obj = (btRigidBody*) RayCallback.m_collisionObject;
                return obj;
        }
}

void SimulationObject::draw_buffer()
{
        if (mesh)
        {
                btTransform t;
                float mat[16];
                body->getMotionState()->getWorldTransform(t);
                t.getOpenGLMatrix(mat);
                glBindVertexArray (mesh->vao);
                glUniform4f (glGetUniformLocation(shader, "color"), (GLfloat) rgba[0], (GLfloat) rgba[1], (GLfloat) rgba[2], (GLfloat) rgba[3]);
                glUniformMatrix4fv (glGetUniformLocation (shader, "model"), 1, GL_FALSE, mat);
                glUniformMatrix4fv (glGetUniformLocation (shader, "camera"), 1, GL_FALSE, glm::value_ptr(look));
                glUniformMatrix4fv (glGetUniformLocation (shader, "perspective"), 1, GL_FALSE, glm::value_ptr(perspective));
                glUniform3f (glGetUniformLocation (shader, "cameraPosition"), player->transform.getOrigin().x(), 
                                player->transform.getOrigin().y(), 
                                player->transform.getOrigin().z()); 
                glUniform3f(glGetUniformLocation (shader, "light.position"), 100, 100, 100);
                glUniform3f(glGetUniformLocation (shader, "light.intensities"), 1, 1, 1);
                glUniform1i(glGetUniformLocation (shader, "sky"), strcmp(name, "sky"));
                glUniform1i(glGetUniformLocation (shader, "texid"), mesh->texture);
                glUniform1f(glGetUniformLocation (shader, "light.ambientCoefficient"), 0.5);
                glUniform1f(glGetUniformLocation (shader, "materialShininess"), 0.25);
                glUniform3f(glGetUniformLocation (shader, "materialSpecularColor"), 0.5, 0.5, 0.5);

                if (mesh->hasTexture) {
                        glEnable(GL_TEXTURE_2D);
                        glActiveTexture(GL_TEXTURE0);
                        glBindTexture(GL_TEXTURE_2D, mesh->texture);
                } else 
                {
                        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
                        glDisable(GL_TEXTURE_2D);
                }

                glDrawElements(
                                GL_TRIANGLES,
                                (mesh->elements.size()),
                                GL_UNSIGNED_INT,
                                (void*)0
                              );
                glBindVertexArray (0);
        } else {
                printf("No mesh to draw for %s\n", name); 
        }
}

void SimulationContext::AddBodyToWorld(const char *name, btRigidBody *body, Mesh *mesh, float transform[16])
{
        btScalar mass = 1.0 / body->getInvMass();
        btTransform t;
        t.setFromOpenGLMatrix(transform);

        btMotionState* motion=new btDefaultMotionState(t);
        btVector3 inertia(0,0,0);
        body->getCollisionShape()->calculateLocalInertia(mass,inertia);
        btRigidBody::btRigidBodyConstructionInfo info(0,motion,body->getCollisionShape(),inertia);

        {
                btRigidBody *_body=new btRigidBody(info);
                world->removeRigidBody(body);
                _body->setMassProps(mass, inertia);
                if (contains(name, "cell") == true)
                {
                        _body->setSleepingThresholds(10.10,0);
                        _body->forceActivationState(ISLAND_SLEEPING);
                }
                world->addRigidBody(_body);
                float random_color[4] = { (float) rand() / RAND_MAX, (float) rand() / RAND_MAX, (float) rand() / RAND_MAX, 1.0 };
                SimulationObject *object = new SimulationObject(name, _body, mesh, random_color);
                objects.push_back(object);
        }
}

void SimulationContext::Clone(SimulationObject *object, btVector3 position)
{
        btScalar mass = 1.0 / object->body->getInvMass();
        btTransform t;
        t.setIdentity();
        t.setOrigin(position);

        btMotionState* motion=new btDefaultMotionState(t);
        btVector3 inertia(0,0,0);
        object->body->getCollisionShape()->calculateLocalInertia(mass,inertia);
        btRigidBody::btRigidBodyConstructionInfo info(0,motion,object->body->getCollisionShape(),inertia);
        {
                btRigidBody *_body=new btRigidBody(info);
                _body->setMassProps(mass, inertia);
                //_body->setSleepingThresholds(10.10,0);
                if (contains(object->name, "cell") == true)
                        _body->forceActivationState(ISLAND_SLEEPING);
                world->addRigidBody(_body);
                float random_color[4] = { (float) rand() / RAND_MAX, (float) rand() / RAND_MAX, (float) rand() / RAND_MAX, 1.0 };
                char newname[256];
                strcpy(newname, object->name);
                strcat(newname, "_clone");
                SimulationObject *clone = new SimulationObject(newname, _body, object->mesh, random_color);
                objects.push_back(clone);
        }
        printf("Added clone of %s\n", object->name);
}

bool SimulationContext::SetRigidBodyWithSceneNodeTransformation(btRigidBody *body, const char *name, const struct aiScene *scene)
{
        struct aiNode *node = findnode(scene->mRootNode, name);
        float transform[16];
        char _b[256];
        if (!node)
        {
                strcpy(_b, name);
                for (unsigned long i = 0; i < strlen(_b); i++)
                {
                        _b[i] = _b[i] == '.' ? '_' : _b[i];
                }
                node = findnode(scene->mRootNode, _b);
        } 

        if (node)
        {
                for(Mesh &m : meshes)
                {
                        if (strcmp(name, m.name) == 0)
                        {
                                transposematrix(transform, &node->mTransformation);
                                AddBodyToWorld(name, body, &m, transform);
                                return true;
                        }
                }
        }
        return false;
}
