#define NO_SDL_GLEXT
#include <GL/glew.h>
#include <GL/gl.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <stdio.h>
#include <memory>
#include <vector>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <btBulletDynamicsCommon.h>
#include <LinearMath/btSerializer.h>
#include <LinearMath/btIDebugDraw.h>
#include <btBulletFile.h>
#include <btBulletWorldImporter.h>

#include "glstuff.h"
#include "text.h"

/*****************************************************************************************
 * Orthodox C++                                                                          *
 ****************************************************************************************/

using namespace std;
using namespace glm;

static const float TWOPI = 6.28318530718;
static const float playerMass = 1.0;
static const float playerHeight = 2.0;
static const float playerRadius = 2.0;
static const float movementSpeed = 8.0;
static const float breakFactor = -25.0;
static const vec3 up(0,0,-1);
static atlas *texture_atlas;

class Context;
class Material;
class Mesh;
class Object;
class Camera;

enum {LEFT = 0, RIGHT = 1, FORWARD = 2, BACK = 3, LEFT_CLICK = 4, RIGHT_CLICK = 5, MIDDLE_CLICK = 6, JUMP = 7};

struct draw_opts
{
        bool textured = true;
        bool selected = false;
        btTransform camera;
};

class Material
{
        friend Object;
        friend Context;
        private:
        string name;
        int texture;
        string bitmap_file;
        float shininess = 0.1;
        float diffuse[4] = {1,1,1,1};
        float specular[4] = {1,1,1,1};
};

class Mesh
{
        friend Object;
        friend Context;
        private:
        vector<float> vertexdata;
        vector<unsigned int> elements;
        unsigned int numElements, vao, vbo, ebo, material_idx;
        GLuint shader;
        bool hasTexture, hasAnimations;
        unsigned int texture;
        public:
        Mesh() : hasTexture(false), hasAnimations(false) {};

        ~Mesh() {};

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
        friend Object;
        friend Context;
        private:
        float transform[16];
        string name;
        public:
        Instance(const char *_name) : name(string(_name)) {};
        Instance(string _name) : name(_name) {};
        Instance() {};
};


class Camera
{
        friend Context;
        private:
        string name = "Camera";
  
        float fov = 49.134342;
        float near = 0.01;
        float far = 10000.f;
        float aspect = 1.77778;
        mat4 world = mat4(1.0);

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
};

class Object
{
        friend Context;

        private:
        string name;
        float tint[4];
        btCollisionShape *shape;
        btRigidBody *body;
        vector<btRigidBody*> bodies;
        shared_ptr<Mesh> mesh;
        btTransform t;
        float mat[16];

        public:
        Object(const char *_name, btRigidBody* _body, shared_ptr<Mesh>_mesh)
                : name(string(_name)),
                body(_body),
                mesh(_mesh)
        {
                tint[0] = 1.0;
                tint[1] = 1.0;
                tint[2] = 1.0;
                tint[3] = 1.0;
                shape = _body->getCollisionShape();
        }

        Object(const char *_name, btRigidBody* _body, shared_ptr<Mesh> _mesh, float _tint[4])
                : name(string(_name)),
                body(_body),
                mesh(_mesh)
        {
                tint[0] =_tint[0];
                tint[1] =_tint[1];
                tint[2] =_tint[2];
                tint[3] =_tint[3];
                shape = _body->getCollisionShape();
        }


        ~Object()
        {
                delete body;
                for (btRigidBody *b : bodies)
                {
                        delete b;
                }
                delete shape;
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
                        glUniform1i(glGetUniformLocation (mesh->shader, "texid"), mesh->hasTexture ? mesh->texture : 0);
                        glUniform1i(glGetUniformLocation (mesh->shader, "isHighlighted"), 0);
                        glUniform1f(glGetUniformLocation (mesh->shader, "light.ambientCoefficient"), 0.01);
                        glUniform1f(glGetUniformLocation (mesh->shader, "materialShininess"), material->shininess);
                        glUniform3f(glGetUniformLocation (mesh->shader, "materialSpecularColor"), material->specular[0],material->specular[0],material->specular[0]); 

                        if (mesh->hasTexture) 
                        {
                                glEnable(GL_TEXTURE_2D);
                                glActiveTexture(GL_TEXTURE0);
                                glBindTexture(GL_TEXTURE_2D, mesh->texture);
                        } 
                        else 
                        {
                                glDisableClientState(GL_TEXTURE_COORD_ARRAY);
                                glDisable(GL_TEXTURE_2D);
                        }

                        for (btRigidBody *b : bodies)
                        {
                                b->getMotionState()->getWorldTransform(t);
                                t.getOpenGLMatrix(mat);

                                glUniformMatrix4fv (glGetUniformLocation (mesh->shader, "model"), 1, GL_FALSE, mat);

                                if (name == "Sky")
                                {
                                        glDisable(GL_CULL_FACE);
                                }
                                glDrawElements( GL_TRIANGLES, (mesh->elements.size()), GL_UNSIGNED_INT, NULL);
                                if (name == "Sky" == 0)
                                {
                                        glEnable(GL_CULL_FACE);
                                }
                        }

                        if (opt.selected)
                        {
                                opt.camera.getOpenGLMatrix(mat);

                                glUniformMatrix4fv (glGetUniformLocation (mesh->shader, "model"), 1, GL_FALSE, mat);
                                glUniform1i(glGetUniformLocation (mesh->shader, "isHighlighted"), 1);
                                glUniform4f (glGetUniformLocation(mesh->shader, "color"), 0.0, 0.0, 1.0, 1.0);
                                glDrawElements( GL_TRIANGLES, (mesh->elements.size()), GL_UNSIGNED_INT, NULL);
                        }
                        glBindVertexArray (0);
                } else {
                        printf("No mesh for %s\n", name.c_str()); 
                }
        };
};

class Context
{
        private:
                const char *scene_file = "assets/sandbox.obj", *bullet_file = "assets/sandbox.bullet";
                btBulletWorldImporter*		m_fileLoader;
                Assimp::Importer importer;
                shared_ptr<btDiscreteDynamicsWorld> world;
                shared_ptr<btCollisionDispatcher> dispatcher;
                shared_ptr<btCollisionConfiguration> collisionConfig;
                shared_ptr<btDbvtBroadphase> broadphase;
                shared_ptr<btSequentialImpulseConstraintSolver> solver;

                vector<Material> materials;
                vector<Camera> cameras;
                unordered_map<string, shared_ptr<Mesh>> meshes;

                unordered_map<string, shared_ptr<Object>> objects;
                vector<Instance> addedInstances;

                SDL_Window *window;
                SDL_DisplayMode displayMode = { SDL_PIXELFORMAT_UNKNOWN, 0, 0, 0, 0 };

                int screenWidth;
                int screenHeight;
                int videoMode;

                Object *player;
                float mouseSensitivity = 0.005;
                float playerPitch; 
                float playerYaw;
                //bool player_grounded = false;

                int playerInput[8];

                int createObjIdx = 0; 
                shared_ptr<Object> createObj;

                btRigidBody *heldObject = NULL;

                bool grappleTarget = false;
                btVector3 grapplePos;

                mat4 look, projection;
                vec3 eye, forward;

                GLuint staticShader;
                unsigned int tick;

                btRigidBody *RayTrace(int x, int y)
                {
                        vec4 ray_start_NDC( ((float)x/(float)screenWidth  - 0.5f) * 2.0f, ((float)y/(float)screenHeight - 0.5f) * 2.0f, -1.0, 1.0f);
                        vec4 ray_end_NDC( ((float)x/(float)screenWidth  - 0.5f) * 2.0f, ((float)y/(float)screenHeight - 0.5f) * 2.0f, 0.0, 1.0f);

                        mat4 InverseProjectionMatrix = inverse(projection);

                        mat4 InverseViewMatrix = inverse(look);

                        vec4 ray_start_camera = InverseProjectionMatrix * ray_start_NDC; ray_start_camera/=ray_start_camera.w;
                        vec4 ray_start_world = InverseViewMatrix * ray_start_camera; ray_start_world /=ray_start_world.w;
                        vec4 ray_end_camera = InverseProjectionMatrix * ray_end_NDC; ray_end_camera /=ray_end_camera.w;
                        vec4 ray_end_world = InverseViewMatrix * ray_end_camera; ray_end_world /=ray_end_world.w;

                        const float ray_travel_length = 10000.0;

                        vec3 out_direction(normalize(ray_end_world - ray_start_world) * ray_travel_length);

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
                                if (!grappleTarget)
                                {
                                        grapplePos.setX(RayCallback.m_hitPointWorld.x());
                                        grapplePos.setY(RayCallback.m_hitPointWorld.y());
                                        grapplePos.setZ(RayCallback.m_hitPointWorld.z());
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
                                addedInstances.push_back(i);
                        }
                }

                void instancesFromFile(const char *filename)
                {
                        fstream ifs(filename,ios::binary|ios::in|ios::ate );
                        if (ifs.is_open())
                        {
                                int size = ifs.tellg();
                                for(int i = 0; i<=size/sizeof(Instance); i++)
                                { 
                                        Instance instance;
                                        ifs.seekg(i*sizeof(Instance));
                                        ifs.read(reinterpret_cast<char *>(&instance),sizeof(Instance));
                                        addedInstances.push_back(instance);
                                }
                                printf("Loaded %lu bodies from file\n", addedInstances.size());
                                ifs.close();
                        }
                }
                void instancesToFile(const char *filename)
                {
                        remove(filename);
                        fstream ofs(filename,ios::out|ios::binary|ios::app);
                        for (Instance i : addedInstances)
                        {
                                ofs.write(reinterpret_cast<char *>(&i),sizeof(Instance));
                                printf("Wrote instance %s\n", i.name.c_str());
                        }
                        ofs.close();
                }

                void InitSDL(void)
                {
                        SDL_Init( SDL_INIT_VIDEO );
                        SDL_SetRelativeMouseMode(SDL_TRUE);
                        SDL_ShowCursor(0);
                        videoMode = 0;

                        if (SDL_GetDisplayMode(0, 0, &displayMode) != 0) {
                                SDL_Log("SDL_GetDisplayMode failed: %s", SDL_GetError());
                                exit(1);       
                        }

                        screenWidth = displayMode.w;
                        screenHeight = displayMode.h;
                        window = SDL_CreateWindow("Shoestring Game Engine", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, screenWidth, screenHeight, SDL_WINDOW_OPENGL | SDL_WINDOW_MAXIMIZED);
                }

                void InitGL(void)
                {
                        SDL_GL_CreateContext(window);
                        glewExperimental = GL_TRUE;
                        glewInit(); 
                        staticShader = compile_shader("src/default.vs", "src/default.fs");

                        gl_error();
                }

                void InitFreetype(void)
                {
                  init_resources(texture_atlas);
                }


                void InitBullet(void)
                {
                        collisionConfig.reset(new btDefaultCollisionConfiguration());
                        dispatcher.reset(new btCollisionDispatcher(&*collisionConfig));
                        broadphase.reset(new btDbvtBroadphase());
                        solver.reset(new btSequentialImpulseConstraintSolver());
                        world.reset(new btDiscreteDynamicsWorld(&*dispatcher,&*broadphase,&*solver,&*collisionConfig));
                }

                void InitScene(void)
                {
                        const struct aiScene *scene = importer.ReadFile(scene_file, aiProcessPreset_TargetRealtime_Fast);
                        printf("Loading scene from %s\n\t%s\n", scene_file, importer.GetErrorString());

                        instancesFromGraph(scene->mRootNode, aiMatrix4x4());

                        {
                                fprintf(stderr, "%d\tmeshes\n%d\tmaterials\n%d\tcameras\n", 
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
                                        AddCamera(scene, scene->mCameras[i]);
                                }
                        }


                        for (auto const &mesh : meshes)
                        {
                                if (mesh.second->hasAnimations)
                                {
                                        printf("Animations not yet implemented\n");
                                        mesh.second->init(staticShader);
                                }
                                else
                                        mesh.second->init(staticShader);
                        }

                        importer.FreeScene();
                }

                void InitPhysics(void)
                {
                        m_fileLoader = new btBulletWorldImporter(&*world);

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
                                                char copy[256];
                                                strcpy(copy, name);
                                                copy[strlen(name) - 4] = '\0';
                                                if (meshes.count(name) || meshes.count(copy))
                                                {

                                                        shared_ptr<Object> object(new Object(name, body, meshes.at(name)));

                                                        if (meshes.count(name))
                                                        {
                                                                objects[name] = object;
                                                                printf("Added object %s\n", name);
                                                        }
                                                        else
                                                        {
                                                                objects[copy] = object;
                                                                printf("Added object %s as copy of %s\n", name, copy);
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

                                printf("Bodies in world %d constraints in world %d\n", world->getNumCollisionObjects(), world->getNumConstraints());
                        }
                        else 
                        {
                                throw runtime_error("Failed to physics data");
                        }
                }

                void Bodies(void)
                {

                        btTransform t;

                        //instancesFromFile("bodies.dat");

                        for(Instance &i : addedInstances)
                        {
                                t.setFromOpenGLMatrix(i.transform);

                                if (objects.count(i.name))
                                {
                                        objects[i.name]->new_instance(world, t, 1.0 / objects[i.name]->body->getInvMass(), objects[i.name]->body);
                                        printf("Added instance %s\n", i.name.c_str());
                                }
                        }

                        {
                                Camera cam = cameras.size() ? cameras.at(0) : Camera();

                                cam = Camera();

                                playerPitch = TWOPI;
                                playerYaw = TWOPI;

                                projection = perspective(cam.fov, cam.aspect, cam.near, cam.far);

                                btVector3 inertia(0,0,0);

                                btCapsuleShape *playerShape=new btCapsuleShape(playerRadius, playerHeight);
                                playerShape->calculateLocalInertia(playerMass,inertia);

                                t.setFromOpenGLMatrix(value_ptr(transpose(cam.world)));
                                btMotionState* motion=new btDefaultMotionState(t);

                                btRigidBody::btRigidBodyConstructionInfo info(playerMass,motion,playerShape,inertia);
                                btRigidBody* playerBody=new btRigidBody(info);

                                playerBody->setCollisionFlags( btCollisionObject::CF_CHARACTER_OBJECT);
                                playerBody->setSleepingThresholds(0.0, 0.0);
                                playerBody->setAngularFactor(0.0);

                                world->addRigidBody(playerBody);

                                player = new Object("Player", playerBody, NULL);
                        } 

                        addedInstances.clear();

                }

                void clearInstances(void)
                {
                        for (auto &obj : objects)
                        {
                                for(vector<btRigidBody*>::iterator it = obj.second->bodies.begin() + 1; 
                                                it != obj.second->bodies.end(); ++it) 
                                {
                                        world->removeRigidBody(*it);
                                }
                                obj.second->bodies.erase(obj.second->bodies.begin() + 1, obj.second->bodies.end());
                        }
                        printf("Cleared bodies, remaining bodies in world %d\n", world->getNumCollisionObjects());
                        addedInstances.clear();
                }

                struct aiNode *findnode(struct aiNode *node, const char *name)
                {
                        if (node)
                        {
                          if (!strcmp(name, node->mName.data))
                            return node;
                          for (struct { unsigned int i; struct aiNode *found; } it = { 0, NULL }; it.i < node->mNumChildren; it.i++) {
                                        it.found = findnode(node->mChildren[it.i], name);
                                        if (it.found)
                                                return it.found;
                                }
                        }
                        return NULL;
                };

                void AddMesh(const aiMesh *AIMesh)
                {
                        shared_ptr<Mesh> mesh(new Mesh());
                        mesh->numElements = AIMesh->mNumFaces * 3;
                        for (unsigned int j = 0; j < AIMesh->mNumVertices; j++)
                        {
                                mesh->vertexdata.push_back(AIMesh->mVertices[j].x);
                                mesh->vertexdata.push_back(AIMesh->mVertices[j].y);
                                mesh->vertexdata.push_back(AIMesh->mVertices[j].z);

                                if (AIMesh->mNormals)
                                {
                                        mesh->vertexdata.push_back(AIMesh->mNormals[j].x);
                                        mesh->vertexdata.push_back(AIMesh->mNormals[j].y);
                                        mesh->vertexdata.push_back(AIMesh->mNormals[j].z);
                                }

                                if (AIMesh->mTextureCoords[0])
                                {
                                        mesh->vertexdata.push_back(AIMesh->mTextureCoords[0][j].x);
                                        mesh->vertexdata.push_back(AIMesh->mTextureCoords[0][j].y);
                                        mesh->hasTexture = true;
                                }
                        }

                        for (unsigned int j = 0; j < AIMesh->mNumFaces; j++)
                        {
                                mesh->elements.push_back(AIMesh->mFaces[j].mIndices[0]);
                                mesh->elements.push_back(AIMesh->mFaces[j].mIndices[1]);
                                mesh->elements.push_back(AIMesh->mFaces[j].mIndices[2]);
                        }

                        if (mesh->hasTexture)
                        {
                                mesh->material_idx = AIMesh->mMaterialIndex;
                                mesh->texture = materials.at(AIMesh->mMaterialIndex).texture;
                        }

                        if (AIMesh->HasBones())
                        {
                                mesh->hasAnimations = true;
                        }

                        meshes[string(AIMesh->mName.C_Str())] = mesh;
                };

                void AddMaterial(const struct aiMaterial *AIMaterial)
                {
                        Material material;
                        char filename[2000] = "";
                        struct aiString str;
                        if (!aiGetMaterialString(AIMaterial, AI_MATKEY_TEXTURE_DIFFUSE(0), &str)) 
                        {
                                char *s = strrchr(str.data, '/');
                                if (!s) s = strrchr(str.data, '\\');
                                if (!s) s = str.data; else s++;
                                strcpy(filename, "assets/");
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
                                        material.texture = load_texture(filename);
                                }
                        }
                        else
                                material.texture = -1;
                        aiString _n;
                        AIMaterial->Get(AI_MATKEY_NAME, _n);
                        AIMaterial->Get(AI_MATKEY_COLOR_DIFFUSE,material.diffuse);
                        AIMaterial->Get(AI_MATKEY_COLOR_SPECULAR,material.specular);
                        AIMaterial->Get(AI_MATKEY_SHININESS,material.shininess);
                        material.name = string(_n.data);
                        materials.push_back(material);
                }

                void AddCamera(const struct aiScene *scene, const aiCamera *AICamera)
                {
                        Camera camera(string(AICamera->mName.C_Str()),
                                        AICamera->mHorizontalFOV * 57.2957795,
                                        AICamera->mClipPlaneNear,
                                        AICamera->mClipPlaneFar,
                                        AICamera->mAspect);

                        struct aiNode *node = findnode(scene->mRootNode, AICamera->mName.C_Str());

                        if (node)
                        {
                                aiMatrix4x4 *mat = &node->mTransformation;
                                camera.world = mat4(vec4(mat->a1, mat->a2, mat->a3, mat->a4),
                                                vec4(mat->b1, mat->b2, mat->b3, mat->b4),
                                                vec4(mat->c1, mat->c2, mat->c3, mat->c4),
                                                vec4(mat->d1, mat->d2, mat->d3, mat->d4));
                                cameras.push_back(camera);
                        } 
                };


                void Draw()
                {
                        struct draw_opts opt;
                        Material defaultMaterial;
                        glUseProgram(staticShader);
                        glEnable(GL_BLEND);
                        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);


                        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                        glClearColor(0,0,1,0.5);
                        glEnable(GL_CULL_FACE);
                        glEnable(GL_DEPTH_TEST);
                        glDepthMask(GL_TRUE);

                        {
                                glUniformMatrix4fv(glGetUniformLocation (staticShader, "camera"), 1, GL_FALSE, value_ptr(look));
                                glUniformMatrix4fv(glGetUniformLocation (staticShader, "projection"), 1, GL_FALSE, value_ptr(projection));
                                glUniform3f(glGetUniformLocation (staticShader, "cameraPosition"), eye.x, eye.y, eye.z);
                        }

                        for (const auto &object : objects)
                        {
                                opt.selected = false;
                                if (object.second->mesh->hasTexture)
                                        object.second->draw_buffer(opt, &materials.at(object.second->mesh->material_idx));
                                else
                                        object.second->draw_buffer(opt, &defaultMaterial);
                        }

                        {
                                const float summonDistance = 10.0;

                                btTransform t;
                                t.setIdentity();
                                t.setOrigin(player->body->getWorldTransform().getOrigin());
                                t.setOrigin(t.getOrigin() + btVector3(forward.x * summonDistance, 
                                                        forward.y * summonDistance, 
                                                        forward.z * summonDistance));
                                t.setOrigin(btVector3(round(t.getOrigin().x()),round(t.getOrigin().y()), round(t.getOrigin().z())));
                                opt.camera = t;
                                opt.selected = true;

                                if (createObj)
                                        createObj->draw_buffer(opt, &defaultMaterial);
                        }

                        glDisable(GL_CULL_FACE);
                        glDisable(GL_DEPTH_TEST);
                        glDisable(GL_TEXTURE_2D);
                        glDisable(GL_BLEND);
                        btVector3 playerPosition = player->body->getWorldTransform().getOrigin();
                        position(screenWidth, screenHeight, playerPosition.x(), playerPosition.y(), playerPosition.z());
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
              t.setOrigin(player->body->getWorldTransform().getOrigin());

                                                        const float radius = 10;
                                                        t.setOrigin(t.getOrigin() + (btVector3(forward.x, forward.y, forward.z) * radius));
                                                        t.setOrigin(btVector3(round(t.getOrigin().x()),round(t.getOrigin().y()), round(t.getOrigin().z())));
                                                        Instance instance = createObj->new_instance(world, t, 1.0 / createObj->body->getInvMass(), NULL);
                                                        addedInstances.push_back(instance);
                                                        break;
                                                }
                                        case SDL_MOUSEBUTTONDOWN:
                                                switch (event.button.button)
                                                {
                                                        case SDL_BUTTON_LEFT:
                                                                {
                                                                        playerInput[LEFT_CLICK] = 1;
                                                                        break;
                                                                }
                                                        case SDL_BUTTON_MIDDLE:
                                                                {
                                                                        playerInput[MIDDLE_CLICK] = 1;
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
                                                        float pitchMotion = mouseSensitivity * event.motion.xrel;
                                                        float yawMotion = mouseSensitivity * event.motion.yrel;
                                                        playerYaw += yawMotion;
                                                        // Restrict yaw to avoid inverting up/down orientation
                                                        if (playerYaw < 4.75 || playerYaw > 7.8)
                                                                playerYaw-=yawMotion;
                                                        if (playerPitch > 2 * TWOPI) 
                                                                playerPitch = playerPitch - TWOPI;
                                                        else if (playerPitch < TWOPI)
                                                                playerPitch = TWOPI + playerPitch;
                                                        else playerPitch += pitchMotion;
                                                        break;
                                                }
                                        case SDL_KEYDOWN:
                                                {
                                                        if (keystate[SDLK_PLUS])
                                                        {
                                                                createObjIdx++;
                                                                if (createObjIdx + 1 > objects.size())
                                                                        createObjIdx = 0;
                                                                break;
                                                        }
                                                        if (keystate[SDL_SCANCODE_MINUS])
                                                        {
                                                                createObjIdx--;
                                                                if (createObjIdx < 0)
                                                                        createObjIdx = objects.size() - 1;
                                                                break;
                                                        }
                                                        if (keystate[SDL_SCANCODE_I])
                                                        {
                                                                clearInstances();
                                                        }
                                                        if (keystate[SDL_SCANCODE_O])
                                                        {
                                                                instancesToFile("bodies.dat");
                                                        } 
                                                        if (keystate[SDL_SCANCODE_Q]) {
                                                                playerInput[MIDDLE_CLICK] = 1;
                                                        }
                                                        if (keystate[SDL_SCANCODE_SPACE])
                                                        {
                                                                SDL_SetRelativeMouseMode((SDL_bool) !SDL_GetRelativeMouseMode());
                                                        }
                                                }
                                        case SDL_MOUSEBUTTONUP: 
                                                if (event.button.button == SDL_BUTTON_LEFT)
                                                {
                                                        playerInput[LEFT_CLICK] = 0;
                                                        break;
                                                }
                                                if (event.button.button == SDL_BUTTON_MIDDLE)
                                                {
                                                        playerInput[MIDDLE_CLICK] = 0;
                                                        break;
                                                }
                                        case SDL_KEYUP:
                                                {

                                                        playerInput[MIDDLE_CLICK] = keystate[SDL_SCANCODE_Q] ? 1 : 0;
                                                        playerInput[FORWARD] = keystate[SDL_SCANCODE_W] ? 1 : 0;
                                                        playerInput[BACK] = keystate[SDL_SCANCODE_S] ? 1 : 0;
                                                        playerInput[RIGHT] = keystate[SDL_SCANCODE_D] ? 1 : 0;
                                                        playerInput[LEFT] = keystate[SDL_SCANCODE_A] ? 1 : 0;
                                                        if (keystate[SDL_SCANCODE_ESCAPE])
                                                                throw logic_error("User quit");
                                                        break;
                                                }
                                        case SDL_QUIT:
                                                exit(0);
                                }
                        }

                }

                void Update()
                {
                        // Update player orientation
                        {
                                btVector3 playerOrigin = player->body->getWorldTransform().getOrigin();

                                eye = vec3(playerOrigin.x(), playerOrigin.y(), playerOrigin.z());
                                forward = vec3( cos(playerYaw)*sin(playerPitch), cos(playerYaw) * cos(playerPitch), sin(playerYaw) );

                                look =  lookAt(eye, eye + (forward * float(5.0)), up);
                        }

                        { // Update player velocity
                                vec3 left = cross(forward, up);
                                btVector3 velocity = player->body->getLinearVelocity();
                                btVector3 inputDirection(0,0,0);
                                if (playerInput[LEFT])
                                {
                                        inputDirection += btVector3(left.x, left.y, 0.f);
                                }
                                if (playerInput[FORWARD])
                                {
                                        inputDirection += btVector3(forward.x, forward.y, 0.f);
                                }
                                if (playerInput[RIGHT])
                                {
                                        inputDirection -= btVector3(left.x, left.y, 0.f);
                                }
                                if (playerInput[BACK])
                                {
                                        inputDirection -= btVector3(forward.x, forward.y, 0.f);
                                }
                                if (inputDirection.length() == 0)
                                {
                                        btTransform t;
                                        player->body->getMotionState()->getWorldTransform(t);
                                        player->body->applyForce(btVector3(velocity.x() * breakFactor, velocity.y() * breakFactor, 0),t.getOrigin());
                                }
                                else
                                {
                                        inputDirection = inputDirection.normalize();
                                        velocity = btVector3(inputDirection.x() * movementSpeed, inputDirection.y() * movementSpeed, velocity.z());
                                }
                                player->body->setLinearVelocity(velocity);
                        }

                        {
                                btTransform object, t;
                                if (playerInput[LEFT_CLICK])
                                {
                                        if (heldObject)
                                        { // Summon object towards player
                                                player->body->getMotionState()->getWorldTransform(t);
                                                heldObject->getMotionState()->getWorldTransform(object);
                                                btVector3 summonTo(forward.x, forward.y, forward.z);
                                                btVector3 newVelocity = t.getOrigin()+(10*summonTo) - object.getOrigin();
                                                heldObject->setLinearVelocity(newVelocity * 2.5);
                                        }
                                }
                                else 
                                {
                                        if (heldObject)
                                        { // Throw object in facing direction
                                                const float throwVelocity = 40.f;
                                                heldObject->setLinearVelocity(btVector3(forward.x, forward.y, forward.z) * throwVelocity);
                                                heldObject = NULL;
                                        }
                                }
                                if (playerInput[MIDDLE_CLICK])
                                {
                                        if (grappleTarget)
                                        {
                                                player->body->getMotionState()->getWorldTransform(t);
                                                btVector3 distance = (grapplePos - t.getOrigin());
                                                distance *= 2.0;
                                                if (distance.length() > 10)
                                                        player->body->setLinearVelocity(distance/2.0);
                                                return;
                                        }
                                        else
                                                return;

                                } 
                                else 
                                        grappleTarget = false;
                        }

                        {

                                int i = 0;
                                for (auto &o : objects)
                                {
                                        if (i++ == createObjIdx)
                                        {
                                                createObj = o.second;
                                                break;
                                        }
                                }
                        }

                }

                void Collision(void)
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
                                                //player_grounded = true;
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
                        btRigidBody *collisionBody = RayTrace(screenWidth/2, screenHeight/2);
                        if (collisionBody && !heldObject)
                        {
                                for (auto &o : objects)
                                        for (btRigidBody *bodyInstance : o.second->bodies)
                                        {
                                                if (collisionBody == bodyInstance)
                                                {
                                                        if (playerInput[LEFT_CLICK] && collisionBody->getInvMass() != 0)
                                                                heldObject = collisionBody;
                                                        if (playerInput[MIDDLE_CLICK])
                                                                grappleTarget = collisionBody;
                                                }
                                        }
                        }
                }
  

        public: 
                Context(int argc, char **)
                {
                        srand(time(NULL));
                        InitSDL();
                        InitGL();
                        InitFreetype();
                        InitBullet();
                        InitScene();
                        InitPhysics();
                        Bodies();

                }

                ~Context()
                {
                        delete player;

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


                void Loop()
                {
                        while (1)
                        {
                                tick = SDL_GetTicks();
                                world->stepSimulation(1/60.0);
                                Collision();
                                PollInput();
                                Update();
                                Draw();
                                SDL_GL_SwapWindow(window);

                                if(1000/60>=SDL_GetTicks()-tick)
                                {
                                        SDL_Delay(1000.0/60-(SDL_GetTicks()-tick));
                                }
                        }
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



