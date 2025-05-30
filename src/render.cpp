#include "render.h"

using namespace std;

static const unsigned int DEFAULT_SCREENWIDTH = 1024;
static const unsigned int DEFAULT_SCREENHEIGHT = 768;
static Matrix4 inverseTransposeProjectionMatrix = Matrix4();
float Render::sceneRadius = 1.f;
Vec3f Render::sceneCenter = Vec3f (0.f, 0.f, 0.f);
std::vector<tinyobj::shape_t> Render::shapes = std::vector<tinyobj::shape_t>();
std::vector<tinyobj::material_t> Render::materials = std::vector<tinyobj::material_t>();

char checkError(const char* placeName)
{
    GLenum glError = glGetError(); // Recupere les erreurs OpenGL
    char message[100] = {'\0'}; // Un peu dangereux, mais supposons que nos messages ne sont pas plus long que 100 char

    assert(placeName != NULL);

    switch (glError)
    {
        case GL_NO_ERROR:
                return 0;
                break;

        case GL_INVALID_ENUM:
            strcpy(message,"Invalid enumerator");
            break;

        case GL_INVALID_VALUE:
            strcpy(message,"Invalid value");
            break;

        case GL_INVALID_OPERATION:
            strcpy(message,"Invalid operation");
            break;

        case GL_OUT_OF_MEMORY:
            strcpy(message,"Out of memory");
            break;

        // Cas special pour les FBO
        case GL_INVALID_FRAMEBUFFER_OPERATION_EXT:
            strcpy(message,"Invalid Framebuffer operation");
            break;

        default:
            sprintf(message,"Unknown GL error (0x%ix)",glError);
            break;
    }

    fprintf(stderr,"%s for %s\n",message,placeName);

    return 1;
}


char Render::init (const string & filename)
{



  char error = 0;
  //assert(pRInfo);

    
  //Initialisation du FBO

    error |= initTextureColour(&pRInfo.textureCouleur,pRInfo.width,pRInfo.height);
        std::cout << error << std::endl;

    error |= initTextureColour(&pRInfo.textureNormal,pRInfo.width,pRInfo.height);
    error |= initTextureColour(&pRInfo.texturePosition,pRInfo.width,pRInfo.height);


    error |= initTextureDepth(&pRInfo.textureDepth,pRInfo.width,pRInfo.height);
    //init la deuxieme texture
    error |= initFBO(&(pRInfo.buffer),&pRInfo.textureDepth, &pRInfo.textureNormal, &pRInfo.textureCouleur, &pRInfo.texturePosition, pRInfo.width, pRInfo.height);
    pRInfo.mipColor = new MipMap(pRInfo.textureCouleur, pRInfo.width, pRInfo.height);
    pRInfo.mipNormal = new MipMap(pRInfo.textureNormal, pRInfo.width, pRInfo.height);
    pRInfo.mipPosition = new MipMap(pRInfo.texturePosition, pRInfo.width, pRInfo.height);


  //initialisation random

  glCullFace (GL_BACK);     // Specifies the faces to cull (here the ones pointing away from the camera)
  glEnable (GL_CULL_FACE); // Enables face culling (based on the orientation defined by the CW/CCW enumeration).
  glDepthFunc (GL_LESS); // Specify the depth test for the z-buffer
  glEnable (GL_DEPTH_TEST); // Enable the z-buffer in the rasterization
  glLineWidth (2.0); // Set the width of edges in GL_LINE polygon mode
  glClearColor (0.0f, 0.0f, 0.0f, 1.0f); // Background color
	

  //initialisation du Mesh
  pRInfo.camera->resize (DEFAULT_SCREENWIDTH, DEFAULT_SCREENHEIGHT); // Setup the camera
  pRInfo.mesh->loadOFF (pRInfo.modelFileName); // Load a mesh file
  unsigned int i = filename.find_last_of ("/");
  std::cout<< "scene loaded ? (0 if true)-> " <<  loadScene (filename, filename.substr (0, i+1)) << std::endl;  

  try {

  //initialisation du Premier Programme
  pRInfo.firstPass = Program::genVFProgram ("Simple fist pass Program", "shader1.vert.glsl", "shader1.frag.glsl"); // Load and compile pair of shaders
  pRInfo.firstPass->use (); // Activate the shader program
  pRInfo.firstPass->Program::setUniform4f("matAlbedo",0.4,0.4,0.4,1.);
  glUseProgram(0);
  //initialisation  du Second Programme
  pRInfo.secondPass = Program::genVFProgram ("Simple second pass Program", "shader2.vert.glsl", "shader2.frag.glsl"); // Load and compile pair of shaders
  pRInfo.secondPass->use ();
  pRInfo.secondPass->Program::setUniform1f("roughness_shader", 0.3);
  pRInfo.secondPass->Program::setUniform1f("coeffFresnel", 0.9);
  pRInfo.secondPass->Program::setUniform4f("lightPos", 0., 0., -2., 1.0);

  } catch (Exception & e) {
    std::cout << e.msg () << std::endl;
        std::cout << "Je suis la" << std::endl;

  }

  return error;

}
char Render::initTextureColour(GLuint* pTextureID, unsigned int width, unsigned int height)
{
    char error = 0;

    assert(pTextureID);

    //pTextureID va contenir l'index de la texture
    glGenTextures(1,pTextureID);
    error |= checkError("glGenTextures");

    //on bind la texture pour faire des manip (machine à état...)
    glBindTexture(GL_TEXTURE_2D,*pTextureID);
    error |= checkError("glBindTexture(texId)");

    // Definit les paramètre de la texture
    // Si ce n'est pas fait, la texture n'est pas valide
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    error |= checkError("glTexParameterf");
    // On alloue un espace pour les futures donnees
    // ici on définit le format de la texture, ici ca va pas puisque moi je veux récupérer les normales et la depth donc la couleur m'interresse pas...
    //std::cout << width << " width " << height << " height " << std::endl;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F_ARB, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
    error |= checkError("glTexImage2D");

    //on debind la texture (machine à état)
    glBindTexture(GL_TEXTURE_2D,0);
    error |= checkError("glBindTexture(0)");

    return error;
}

char Render::initTextureDepth(GLuint* pTextureID, unsigned int width, unsigned int height)
{
    char error = 0;

    assert(pTextureID);

    //pTextureID va contenir l'index de la texture
    glGenTextures(1,pTextureID);
    error |= checkError("glGenTextures");

    //on bind la texture pour faire des manip (machine à état...)
    glBindTexture(GL_TEXTURE_2D,*pTextureID);
    error |= checkError("glBindTexture(texId)");

    // Definit les paramètre de la texture
    // Si ce n'est pas fait, la texture n'est pas valide
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    error |= checkError("glTexParameterf");
    // On alloue un espace pour les futures donnees
    // ici on définit le format de la texture, ici ca va pas puisque moi je veux récupérer les normales et la depth donc la couleur m'interresse pas...

    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, width, height, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, 0);
    error |= checkError("glTexImage2D");

    //on debind la texture (machine à état)
    glBindTexture(GL_TEXTURE_2D,0);
    error |= checkError("glBindTexture(0)");

    return error;
}

char Render::initFBO(GLuint* pBuffer, GLuint* pDepth, GLuint* pTextureNormal, GLuint * pTextureCouleur, GLuint * pTexturePosition, unsigned int width, unsigned int height)
{
    char error = 0;

    assert(pBuffer);
    assert(pDepth);
    assert(pTextureCouleur);
    assert(pTextureNormal);
    //on génère un FBO, dont l'index est contenu dans pBuffer
    glGenFramebuffers(1, pBuffer);
 

    // Attachement des donnees au FBO
    glBindFramebuffer(GL_FRAMEBUFFER,*pBuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, *pDepth,0);
    error |= checkError("glFramebufferTexture2D");
    error |= checkError("glBindFramebuffer(FBO)");
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, *pTextureNormal, 0);
    error |= checkError("glFramebufferTexture2D");
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, *pTexturePosition, 0);
    error |= checkError("glFramebufferTexture2D");
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, *pTextureCouleur, 0);
    error |= checkError("glFramebufferTexture2D");
    glDrawBuffer(GL_COLOR_ATTACHMENT1);
    if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
      std::cout << "gros pb" << std::endl;
    }
    //on débind le FBO (machine à état...)
    glBindFramebuffer(GL_FRAMEBUFFER,0);
    error |= checkError("glBindFramebuffer(0)");

    return error;
}


void Render::GenerateGBufferFromMesh()
{
  // Active le rendering dans le FBO (donc, dans une texture)
  pRInfo.firstPass->use();
  glClearColor(0.0f,0.0f,0.0f,1.0f);

  glBindFramebuffer(GL_FRAMEBUFFER,pRInfo.buffer);
  GLenum drawbuffers[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1,GL_COLOR_ATTACHMENT2 };
  glDrawBuffers(3, drawbuffers);
  glClear(GL_COLOR_BUFFER_BIT |GL_DEPTH_BUFFER_BIT );
  //////////////////////////////////

  glBegin (GL_TRIANGLES);
  for (unsigned int i = 0; i < pRInfo.mesh->T.size (); i++) 
    for (unsigned int j = 0; j < 3; j++)
    {
      const Vertex & v = pRInfo.mesh->V[pRInfo.mesh->T[i].v[j]];
      glNormal3f (v.n[0], v.n[1], v.n[2]); // Specifies current normal vertex   
      glVertex3f (v.p[0], v.p[1], v.p[2]); // Emit a vertex (one triangle is emitted each time 3 vertices are emitted)
    }
  glEnd ();
  glUseProgram(0);
}

void Render::GenerateGBufferFromObject()
{
  pRInfo.firstPass->use();
  glClearColor(0.0f,0.0f,0.0f,1.0f);

  glBindFramebuffer(GL_FRAMEBUFFER,pRInfo.buffer);
  GLenum drawbuffers[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1,GL_COLOR_ATTACHMENT2 };
  glDrawBuffers(3, drawbuffers);
  glClear(GL_COLOR_BUFFER_BIT |GL_DEPTH_BUFFER_BIT );
  glBegin (GL_TRIANGLES);
  glColor3f (1.f, 1.f, 1.f);
  for (size_t s = 0; s < shapes.size (); s++)
  {
    for (size_t f = 0; f < shapes[s].mesh.indices.size() / 3; f++) 
    {
      if (!materials.empty ()) 
      {
        unsigned int i = shapes[s].mesh.material_ids[f];
        glColor3f (materials[i].diffuse[0], materials[i].diffuse[1], materials[i].diffuse[2]);
        float shininessValue = materials[i].shininess;
        float rugosity = 1./shininessValue;
        if (rugosity>1.)
        {
          rugosity =1.;
        }

      }
      for (size_t v = 0; v  < 3; v++) 
      {
        unsigned int index = 3*shapes[s].mesh.indices[3*f+v];
        glNormal3f (shapes[s].mesh.normals[index],
              shapes[s].mesh.normals[index+1],
              shapes[s].mesh.normals[index+2]);
        glVertex3f (shapes[s].mesh.positions[index],
              shapes[s].mesh.positions[index+1],
              shapes[s].mesh.positions[index+2]);
      }
    }
  }
  glEnd ();
  glUseProgram(0);

}



void Render::ComputeBRDF(const int& _levelColor, const int& _levelPosition, const int& _levelNormal)
{
  pRInfo.secondPass->use();
  glViewport(0,0,pRInfo.width,pRInfo.height);

  GLfloat model[16]; 
  glGetFloatv(GL_PROJECTION_MATRIX, model);
  inverseTransposeProjectionMatrix = Matrix4(model);
  inverseTransposeProjectionMatrix.invert();
  //inverseTransposeProjectionMatrix.transpose();
  glBindFramebuffer(GL_FRAMEBUFFER,0);
  glClearColor(1.f,0.f,0.0f,1.0f);
  glClear(GL_COLOR_BUFFER_BIT |GL_DEPTH_BUFFER_BIT );

  // Recuperation de l'id de l'uniform
  //GLint idInverseTransposeProjectionMatrix = glGetUniformLocation(pRInfo.secondPass->id(), "inverseTransposeProjectionMatrix");
  GLint idTextureNormal = glGetUniformLocation(pRInfo.secondPass->id(),"fboTexNormal");
  GLint idTextureColor = glGetUniformLocation(pRInfo.secondPass->id(),"fboTexColor");
  //GLint idTextureDepth = glGetUniformLocation(pRInfo.secondPass->id(),"fboTexDepth");
  GLint idTexturePosition = glGetUniformLocation(pRInfo.secondPass->id(),"fboTexPosition");

  if ( idTextureNormal == -1 )
  {        
    fprintf(stderr,"Error while getting the uniform 'fboTexNormal'\n");
  }
  if(idTextureColor == -1 )
  {        
    fprintf(stderr,"Error while getting the uniform 'fboTexColor'\n");
  }
  if(idTexturePosition == -1)
  {
    fprintf(stderr,"Error while getting the uniform 'fboTexPosition'\n");
  }
  //   if(idTextureDepth == -1)
  // {
  //     fprintf(stderr,"Error while getting the uniform 'fboTexDepth'\n");
  // }
    //Assigne la variable inverse transposée de la matrice de projection du fragment shader
   // pRInfo.secondPass->Program::setUniformMatrix4fv(idInverseTransposeProjectionMatrix,inverseTransposeProjectionMatrix.get());

    //Indique la texture au shader
    // glActiveTexture(GL_TEXTURE2);
    // glBindTexture(GL_TEXTURE_2D,pRInfo.textureDepth);
    // glUniform1i(idTextureDepth,2);
    // checkError("glUniform1ui()");
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D,pRInfo.mipNormal->handleTextures[_levelNormal]);
    glUniform1i(idTextureNormal,0);
    checkError("glUniform1i()");

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D,pRInfo.mipColor->handleTextures[_levelColor]);
    glUniform1i(idTextureColor,1);
    checkError("glUniform1i()");

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D,pRInfo.mipPosition->handleTextures[_levelPosition]);
    glUniform1i(idTexturePosition,2 );
    checkError("glUniform1i()");

    // Affichage d'un gros cube
    glDisable (GL_CULL_FACE);
    glBegin(GL_QUADS);
          glColor4f(1, 1, 1, 1);
          glTexCoord2f(0.0f, 0.0f);
          glVertex3f(-1.0f, -1.0f, 0.0f); // The bottom left corner
          glTexCoord2f(0.0f, 1.0f);
          glVertex3f(-1.0f, 1.0f, 0.0f); // The top left corner
          glTexCoord2f(1.0f, 1.0f);
          glVertex3f(1.0f, 1.0f, 0.0f); // The top right corner
          glTexCoord2f(1.0f, 0.0f);
          glVertex3f(1.0f, -1.0f, 0.0f); // The bottom right corner
      glEnd(); // Emit a vertex (one triangle is emitted each time 3 vertices are emitted)
  
    // On desactive la texture
    glUseProgram(0);
    glEnable (GL_CULL_FACE);
}

void Render::drawScene ()
{
  if(loadingBool)
  {
    GenerateGBufferFromMesh();
  }
  else{
    GenerateGBufferFromObject();
  }
  pRInfo.mipColor->raffiner(pRInfo.levelColor);
  pRInfo.mipNormal->raffiner(pRInfo.levelNormal);
  pRInfo.mipPosition->raffiner(pRInfo.levelPosition);
  ComputeBRDF(pRInfo.levelColor,pRInfo.levelPosition,pRInfo.levelNormal);
  displayScreen(pRInfo.levelColor,pRInfo.levelPosition,pRInfo.levelNormal);
}

  void Render::displayScreen(const int& levelC, const int& levelP, const int& levelN)
{

  // Ajout d'un affichage de debug, prouvant que le FBO fonctionne
  // glBindFramebuffer(GL_READ_FRAMEBUFFER,pRInfo.buffer);
  // glReadBuffer(GL_COLOR_ATTACHMENT0);
  // glBlitFramebuffer(0,0,pRInfo.width,pRInfo.height,
  //                 0,0,pRInfo.width/3,pRInfo.height/3,
  //                 GL_COLOR_BUFFER_BIT,
  //                 GL_LINEAR);

  // //Le draw Buffer est par défault le fbo de l'écran et c'est pour ca que ca fonctionne je pense
  // glReadBuffer(GL_COLOR_ATTACHMENT1);
  // glBlitFramebuffer(0,0,pRInfo.width,pRInfo.height,
  //                 0,pRInfo.height/3,pRInfo.width/3,2*pRInfo.height/3,
  //                 GL_COLOR_BUFFER_BIT,
  //                 GL_LINEAR);

  // glReadBuffer(GL_COLOR_ATTACHMENT2);
  // glBlitFramebuffer(0,0,pRInfo.width,pRInfo.height,
  //                 pRInfo.width/3,0,2*pRInfo.width/3,pRInfo.height/3,
  //                 GL_COLOR_BUFFER_BIT,
  //                 GL_LINEAR);

  pRInfo.mipColor ->use(levelC);
  glBindFramebuffer(GL_READ_FRAMEBUFFER,pRInfo.mipColor->handleFBOEnd);
  glReadBuffer(GL_COLOR_ATTACHMENT0);
  glBlitFramebuffer(0,0,pRInfo.mipColor->width/pow(2,levelC),pRInfo.mipColor->height/pow(2,levelC),
                  0,0,pRInfo.width/3,pRInfo.height/3,
                  GL_COLOR_BUFFER_BIT,
                  GL_LINEAR);

  pRInfo.mipNormal->use(levelN);
  glBindFramebuffer(GL_READ_FRAMEBUFFER,pRInfo.mipNormal->handleFBOEnd);
  glReadBuffer(GL_COLOR_ATTACHMENT0);
  glBlitFramebuffer(0,0,pRInfo.mipColor->width/pow(2,levelN),pRInfo.mipColor->height/pow(2,levelN),
                  0,pRInfo.height/3,pRInfo.width/3,2*pRInfo.height/3,
                  GL_COLOR_BUFFER_BIT,
                  GL_LINEAR);

  pRInfo.mipPosition->use(levelP);
  glBindFramebuffer(GL_READ_FRAMEBUFFER,pRInfo.mipPosition->handleFBOEnd);
  glReadBuffer(GL_COLOR_ATTACHMENT0);
  glBlitFramebuffer(0,0,pRInfo.mipColor->width/pow(2,levelP),pRInfo.mipColor->height/pow(2,levelP),
                  pRInfo.width/3,0,2*pRInfo.width/3,pRInfo.height/3,
                  GL_COLOR_BUFFER_BIT,
                  GL_LINEAR);

  glBindFramebuffer(GL_FRAMEBUFFER,0);
  pRInfo.mipColor->SetLevel(0);
  pRInfo.mipPosition->SetLevel(0);
  pRInfo.mipNormal->SetLevel(0);

}


  Render::Render(RenderingInfo const &  _pRInfo, Camera * const camera_, Mesh * const mesh_):
pRInfo(_pRInfo),
loadingBool(true)
{
  pRInfo.camera = camera_;
  pRInfo.mesh = mesh_;
  pRInfo.levelPosition = 0;
  pRInfo.levelNormal = 0;
  pRInfo.levelColor = 0;
  std::cout << "Level of mipmap in texture Color used for rendering : " << pRInfo.levelColor << std::endl;
  std::cout << "Level of mipmap in texture Normal used for rendering : " << pRInfo.levelNormal << std::endl;
  std::cout << "Level of mipmap in texture Position used for rendering : " << pRInfo.levelPosition << std::endl;

}


 Render::~Render()
 {

 }


void Render::computeSceneNormals () 
{
//ok recalcule les normales de chaques vertex comme somme des normales des faces adjacentes
  for (unsigned int s = 0; s < shapes.size (); s++) 
    if (shapes[s].mesh.normals.empty ())
    {
      shapes[s].mesh.normals.resize (shapes[s].mesh.positions.size (), 0.f);
      for (size_t f = 0; f < shapes[s].mesh.indices.size() / 3; f++)
      {
        Vec3f q[3];
        for (size_t v = 0; v < 3; v++) 
        {
          unsigned int index = 3*shapes[s].mesh.indices[3*f+v];
          for (unsigned int i = 0; i < 3; i++)
          q[v][i] = shapes[s].mesh.positions[index+i];
        }
        Vec3f e01 = q[1] - q[0];
        Vec3f e02 = q[2] - q[0];
        Vec3f nf = normalize (cross (e01, e02));
        for (size_t v = 0; v < 3; v++) 
        {
          unsigned int index = 3*shapes[s].mesh.indices[3*f+v];
          for (unsigned int i = 0; i < 3; i++)
          shapes[s].mesh.normals[index+i] += nf[i];
        }
      }
      for (unsigned int i = 0; i < shapes[s].mesh.normals.size () / 3; i++) 
      {
        Vec3f n;
        for (unsigned int j = 0; j < 3; j++)
        {
          n[j] = shapes[s].mesh.normals[3*i+j];
        }
        n.normalize ();
        for (unsigned int j = 0; j < 3; j++)
        {
          shapes[s].mesh.normals[3*i+j] = n[j];
        }     
      } 
    }
}

void Render::computeSceneBoundingSphere () {
  sceneCenter = Vec3f (0.f, 0.f, 0.f);
  unsigned int count = 0;
  for (unsigned int s = 0; s < shapes.size (); s++)
  for (unsigned int p = 0; p < shapes[s].mesh.positions.size () / 3; p++) {
    sceneCenter += Vec3f (shapes[s].mesh.positions[3*p],
              shapes[s].mesh.positions[3*p+1],
              shapes[s].mesh.positions[3*p+2]);
    count++;
  }
  sceneCenter /= count;
  sceneRadius = 0.f;
  for (unsigned int s = 0; s < shapes.size (); s++)
    for (unsigned int p = 0; p < shapes[s].mesh.positions.size () / 3; p++) {
      float d = dist (sceneCenter, Vec3f (shapes[s].mesh.positions[3*p],
                      shapes[s].mesh.positions[3*p+1],
                      shapes[s].mesh.positions[3*p+2]));
      if (d > sceneRadius)
    sceneRadius = d;
    }
}

// Loads an OBJ file using tinyOBJ (http://syoyo.github.io/tinyobjloader/)
bool Render::loadScene(const string & filename, const string & basepath) {
  shapes.clear ();
  materials.clear ();
  std::cout << "Loading " << filename << std::endl;
  std::string err = tinyobj::LoadObj(shapes, materials, filename.c_str (), basepath.c_str ());
  if (!err.empty()) {
    std::cerr << err << std::endl;
    return false;
  }
  computeSceneNormals ();
  computeSceneBoundingSphere ();
  return true;
}


// Ajouter une texture pour stocker la roughness
// La texture (couleur diffuse)
// Voir avec malik ce qu'il faut stocker exactement pk pas trés claire pour l'instant

// Pour quoi pas faire le sujet de Ray Tracing
