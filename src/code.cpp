#define NO_SDL_GLEXT

#include <GL/glew.h>
#include <GL/gl.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <stdio.h>
#include <memory>
#include <vector>
#include <fstream>
#include <iostream>
#include <unordered_map>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Woverloaded-virtual"
#include <btBulletDynamicsCommon.h>
#pragma clang diagnostic pop
#include <LinearMath/btSerializer.h>
#include <LinearMath/btIDebugDraw.h>
#include <btBulletFile.h>
#include <btBulletWorldImporter.h>

#include <glstuff.h>
#include <text.h>

using namespace std;
using namespace glm;
enum {LEFT = 0, RIGHT = 1, FORWARD = 2, BACK = 3, LEFT_CLICK = 4, RIGHT_CLICK = 5, MIDDLE_CLICK = 6, JUMP = 7};
static const float TWOPI = 6.28318530718;
static const vec3 up(0,0,-1);
class Context;
class Material;
class Mesh;
class Object;
class Camera;

struct drawOptions
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

  Instance addInstance(shared_ptr<btDiscreteDynamicsWorld> world, btTransform t, btScalar mass, btRigidBody *b)
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

  void drawBufferr(struct drawOptions opt, Material *material)
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

            glDrawElements( GL_TRIANGLES, (mesh->elements.size()), GL_UNSIGNED_INT, NULL);
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

class Player {
public:
  Player(const char *name, mat4 worldTransform, shared_ptr<btDiscreteDynamicsWorld> world) {
    this->name = name;

    btVector3 inertia(0,0,0);
    btTransform t;
    btCapsuleShape *playerShape=new btCapsuleShape(radius, height);
    playerShape->calculateLocalInertia(mass, inertia);
    t.setFromOpenGLMatrix(value_ptr(transpose(worldTransform)));
    btMotionState* motion=new btDefaultMotionState(t);
    btRigidBody::btRigidBodyConstructionInfo info(mass,motion,playerShape,inertia);
    btRigidBody* playerBody=new btRigidBody(info);

    playerBody->setCollisionFlags( btCollisionObject::CF_CHARACTER_OBJECT);
    playerBody->setSleepingThresholds(0.0, 0.0);
    playerBody->setAngularFactor(0.0);

    world->addRigidBody(playerBody);

    this->object = new Object(name, playerBody, NULL);
  }
  string name;
  Object *object;
  btRigidBody *heldObject = NULL;
  bool grappleTarget = false;
  btVector3 grapplePos;
  float mouseSensitivity = 0.005;
  double previousX = 0.f;
  double previousY = 0.f;
  bool grounded = false;
  int createObjectIndex = 0;
  float yaw = TWOPI;
  float pitch = TWOPI;
  int input[8] = {0};
  float mass = 1.0;
  float height = 2.0;
  float radius = 2.0;
  float movementSpeed = 8.0;
  float breakFactor = -25.0;
};

class Context
{
private:
  const char *scene_file = "assets/sandbox.fbx", *bullet_file = "assets/sandbox.bullet";
  btBulletWorldImporter* m_fileLoader;
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


  int screenWidth;
  int screenHeight;
  GLFWwindow* window;

  Player *player;
  shared_ptr<Object> createObj;

  mat4 look, projection;
  vec3 eye, forward;

  GLuint staticShader;

  btRigidBody *rayCast(int x, int y)
  {
    vec4 ray_start_NDC( ((float)x/(float)screenWidth  - 0.5f) * 2.0f, ((float)y/(float)screenHeight - 0.5f) * 2.0f, -1.0, 1.0f);
    vec4 ray_end_NDC( ((float)x/(float)screenWidth  - 0.5f) * 2.0f, ((float)y/(float)screenHeight - 0.5f) * 2.0f, 0.0, 1.0f);

    mat4 inverseProjectionMatrix = inverse(projection);

    mat4 inverseViewMatrix = inverse(look);

    vec4 ray_start_camera = inverseProjectionMatrix * ray_start_NDC; ray_start_camera/=ray_start_camera.w;
    vec4 ray_start_world = inverseViewMatrix * ray_start_camera; ray_start_world /=ray_start_world.w;
    vec4 ray_end_camera = inverseProjectionMatrix * ray_end_NDC; ray_end_camera /=ray_end_camera.w;
    vec4 ray_end_world = inverseViewMatrix * ray_end_camera; ray_end_world /=ray_end_world.w;

    const float ray_travel_length = 10000.0;

    vec3 out_direction(normalize(ray_end_world - ray_start_world) * ray_travel_length);

    btCollisionWorld::ClosestRayResultCallback rayCallback(
                                                           btVector3(ray_start_world.x, ray_start_world.y, ray_start_world.z),
                                                           btVector3(out_direction.x, out_direction.y, out_direction.z)
                                                           );
    world->rayTest(
                   btVector3(ray_start_world.x, ray_start_world.y, ray_start_world.z),
                   btVector3(out_direction.x, out_direction.y, out_direction.z),
                   rayCallback
                   );

    if(rayCallback.hasHit())
      {
        if (!player->grappleTarget)
          {
            player->grapplePos.setX(rayCallback.m_hitPointWorld.x());
            player->grapplePos.setY(rayCallback.m_hitPointWorld.y());
            player->grapplePos.setZ(rayCallback.m_hitPointWorld.z());
          }
        btRigidBody *obj = (btRigidBody*) rayCallback.m_collisionObject;
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

  static void glfwMouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
  {
    Context *context = static_cast<Context *> (glfwGetWindowUserPointer(window));
    context->player->input[LEFT_CLICK] = button == GLFW_MOUSE_BUTTON_LEFT ? action : context->player->input[LEFT_CLICK];
    context->player->input[MIDDLE_CLICK] = button == GLFW_MOUSE_BUTTON_MIDDLE ? action : context->player->input[MIDDLE_CLICK];
    if (button == GLFW_MOUSE_BUTTON_RIGHT) {
      btVector3 velocity = context->player->object->body->getLinearVelocity();
      velocity.setZ(10);
      context->player->object->body->setLinearVelocity(velocity);
    }
  }

  static void glfwKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
  {
    Context *context = static_cast<Context *> (glfwGetWindowUserPointer(window));
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
      glfwSetWindowShouldClose(window, 1);
    }
    if (key == GLFW_KEY_SPACE && action == GLFW_PRESS) {
      context->spawnCurrentObject();
    }
    context->player->input[FORWARD] = key == GLFW_KEY_W ? action : context->player->input[FORWARD];
    context->player->input[BACK] = key == GLFW_KEY_S ? action : context->player->input[BACK];
    context->player->input[RIGHT] = key == GLFW_KEY_D ? action : context->player->input[RIGHT];
    context->player->input[LEFT] = key == GLFW_KEY_A ? action : context->player->input[LEFT];
    if (action == GLFW_PRESS) {
      if (key == GLFW_KEY_J)
        {
          context->player->createObjectIndex++;
          if (context->player->createObjectIndex + 1 > context->objects.size())
            context->player->createObjectIndex = 0;
        }
      if (key == GLFW_KEY_K)
        {
          context->player->createObjectIndex--;
          if (context->player->createObjectIndex < 0)
            context->player->createObjectIndex = context->objects.size() - 1;
        }
      if (key == GLFW_KEY_I)
        {
          context->clearInstances();
        }
      if (key == GLFW_KEY_O)
        {
          context->instancesToFile("bodies.dat");
        }
    }
  }

  static void glfwErrorCallback(int error, const char* description)
  {
    fprintf(stderr, "Error: %s\n", description);
  }

  static void glfwMouseCallback(GLFWwindow* window, double x, double y)
  {
    Context *context = static_cast<Context *> (glfwGetWindowUserPointer(window));
    float pitchMotion = context->player->mouseSensitivity * (x - context->player->previousX);
    float yawMotion = context->player->mouseSensitivity * (y - context->player->previousY);
    context->player->previousX = x;
    context->player->previousY = y;
    context->player->yaw += yawMotion;
    // Restrict yaw to avoid inverting Y orientation
    if (context->player->yaw < 4.75 || context->player->yaw > 7.8)
      context->player->yaw-=yawMotion;
    if (context->player->pitch > 2 * TWOPI)
      context->player->pitch = context->player->pitch - TWOPI;
    else if (context->player->pitch < TWOPI)
      context->player->pitch = TWOPI + context->player->pitch;
    else context->player->pitch += pitchMotion;
  }

  void initGLFW(void)
  {
    if (!glfwInit()) {
      throw runtime_error("glfw error");
    }
    const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
    screenWidth = mode->width;
    screenHeight = mode->height;
    glfwWindowHint(GLFW_RED_BITS, mode->redBits);
    glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
    glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
    glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);
    window = glfwCreateWindow(mode->width, mode->height, "Shoestring", glfwGetPrimaryMonitor(), NULL);
    glfwSetErrorCallback(glfwErrorCallback);
    glfwSetKeyCallback(window, glfwKeyCallback);
    glfwSetMouseButtonCallback(window, glfwMouseButtonCallback);
    glfwSetCursorPosCallback(window, glfwMouseCallback);
    glfwSetWindowUserPointer(window, this);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwMakeContextCurrent(window);
  }

  void initGL(void)
  {
    glewExperimental = GL_TRUE;
    glewInit();
    staticShader = compile_shader("src/default.vs", "src/default.fs");

    gl_error();
  }

  void initBullet(void)
  {
    collisionConfig.reset(new btDefaultCollisionConfiguration());
    dispatcher.reset(new btCollisionDispatcher(&*collisionConfig));
    broadphase.reset(new btDbvtBroadphase());
    solver.reset(new btSequentialImpulseConstraintSolver());
    world.reset(new btDiscreteDynamicsWorld(&*dispatcher,&*broadphase,&*solver,&*collisionConfig));
  }

  void initScene()
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

  void initPhysics()
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

      }
    else
      {
        throw runtime_error("Failed to physics data");
      }
    printf("%d bodies and %d constraints in world\n", world->getNumCollisionObjects(), world->getNumConstraints());
  }

  void initRigidBodies()
  {

    btTransform t;

    //instancesFromFile("bodies.dat");

    for(Instance &i : addedInstances) {
      t.setFromOpenGLMatrix(i.transform);

      if (objects.count(i.name))
        {
          objects[i.name]->addInstance(world, t, 1.0 / objects[i.name]->body->getInvMass(), objects[i.name]->body);
          printf("Added instance %s\n", i.name.c_str());
        }
    }


    {
      Camera cam = cameras.size() ? cameras.at(0) : Camera();

      cam = Camera();

      projection = perspective(cam.fov, cam.aspect, cam.near, cam.far);

      player = new Player("Player", cam.world, world);
    }


    addedInstances.clear();
  }

  void clearInstances(void)
  {
    for (auto &obj : objects)
      {
        for(vector<btRigidBody*>::iterator i = obj.second->bodies.begin() + 1;
            i != obj.second->bodies.end(); ++i)
          {
            world->removeRigidBody(*i);
          }
        obj.second->bodies.erase(obj.second->bodies.begin() + 1, obj.second->bodies.end());
      }
    printf("Cleared bodies, remaining bodies in world %d\n", world->getNumCollisionObjects());
    addedInstances.clear();
  }

  struct aiNode *findNode(struct aiNode *node, const char *name)
  {
    if (node)
      {
        if (!strcmp(name, node->mName.data))
          return node;
        for (struct { unsigned int i; struct aiNode *found; } i = { 0, NULL }; i.i < node->mNumChildren; i.i++) {
          i.found = findNode(node->mChildren[i.i], name);
          if (i.found)
            return i.found;
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

    struct aiNode *node = findNode(scene->mRootNode, AICamera->mName.C_Str());

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


  void drawScene()
  {
    struct drawOptions opt;
    Material defaultMaterial;
    glUseProgram(staticShader);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);


    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glClearColor(0,0,0,0);
    glEnable(GL_DEPTH_TEST);
    glCullFace(GL_FRONT);
    glDepthMask(GL_TRUE);

    {
      glUniformMatrix4fv(glGetUniformLocation (staticShader, "view"), 1, GL_FALSE, value_ptr(look));
      glUniformMatrix4fv(glGetUniformLocation (staticShader, "projection"), 1, GL_FALSE, value_ptr(projection));
      glUniform3f(glGetUniformLocation (staticShader, "campPos"), eye.x, eye.y, eye.z);
    }

    for (const auto &object : objects)
      {
        opt.selected = false;
        if (object.second->mesh->hasTexture)
          object.second->drawBufferr(opt, &materials.at(object.second->mesh->material_idx));
        else
          object.second->drawBufferr(opt, &defaultMaterial);
      }

    {
      const float summonDistance = 10.0;

      btTransform t;
      t.setIdentity();
      t.setOrigin(player->object->body->getWorldTransform().getOrigin());
      t.setOrigin(t.getOrigin() + btVector3(forward.x * summonDistance,
                                            forward.y * summonDistance,
                                            forward.z * summonDistance));
      t.setOrigin(btVector3(round(t.getOrigin().x()),round(t.getOrigin().y()), round(t.getOrigin().z())));
      opt.camera = t;
      opt.selected = true;

      if (createObj)
        createObj->drawBufferr(opt, &defaultMaterial);
    }

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
  }

  void drawUI()  {
    //uiPosition(screenWidth, screenHeight, playerPosition.x(), playerPosition.y(), playerPosition.z());
    //uiObject(screenWidth, screenHeight, createObj->name.c_str());

    char buff[128];
    btVector3 playerPosition = player->object->body->getWorldTransform().getOrigin();
    snprintf(buff, sizeof(buff), "FPS: %f", 1.f);
    uiText(0, screenWidth, screenHeight, buff);
    snprintf(buff, sizeof(buff), "Position: [%f %f %f]", playerPosition.x(), playerPosition.y(), playerPosition.z());
    uiText(1, screenWidth, screenHeight, buff);
    snprintf(buff, sizeof(buff), "Creating object: %s", createObj->name.c_str());
    uiText(2, screenWidth, screenHeight, buff);
  }

  void updatePlayer() {
    {// Orientation
      btVector3 playerOrigin = player->object->body->getWorldTransform().getOrigin();

      eye = vec3(playerOrigin.x(), playerOrigin.y(), playerOrigin.z());
      forward = vec3( cos(player->yaw)*sin(player->pitch), cos(player->yaw) * cos(player->pitch), sin(player->yaw) );

      look =  lookAt(eye, eye + (forward * float(5.0)), up);
    }

    { // Velocity
      vec3 left = cross(forward, up);
      btVector3 velocity = player->object->body->getLinearVelocity();
      btVector3 inputDirection(0,0,0);
      if (player->input[LEFT])
        {
          inputDirection += btVector3(left.x, left.y, 0.f);
        }
      if (player->input[FORWARD])
        {
          inputDirection += btVector3(forward.x, forward.y, 0.f);
        }
      if (player->input[RIGHT])
        {
          inputDirection -= btVector3(left.x, left.y, 0.f);
        }
      if (player->input[BACK])
        {
          inputDirection -= btVector3(forward.x, forward.y, 0.f);
        }
      if (inputDirection.length() == 0)
        {
          btTransform t;
          player->object->body->getMotionState()->getWorldTransform(t);
          player->object->body->applyForce(btVector3(velocity.x() * player->breakFactor, velocity.y() * player->breakFactor, 0),t.getOrigin());
        }
      else
        {
          inputDirection = inputDirection.normalize();
          velocity = btVector3(inputDirection.x() * player->movementSpeed, inputDirection.y() * player->movementSpeed, velocity.z());
        }
      player->object->body->setLinearVelocity(velocity);
    }

    {
      btTransform object, t;
      if (player->input[LEFT_CLICK])
        {
          if (player->heldObject)
            { // Summon object
              player->object->body->getMotionState()->getWorldTransform(t);
              player->heldObject->getMotionState()->getWorldTransform(object);
              btVector3 summonTo(forward.x, forward.y, forward.z);
              btVector3 newVelocity = t.getOrigin()+(10*summonTo) - object.getOrigin();
              player->heldObject->setLinearVelocity(newVelocity * 2.5);
            }
        }
      else {
        if (player->heldObject)
          { // Throw object
            const float throwVelocity = 40.f;
            player->heldObject->setLinearVelocity(btVector3(forward.x, forward.y, forward.z) * throwVelocity);
            player->heldObject = NULL;
          }
      }
      if (player->input[MIDDLE_CLICK])
        {
          if (player->grappleTarget)
            {
              player->object->body->getMotionState()->getWorldTransform(t);
              btVector3 distance = (player->grapplePos - t.getOrigin());
              distance *= 2.0;
              if (distance.length() > 10)
                player->object->body->setLinearVelocity(distance/2.0);
              return;
            }
          else
            return;

        } else {
        player->grappleTarget = false;
      }

      {

        int i = 0;
        for (auto &o : objects)
          {
            if (i++ == player->createObjectIndex)
              {
                createObj = o.second;
                break;
              }
          }
      }

    }
  }

  void collision(void)
  {
    int numManifolds = world->getDispatcher()->getNumManifolds();
    for (int i=0;i<numManifolds;i++)
      {
        btPersistentManifold* contactManifold =  world->getDispatcher()->getManifoldByIndexInternal(i);
        //const btCollisionObject* obA = contactManifold->getBody0();
        btRigidBody *_p = (btRigidBody*) contactManifold->getBody1();
        if (_p == player->object->body)
          {
            btVector3 velocity = player->object->body->getLinearVelocity();
            if (fabs(velocity.z()) < 0.1)
              {
                //player->grounded = true;
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
    btRigidBody *collisionBody = rayCast(screenWidth/2, screenHeight/2);
    if (collisionBody && !player->heldObject)
      {
        for (auto &o : objects)
          for (btRigidBody *bodyInstance : o.second->bodies)
            {
              if (collisionBody == bodyInstance)
                {
                  if (player->input[LEFT_CLICK] && collisionBody->getInvMass() != 0)
                    player->heldObject = collisionBody;
                  if (player->input[MIDDLE_CLICK])
                    player->grappleTarget = collisionBody;
                }
            }
      }
  }

  void spawnCurrentObject() {
    btTransform t;
    t.setIdentity();
    t.setOrigin(player->object->body->getWorldTransform().getOrigin());

    const float radius = 10;
    t.setOrigin(t.getOrigin() + (btVector3(forward.x, forward.y, forward.z) * radius));
    t.setOrigin(btVector3(round(t.getOrigin().x()),round(t.getOrigin().y()), round(t.getOrigin().z())));
    Instance instance = createObj->addInstance(world, t, 1.0 / createObj->body->getInvMass(), NULL);
    addedInstances.push_back(instance);
  }

  void spawnStuff() {
    int x, y;
    for (y = 0; y < 10; y++) {
      for (x = 0; x < 10; x++) {
        btTransform t;
        t.setIdentity();
        t.setOrigin(btVector3(x*2,y*2,0));
        Instance instance = objects["Cube.001"]->addInstance(world, t, 1.0 / objects["Cube.001"]->body->getInvMass(), NULL);
        addedInstances.push_back(instance);
      }
    }
  }

public:
  Context(int argc, char **)
  {
    initGLFW();
    initGL();
    initFreetype();
    initBullet();
    initScene();
    initPhysics();
    initRigidBodies();
    spawnStuff();
  }

  ~Context()
  {
    delete player->object;

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
    destroyFreetype();
  }


  void loop()
  {
    srand(time(NULL));
    while (!glfwWindowShouldClose(window))
      {
        world->stepSimulation(1/60.0);
        collision();
        updatePlayer();
        drawScene();
        drawUI();
        glfwPollEvents();
        glfwSwapBuffers(window);
      }
  }

};

int main( int argc, char *argv[] )
{
  try
    {
      Context *ctx = new Context(argc, argv);
      ctx->loop();
    }
  catch (exception &e)
    {
      fprintf(stderr, "%s\n", e.what());
    }
  return 0;
}
