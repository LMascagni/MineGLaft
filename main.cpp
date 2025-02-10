// ================================
// LIBRERIE
// ================================
#include <GL/glut.h>
#include <cmath>
#include <vector>
#include <random>
#include <unordered_map>
#include <functional>
#include <array>
#include <iostream>
#include <string>
#include <chrono>
#include <algorithm>

// Aggiungi subito dopo gli include per definire GL_CLAMP_TO_EDGE se non definito
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

// Inserisci l'header per il caricamento delle immagini (assicurati di aggiungere il file stb_image.h nel progetto)
#define STB_IMAGE_IMPLEMENTATION
#include "lib/stb_image/stb_image.h"

// ================================
// COSTANTI GLOBALI
// ================================
const int CHUNK_SIZE = 16;
const int CHUNK_HEIGHT = 100;
const int RENDER_DISTANCE = 20;

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Aggiungi una costante che indica il numero di tipi di blocco texturizzati
const int NUM_BLOCK_TYPES = 8;

// Variabile globale per la texture dei blocchi
GLuint blockTexture = 0;

// Variabili globali per il sistema delle texture
int atlasWidth = 0;
int atlasHeight = 0;
const int textureCellSize = 16;

// ================================
// STRUTTURE E CLASSI
// ================================
class PerlinNoise
{
private:
   int seed;
   std::vector<int> permutation;

   // Funzione per generare una tabella di permutazione pseudocasuale
   void generatePermutation()
   {
      std::mt19937 generator(seed);
      permutation.resize(512);
      for (int i = 0; i < 256; ++i)
      {
         permutation[i] = i;
      }
      std::shuffle(permutation.begin(), permutation.begin() + 256, generator);
      // Duplica la tabella per evitare controlli di wrapping
      for (int i = 0; i < 256; i++)
      {
         permutation[256 + i] = permutation[i];
      }
   }

   // Funzione per interpolare con un polinomio smoothstep
   float smoothstep(float t) const
   {
      return t * t * (3.0f - 2.0f * t);
   }

   // Funzione per interpolare linearmente tra due valori
   float lerp(float a, float b, float t) const
   {
      return a + t * (b - a);
   }

   // Funzione per ottenere il gradiente in un punto della griglia
   float gradient(int hash, float x, float y, float z) const
   {
      hash = hash & 15; // Limita hash a 0-15
      float u = (hash < 8) ? x : y;
      float v = (hash < 4) ? y : ((hash == 12 || hash == 14) ? x : z);
      return ((hash & 1) == 0 ? u : -u) + ((hash & 2) == 0 ? v : -v);
   }

public:
   PerlinNoise(int seedValue) : seed(seedValue)
   {
      generatePermutation();
   }

   // Funzione principale per calcolare il Perlin Noise
   float getNoise(float x, float y, float z) const
   {
      // Trova la cella della griglia contenente il punto (x, y, z)
      int X = static_cast<int>(std::floor(x));
      int Y = static_cast<int>(std::floor(y));
      int Z = static_cast<int>(std::floor(z));

      // Usa un sistema di coordinate che gestisce correttamente i negativi
      int XX = X & 255;
      int YY = Y & 255;
      int ZZ = Z & 255;

      x -= std::floor(x);
      y -= std::floor(y);
      z -= std::floor(z);

      // Interpolazione smoothstep
      float u = smoothstep(x);
      float v = smoothstep(y);
      float w = smoothstep(z);

      // Hash delle coordinate della griglia
      int A = permutation[XX] + YY;
      int AA = permutation[A & 255] + ZZ;
      int AB = permutation[(A + 1) & 255] + ZZ;
      int B = permutation[(XX + 1) & 255] + YY;
      int BA = permutation[B & 255] + ZZ;
      int BB = permutation[(B + 1) & 255] + ZZ;

      // Combina i risultati dei gradienti
      float res = lerp(
          lerp(
              lerp(gradient(permutation[AA], x, y, z), gradient(permutation[BA], x - 1, y, z), u),
              lerp(gradient(permutation[AB], x, y - 1, z), gradient(permutation[BB], x - 1, y - 1, z), u), v),
          lerp(
              lerp(gradient(permutation[AA + 1], x, y, z - 1), gradient(permutation[BA + 1], x - 1, y, z - 1), u),
              lerp(gradient(permutation[AB + 1], x, y - 1, z - 1), gradient(permutation[BB + 1], x - 1, y - 1, z - 1), u), v),
          w);

      return res;
   }
};

// Classe Punto
class Point3D
{
public:
   float x, y, z;

   Point3D(float xPos = 0.0f, float yPos = 0.0f, float zPos = 0.0f) : x(xPos), y(yPos), z(zPos) {}

   void normalize()
   {
      float length = std::sqrt(x * x + y * y + z * z);
      if (length > 0.0f)
      {
         x /= length;
         y /= length;
         z /= length;
      }
   }

   // Operatore == per confrontare due Point3D
   bool operator==(const Point3D &other) const
   {
      return (x == other.x && y == other.y && z == other.z);
   }

   // Operatore != per confrontare due Point3D
   bool operator!=(const Point3D &other) const
   {
      return !(*this == other);
   }

   // Operatore + per sommare due Point3D
   Point3D operator+(const Point3D &other) const
   {
      return Point3D(x + other.x, y + other.y, z + other.z);
   }

   // Operatore * per moltiplicare un Point3D per uno scalare
   Point3D operator*(float scalar) const
   {
      return Point3D(x * scalar, y * scalar, z * scalar);
   }

   // Operatore * per moltiplicare uno scalare per un Point3D
   friend Point3D operator*(float scalar, const Point3D &point)
   {
      return Point3D(point.x * scalar, point.y * scalar, point.z * scalar);
   }
};

class Point2D
{
public:
   float x, z;
   Point2D(float xPos = 0.0f, float zPos = 0.0f) : x(xPos), z(zPos) {}

   bool operator==(const Point2D &other) const
   {
      return (x == other.x && z == other.z);
   }
};

// Classe Inclinazione
class Rotation
{
public:
   float xRot, yRot, zRot;
   Rotation(float initialXRot = 0.0f, float initialYRot = 0.0f, float initialZRot = 0.0f) : xRot(initialXRot), yRot(initialYRot), zRot(initialZRot) {}
};

// Classe Colore
class Color
{
public:
   float r, g, b, a; // Add alpha component

   // Constructor with default alpha value of 1.0f (fully opaque)
   Color(float red = 1.0f, float green = 1.0f, float blue = 1.0f, float alpha = 1.0f) : r(red), g(green), b(blue), a(alpha) {}

   void apply() const
   {
      glColor4f(r, g, b, a); // Use glColor4f instead of glColor3f to include alpha
   }
};

// Classe Camera
class Camera
{
public:
   Point3D pos;
   Rotation rot;

   Camera(Point3D initialPos = Point3D(0.0f, 0.0f, 0.0f), Rotation initialRot = Rotation(0.0f, 0.0f, 0.0f))
       : pos(initialPos), rot(initialRot) {}

   void reset();
   bool isFaceVisible(const Point3D &blockPos, const Point3D &faceNormalVect) const;
};

// Dichiarazione anticipata della classe Chunk
class Chunk;

// Enumerazione globale per i tipi di blocco
enum class BlockType
{
   TEST,       //01
   AIR,        //00
   GRASS,      //02
   DIRT,       //03
   STONE,      //04
   SAND,       //05
   WATER,      //06
   BEDROCK,    //07
   WOOD,       //08
   LEAVES,     //09
   PLANKS,     //10
   BRICKS     //11

};

// Classe Blocco
class Block
{
public:
   BlockType type;
   Point3D pos;
   Block() : type(BlockType::AIR), pos(0, 0, 0) {}
   Block(BlockType t, Point3D p) : type(t), pos(p) {}
};

// Classe Chunk
class Chunk
{
public:
   Point2D pos; // Coordinate del chunk (in termini di chunk, non di blocco)
   // Vettore 3D di blocchi: indici [0, CHUNK_SIZE) per x e z, [0, CHUNK_HEIGHT) per y
   std::vector<std::vector<std::vector<Block>>> blocks;
   // Mesh generata: lista piatta di vertici (ogni 3 valori rappresentano x,y,z)
   std::vector<float> meshVertices;
   // All'interno della classe Chunk, aggiungi il membro per le coordinate texture:
   std::vector<float> meshTexCoords;

   // Costruttore di default
   Chunk() : pos(0, 0)
   {
      blocks.resize(CHUNK_SIZE, std::vector<std::vector<Block>>(CHUNK_SIZE, std::vector<Block>(CHUNK_HEIGHT, Block())));
   }

   // Costruttore che riceve le coordinate del chunk
   Chunk(Point2D p) : pos(p)
   {
      blocks.resize(CHUNK_SIZE, std::vector<std::vector<Block>>(CHUNK_SIZE, std::vector<Block>(CHUNK_HEIGHT, Block())));
   }

   // Genera il chunk di test: per ogni coordinata (x, z), calcola un'altezza in base al PerlinNoise;
   // i blocchi sotto quella altezza saranno di tipo TEST, quelli sopra rimangono AIR.
   void generate(const PerlinNoise &noise)
   {
      // Parametri per una generazione in stile Minecraft
      float frequency = 0.05f;
      int baseHeight = 50;
      int amplitude = 20; // Variazione di altezza (+/- 20)
      
      for (int x = 0; x < CHUNK_SIZE; x++)
      {
          for (int z = 0; z < CHUNK_SIZE; z++)
          {
              int globalX = static_cast<int>(pos.x * CHUNK_SIZE) + x;
              int globalZ = static_cast<int>(pos.z * CHUNK_SIZE) + z;
              float n = noise.getNoise(globalX * frequency, 0.0f, globalZ * frequency);
              int surfaceHeight = baseHeight + static_cast<int>(n * amplitude);
              
              // Limita l'altezza superficiale per evitare valori fuori range
              if (surfaceHeight < 5)
                 surfaceHeight = 5;
              if (surfaceHeight >= CHUNK_HEIGHT)
                 surfaceHeight = CHUNK_HEIGHT - 1;
              
              for (int y = 0; y < CHUNK_HEIGHT; y++)
              {
                  if (y > surfaceHeight)
                  {
                      // Spazio libero sopra il terreno
                      blocks[x][z][y] = Block(BlockType::AIR, Point3D(globalX, y, globalZ));
                  }
                  else if (y == surfaceHeight)
                  {
                      // Primo blocco della superficie: GRASS
                      blocks[x][z][y] = Block(BlockType::GRASS, Point3D(globalX, y, globalZ));
                  }
                  else if (y >= surfaceHeight - 3)
                  {
                      // Strati immediatamente sotto la superficie: DIRT
                      blocks[x][z][y] = Block(BlockType::DIRT, Point3D(globalX, y, globalZ));
                  }
                  else if (y < 5)
                  {
                      // Strati bassi: BEDROCK fissa fino al livello 4
                      blocks[x][z][y] = Block(BlockType::BEDROCK, Point3D(globalX, y, globalZ));
                  }
                  else
                  {
                      // Il resto del sottosuolo: STONE
                      blocks[x][z][y] = Block(BlockType::STONE, Point3D(globalX, y, globalZ));
                  }
              }
          }
      }
   }

   // Funzione helper per aggiungere un quadrilatero (quattro vertici) alla mesh.
   void addQuadTextured(
       float x1, float y1, float z1, float u1, float v1,
       float x2, float y2, float z2, float u2, float v2,
       float x3, float y3, float z3, float u3, float v3,
       float x4, float y4, float z4, float u4, float v4)
   {
      // Primo vertice
      meshVertices.push_back(x1);
      meshVertices.push_back(y1);
      meshVertices.push_back(z1);
      meshTexCoords.push_back(u1);
      meshTexCoords.push_back(v1);
      // Secondo vertice
      meshVertices.push_back(x2);
      meshVertices.push_back(y2);
      meshVertices.push_back(z2);
      meshTexCoords.push_back(u2);
      meshTexCoords.push_back(v2);
      // Terzo vertice
      meshVertices.push_back(x3);
      meshVertices.push_back(y3);
      meshVertices.push_back(z3);
      meshTexCoords.push_back(u3);
      meshTexCoords.push_back(v3);
      // Quarto vertice
      meshVertices.push_back(x4);
      meshVertices.push_back(y4);
      meshVertices.push_back(z4);
      meshTexCoords.push_back(u4);
      meshTexCoords.push_back(v4);
   }

   // Modifica il metodo generateMesh() della classe Chunk per escludere le facce adiacenti
   void generateMesh()
   {
      meshVertices.clear();
      meshTexCoords.clear();

      auto faceVisible = [this](int x, int z, int y, int dx, int dz, int dy) -> bool
      {
         int nx = x + dx, nz = z + dz, ny = y + dy;
         if (nx < 0 || nx >= CHUNK_SIZE || nz < 0 || nz >= CHUNK_SIZE || ny < 0 || ny >= CHUNK_HEIGHT)
            return true;
         return blocks[nx][nz][ny].type == BlockType::AIR;
      };

      // Calcola le dimensioni di una singola cella dell'atlas
      float tileU = float(textureCellSize) / float(atlasWidth);
      float tileV = float(textureCellSize) / float(atlasHeight);

      for (int x = 0; x < CHUNK_SIZE; x++)
      {
         for (int z = 0; z < CHUNK_SIZE; z++)
         {
            for (int y = 0; y < CHUNK_HEIGHT; y++)
            {
               Block &block = blocks[x][z][y];
               if (block.type == BlockType::AIR)
                  continue;

               // Determina la riga dell'atlas in base al BlockType (stesso ordine dell'enumerazione)
               int typeRow = static_cast<int>(block.type);

               float bx = block.pos.x;
               float by = block.pos.y;
               float bz = block.pos.z;
               float half = 0.5f;

               // Front face (+z) : colonna 0
               if (faceVisible(x, z, y, 0, 1, 0))
               {
                  float uOffset = 0 * tileU;
                  float vOffset = typeRow * tileV;
                  addQuadTextured(
                     bx - half, by - half, bz + half, uOffset, vOffset + tileV,
                     bx + half, by - half, bz + half, uOffset + tileU, vOffset + tileV,
                     bx + half, by + half, bz + half, uOffset + tileU, vOffset,
                     bx - half, by + half, bz + half, uOffset, vOffset);
               }
               // Back face (-z) : colonna 1
               if (faceVisible(x, z, y, 0, -1, 0))
               {
                  float uOffset = 1 * tileU;
                  float vOffset = typeRow * tileV;
                  addQuadTextured(
                      bx + half, by - half, bz - half, uOffset, vOffset + tileV,
                      bx - half, by - half, bz - half, uOffset + tileU, vOffset + tileV,
                      bx - half, by + half, bz - half, uOffset + tileU, vOffset,
                      bx + half, by + half, bz - half, uOffset, vOffset);
               }
               // Left face (-x) : colonna 2
               if (faceVisible(x, z, y, -1, 0, 0))
               {
                  float uOffset = 2 * tileU;
                  float vOffset = typeRow * tileV;
                  addQuadTextured(
                      bx - half, by - half, bz - half, uOffset, vOffset + tileV,
                      bx - half, by - half, bz + half, uOffset + tileU, vOffset + tileV,
                      bx - half, by + half, bz + half, uOffset + tileU, vOffset,
                      bx - half, by + half, bz - half, uOffset, vOffset);
               }
               // Right face (+x) : colonna 3
               if (faceVisible(x, z, y, 1, 0, 0))
               {
                  float uOffset = 3 * tileU;
                  float vOffset = typeRow * tileV;
                  addQuadTextured(
                      bx + half, by - half, bz + half, uOffset, vOffset + tileV,
                      bx + half, by - half, bz - half, uOffset + tileU, vOffset + tileV,
                      bx + half, by + half, bz - half, uOffset + tileU, vOffset,
                      bx + half, by + half, bz + half, uOffset, vOffset);
               }
               // Top face (+y) : colonna 4
               if (faceVisible(x, z, y, 0, 0, 1))
               {
                  float uOffset = 4 * tileU;
                  float vOffset = typeRow * tileV;
                  addQuadTextured(
                      bx - half, by + half, bz + half, uOffset, vOffset,
                      bx + half, by + half, bz + half, uOffset + tileU, vOffset,
                      bx + half, by + half, bz - half, uOffset + tileU, vOffset + tileV,
                      bx - half, by + half, bz - half, uOffset, vOffset + tileV);
               }
               // Bottom face (-y) : colonna 5
               if (faceVisible(x, z, y, 0, 0, -1))
               {
                  float uOffset = 5 * tileU;
                  float vOffset = typeRow * tileV;
                  addQuadTextured(
                      bx - half, by - half, bz - half, uOffset, vOffset,
                      bx + half, by - half, bz - half, uOffset + tileU, vOffset,
                      bx + half, by - half, bz + half, uOffset + tileU, vOffset + tileV,
                      bx - half, by - half, bz + half, uOffset, vOffset + tileV);
               }
            }
         }
      }
   }

   // Modifica il metodo drawTextured() della classe Chunk per utilizzare anche le coordinate texture:
   void drawTextured() const
   {
      glBindTexture(GL_TEXTURE_2D, blockTexture);
      glPushMatrix();
      // Rimuovi la traslazione poiché le coordinate dei blocchi sono già globali
      // glTranslatef(pos.x * CHUNK_SIZE, 0.0f, pos.z * CHUNK_SIZE); 
      glBegin(GL_QUADS);
      for (size_t i = 0, j = 0; i < meshVertices.size(); i += 3, j += 2)
      {
         glTexCoord2f(meshTexCoords[j], meshTexCoords[j + 1]);
         glVertex3f(meshVertices[i], meshVertices[i + 1], meshVertices[i + 2]);
      }
      glEnd();
      glPopMatrix();
   }
};

namespace std
{
   template <>
   struct hash<Point2D>
   {
      size_t operator()(const Point2D &p) const
      {
         // Combinazione hash delle coordinate x e z
         return hash<float>()(p.x) ^ (hash<float>()(p.z) << 1);
      }
   };
}

// Aggiorna la classe World per includere i metodi utili
class World
{
public:
   std::unordered_map<Point2D, Chunk> chunksMap;
   int generationSeed;
   Point3D spawnPoint;

   // Determina le coordinate del chunk in cui cade un punto nel mondo
   Point2D getChunkCoordinates(const Point3D &pos) const
   {
      return Point2D(std::floor(pos.x / CHUNK_SIZE), std::floor(pos.z / CHUNK_SIZE));
   }

   // Stub per aggiornare i chunk visibili in base alla camera e alla render distance.
   // Puoi estendere questa funzione per generare nuovi chunk man mano che la camera si muove.
   void updateVisibleChunks(const Camera &camera, int renderDistance)
   {
      // Attualmente non fa nulla in quanto si usa un chunk di test fisso.
   }

   // Metodo per generare una griglia di chunk
   void generateChunkGrid(int gridSize, const PerlinNoise &noise)
   {
      for (int i = -gridSize; i <= gridSize; ++i)
      {
         for (int j = -gridSize; j <= gridSize; ++j)
         {
            Point2D chunkPos(i, j);
            Chunk chunk(chunkPos);
            chunk.generate(noise);
            chunk.generateMesh();
            chunksMap[chunkPos] = chunk;
         }
      }
   }
};

class UIRenderer
{
public:
   static void drawText(const std::string &text, Point2D pos, void *font = GLUT_BITMAP_HELVETICA_10);
};

// ================================
// PROTOTIPI DELLE FUNZIONI
// ================================
void init();
float toRadians(float degrees);
bool isFaceVisible(const Point3D &blockPos, const Point3D &faceNormalVect);

void display();

void keyboard(unsigned char key, int x, int y);
void mouseMotion(int x, int y);
void resetMousePosition();

// Aggiungi una semplice implementazione per checkWorldIntegrity() (stub)
void checkWorldIntegrity()
{
   // Per ora non esegue controlli particolari
}

// Funzione per caricare la texture
void loadTextures()
{
   int width, height, channels;
   unsigned char *data = stbi_load("textures/textures.png", &width, &height, &channels, STBI_rgb_alpha);
   if (data)
   {
      atlasWidth = width;
      atlasHeight = height;
      glGenTextures(1, &blockTexture);
      glBindTexture(GL_TEXTURE_2D, blockTexture);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

      stbi_image_free(data);
   }
   else
   {
      std::cout << "Errore: impossibile caricare textures/textures.png" << std::endl;
      exit(EXIT_FAILURE);
   }
}

// ================================
// VARIABILI GLOBALI
// ================================
Camera camera;
World world; // Dichiarazione della variabile globale
bool showChunkBorder = false;
bool showData = true;
bool showTopFace = true;
bool enableFaceOptimization = true;
bool wireframeMode = false;         // Variabile globale per la modalità wireframe
bool enableShadows = false;         // Aggiungi una variabile globale per le ombre
int lastMouseX = 0, lastMouseY = 0; // Variabili globali per tracciare la posizione precedente del mouse
// Variabili globali per il calcolo degli FPS
std::chrono::steady_clock::time_point lastFrameTime = std::chrono::steady_clock::now();
float fps = 0.0f;

// Aggiungi all'inizio, accanto alle altre variabili globali:
bool cameraMovementEnabled = true;

// ================================
// FUNZIONE PRINCIPALE
// ================================
int main(int argc, char **argv)
{
   glutInit(&argc, argv);
   glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
   glutInitWindowSize(800, 600);
   glutCreateWindow("MineGLaft");

   // Inizializza il programma
   init();

   checkWorldIntegrity();

   // Registra le funzioni di callback
   glutDisplayFunc(display);
   glutKeyboardFunc(keyboard);
   // Rimuovi o commenta glutMouseFunc(processMouse);
   // Sostituisci glutMotionFunc(motionMouse) e glutPassiveMotionFunc(passiveMouse) con:
   glutMotionFunc(mouseMotion);
   glutPassiveMotionFunc(mouseMotion);

   // Avvia il ciclo principale di GLUT
   glutMainLoop();
   return 0;
}

// ================================
// IMPLEMENTAZIONE DELLE FUNZIONI
// ================================

// Modifica in init(): carica le textures e abilita il texturing
void init()
{
   glEnable(GL_DEPTH_TEST);
   glClearColor(0.0f, 1.0f, 1.0f, 1.0f);

   // Disabilitiamo l'illuminazione
   glDisable(GL_LIGHTING);
   // Abilitiamo il texturing 2D
   glEnable(GL_TEXTURE_2D);
   loadTextures();

   camera.reset();
   //std::cout << "Inserisci il seme per la generazione del mondo: ";
   //std::cin >> world.generationSeed; // Chiede all'utente di inserire il seme per il Per
   world.generationSeed = 12345; // Seme per PerlinNoise

   // Genera una griglia 5x5 di chunk
   PerlinNoise noise(world.generationSeed);
   world.generateChunkGrid(5, noise);
}

void Camera::reset()
{
   pos = Point3D(-4 * CHUNK_SIZE, 10 * CHUNK_SIZE, -4 * CHUNK_SIZE);
   rot = Rotation(-30.0f, 45.0f, 0.0f);
   glutPostRedisplay();
}

bool Camera::isFaceVisible(const Point3D &blockPos, const Point3D &faceNormalVect) const
{
   Point3D directionVect(blockPos.x - pos.x, blockPos.y - pos.y, blockPos.z - pos.z);
   directionVect.normalize();
   float dotProduct = directionVect.x * faceNormalVect.x + directionVect.y * faceNormalVect.y + directionVect.z * faceNormalVect.z;
   return dotProduct < 0.0f;
}

// Funzione per convertire gradi in radianti
float toRadians(float degrees)
{
   return degrees * M_PI / 180.0f;
}

void UIRenderer::drawText(const std::string &text, Point2D pos, void *font)
{
   glDisable(GL_TEXTURE_2D); // Disabilita il texturing per evitare che colori e texture influenzino il testo
   glDisable(GL_LIGHTING);
   glColor3f(1.0f, 1.0f, 1.0f); // Forza il colore bianco

   glRasterPos2f(pos.x, pos.z);
   for (char c : text)
   {
      glutBitmapCharacter(font, c);
   }
   glEnable(GL_TEXTURE_2D);  // Riabilita il texturing
   glEnable(GL_LIGHTING);
}

// Modifica in display(): forza la disabilitazione dell'illuminazione e commenta il rendering delle ombre
void display()
{
   // Se wireframe, imposta lo sfondo nero; altrimenti usa il colore originale.
   if (wireframeMode)
   {
      glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
      glDisable(GL_LIGHTING);
      glColor3f(1.0f, 1.0f, 1.0f);
   }
   else
   {
      glClearColor(0.0f, 1.0f, 1.0f, 1.0f);
      glDisable(GL_LIGHTING);
   }

   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

   auto currentFrameTime = std::chrono::steady_clock::now();
   float deltaTime = std::chrono::duration<float>(currentFrameTime - lastFrameTime).count();
   lastFrameTime = currentFrameTime;

   if (deltaTime > 0.0f)
   {
      fps = 1.0f / deltaTime;
   }

   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

   glMatrixMode(GL_PROJECTION);
   glLoadIdentity();
   gluPerspective(45.0f, 1.0f * glutGet(GLUT_WINDOW_WIDTH) / glutGet(GLUT_WINDOW_HEIGHT), 0.1f, 500.0f);

   glMatrixMode(GL_MODELVIEW);
   glLoadIdentity();

   float cosPitch = cos(toRadians(camera.rot.xRot));
   float sinPitch = sin(toRadians(camera.rot.xRot));
   float cosYaw = cos(toRadians(camera.rot.yRot));
   float sinYaw = sin(toRadians(camera.rot.yRot));

   float lookX = camera.pos.x + cosPitch * cosYaw;
   float lookY = camera.pos.y + sinPitch;
   float lookZ = camera.pos.z + cosPitch * sinYaw;

   gluLookAt(
       camera.pos.x, camera.pos.y, camera.pos.z,
       lookX, lookY, lookZ,
       0.0f, 1.0f, 0.0f);

   // Calcola i chunk visibili entro la render_distance
   Point2D currentChunkCoords = world.getChunkCoordinates(camera.pos);

   for (int i = -RENDER_DISTANCE; i <= RENDER_DISTANCE; i++)
   {
      for (int j = -RENDER_DISTANCE; j <= RENDER_DISTANCE; j++)
      {
         Point2D chunkPos(currentChunkCoords.x + i, currentChunkCoords.z + j);

         // Cerca il chunk nella mappa
         auto it = world.chunksMap.find(chunkPos);
         if (it != world.chunksMap.end())
         {

            // Utilizza il nuovo metodo per disegnare con textures
            it->second.drawTextured();

            if (showChunkBorder)
            {
               //glDisable(GL_LIGHTING); // Disabilita la luce
               glColor3f(0, 0, 0);     // Imposta il colore delle linee a grigio
               glLineWidth(2.0f);
               glBegin(GL_LINES);
               float lineHeight = 500.0f;
               float xMin = it->second.pos.x - 0.5f;
               float xMax = it->second.pos.x + CHUNK_SIZE - 0.5f;
               float zMin = it->second.pos.z - 0.5f;
               float zMax = it->second.pos.z + CHUNK_SIZE - 0.5f;

               // Linee verticali delimitanti il chunk
               glVertex3f(xMin, 0.0f, zMin);
               glVertex3f(xMin, lineHeight, zMin);
               glVertex3f(xMax, 0.0f, zMin);
               glVertex3f(xMax, lineHeight, zMin);
               glVertex3f(xMax, 0.0f, zMax);
               glVertex3f(xMax, lineHeight, zMax);
               glVertex3f(xMin, 0.0f, zMax);
               glVertex3f(xMin, lineHeight, zMax);
               glEnd();
               glEnable(GL_LIGHTING); // Riattiva la luce
            }
         }
      }
   }

   if (showData)
   {
      glMatrixMode(GL_PROJECTION);
      glPushMatrix();
      glLoadIdentity();
      glOrtho(0, glutGet(GLUT_WINDOW_WIDTH), 0, glutGet(GLUT_WINDOW_HEIGHT), -1, 1);
      glMatrixMode(GL_MODELVIEW);
      glPushMatrix();
      glLoadIdentity();

      // Disabilita la luce e abilita il blending per il rettangolo semitrasparente
      glDisable(GL_LIGHTING);
      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

      // Disabilita lo Z-buffer per evitare conflitti
      glDisable(GL_DEPTH_TEST);

      // Imposta il colore del rettangolo (grigio semitrasparente)
      glColor4f(0.2f, 0.2f, 0.2f, 0.7f); // RGBA: Grigio con alpha 0.7

      // Disegna il rettangolo
      glBegin(GL_QUADS);
      glVertex2f(0, glutGet(GLUT_WINDOW_HEIGHT) - 80);   // Alto sinistro
      glVertex2f(0, glutGet(GLUT_WINDOW_HEIGHT));        // Basso sinistro
      glVertex2f(300, glutGet(GLUT_WINDOW_HEIGHT));      // Basso destro
      glVertex2f(300, glutGet(GLUT_WINDOW_HEIGHT) - 80); // Alto destro
      glEnd();

      // Riabilita lo Z-buffer dopo aver disegnato il rettangolo
      glEnable(GL_DEPTH_TEST);

      // Disabilita il blending dopo aver disegnato il rettangolo
      glDisable(GL_BLEND);

      // Imposta il colore del testo a bianco
      glColor3f(1.0f, 1.0f, 1.0f);

      // Disegna i testi sopra il rettangolo
      UIRenderer::drawText("FPS: " + std::to_string(static_cast<int>(fps)), Point2D(10, glutGet(GLUT_WINDOW_HEIGHT) - 15));
      UIRenderer::drawText("Rotazione Camera: (" + std::to_string(camera.rot.xRot) + ", " + std::to_string(camera.rot.yRot) + ", " + std::to_string(camera.rot.zRot) + ")", Point2D(10, glutGet(GLUT_WINDOW_HEIGHT) - 30));
      UIRenderer::drawText("Posizione: (" + std::to_string(camera.pos.x) + ", " + std::to_string(camera.pos.y) + ", " + std::to_string(camera.pos.z) + ")", Point2D(10, glutGet(GLUT_WINDOW_HEIGHT) - 45));
      Point2D chunkCoords = world.getChunkCoordinates(camera.pos);
      UIRenderer::drawText("Chunk corrente: (" + std::to_string(chunkCoords.x) + "," + std::to_string(chunkCoords.z) + ")", Point2D(10, glutGet(GLUT_WINDOW_HEIGHT) - 60));
      UIRenderer::drawText("Seed: " + std::to_string(world.generationSeed), Point2D(10, glutGet(GLUT_WINDOW_HEIGHT) - 75));

      // Ripristina le impostazioni OpenGL
      glPopMatrix();
      glMatrixMode(GL_PROJECTION);
      glPopMatrix();
      glMatrixMode(GL_MODELVIEW);
   }

   glutSwapBuffers();
}

// Modifica in keyboard(): rimuovi (o commenta) il caso 'v' per evitare che si possano riattivare ombre
void keyboard(unsigned char key, int, int)
{
   float horizontalStep = 2.0f;
   float verticalStep = 2.0f;
   bool moved = false;

   switch (key)
   {
      case 27: // ESC: abilita/disabilita il movimento della camera
         cameraMovementEnabled = !cameraMovementEnabled;
         if (cameraMovementEnabled)
         {
            // Nasconde il cursore quando il movimento è abilitato
            glutSetCursor(GLUT_CURSOR_NONE);
            resetMousePosition(); // Assicura che il cursore sia centrato
         }
         else
         {
            // Mostra il cursore quando il movimento viene disabilitato
            glutSetCursor(GLUT_CURSOR_LEFT_ARROW);
         }
         break;
      // ... gli altri casi rimangono invariati ...
      case 'q':
      case 'Q':
         wireframeMode = !wireframeMode;
         if (wireframeMode)
         {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
         }
         else
         {
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
         }
         glutPostRedisplay();
         break;
      case 'w':
         camera.pos.x += horizontalStep * cos(toRadians(camera.rot.yRot));
         camera.pos.z += horizontalStep * sin(toRadians(camera.rot.yRot));
         moved = true;
         break;
      case 's':
         camera.pos.x -= horizontalStep * cos(toRadians(camera.rot.yRot));
         camera.pos.z -= horizontalStep * sin(toRadians(camera.rot.yRot));
         moved = true;
         break;
      case 'd':
         camera.pos.x -= horizontalStep * sin(toRadians(camera.rot.yRot));
         camera.pos.z += horizontalStep * cos(toRadians(camera.rot.yRot));
         moved = true;
         break;
      case 'a':
         camera.pos.x += horizontalStep * sin(toRadians(camera.rot.yRot));
         camera.pos.z -= horizontalStep * cos(toRadians(camera.rot.yRot));
         moved = true;
         break;
      case ' ':
         camera.pos.y += verticalStep;
         moved = true;
         break;
      case 'S':
         camera.pos.y -= verticalStep;
         moved = true;
         break;
      case 'r':
         camera.reset();
         moved = true;
         break;
      case 'b':
         showChunkBorder = !showChunkBorder;
         break;
      case 'n':
         showData = !showData;
         break;
      case 'p':
         // world.placeBlock();
         break;
   }

   if (moved)
   {
      world.updateVisibleChunks(camera, RENDER_DISTANCE); // Aggiorna i chunk visibili
   }

   glutPostRedisplay();
}

// Funzione per gestire i clic del mouse
void processMouse(int button, int state, int x, int y)
{
   if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN)
   {
      lastMouseX = x;
      lastMouseY = y;
   }
}

void mouseMotion(int x, int y)
{
   if (cameraMovementEnabled)
   {
      int deltaX = x - lastMouseX;
      int deltaY = y - lastMouseY;

      camera.rot.yRot += static_cast<float>(deltaX) * 0.25f; // Rotazione orizzontale
      camera.rot.xRot -= static_cast<float>(deltaY) * 0.25f; // Rotazione verticale

      // Limita la rotazione verticale
      camera.rot.xRot = std::max(-89.9f, std::min(89.9f, camera.rot.xRot));
   }

   lastMouseX = x;
   lastMouseY = y;
   if (cameraMovementEnabled)
      resetMousePosition(); // Riporta il cursore al centro
   glutPostRedisplay();
}

// Funzione per reimpostare la posizione del mouse al centro della finestra
void resetMousePosition()
{
   int windowWidth = glutGet(GLUT_WINDOW_WIDTH);
   int windowHeight = glutGet(GLUT_WINDOW_HEIGHT);
   int centerX = windowWidth / 2;
   int centerY = windowHeight / 2;

   glutWarpPointer(centerX, centerY); // Sposta il cursore al centro
   lastMouseX = centerX;
   lastMouseY = centerY;
}
