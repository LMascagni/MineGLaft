// ================================
// LIBRERIE
// ================================
#include <GL/glew.h>
#include <GL/glut.h>
#include <cmath>
#include <vector>
#include <random>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <array>
#include <iostream>
#include <string>
#include <chrono>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>

namespace fs = std::filesystem;

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
const int CHUNK_HEIGHT = 256;
const int RENDER_DISTANCE = 8;

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

   void reset()
   {
      pos = Point3D(0, 125, 0);
      rot = Rotation(-30.0f, 45.0f, 0.0f);
      glutPostRedisplay();
   }

   bool isFaceVisible(const Point3D &blockPos, const Point3D &faceNormalVect) const
   {
      Point3D directionVect(blockPos.x - pos.x, blockPos.y - pos.y, blockPos.z - pos.z);
      directionVect.normalize();
      float dotProduct = directionVect.x * faceNormalVect.x + directionVect.y * faceNormalVect.y + directionVect.z * faceNormalVect.z;
      return dotProduct < 0.0f;
   }
};

// Dichiarazione anticipata della classe Chunk
class Chunk;

// Enumerazione globale per i tipi di blocco
enum class BlockType
{
   TEST,        // 00
   AIR,         // 01
   GRASS,       // 02
   DIRT,        // 03
   STONE,       // 04
   SAND,        // 05
   WATER,       // 06
   BEDROCK,     // 07
   WOOD,        // 08
   LEAVES,      // 09
   PLANKS,      // 10
   COBBLESTONE, // 11
   BRICKS,      // 12

   BLOCK_COUNT

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

   // Aggiungi i buffer OpenGL
   GLuint vao = 0;
   GLuint vbo = 0;

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

   
   
   // Funzione per generare il terreno del chunk
   void generate(const PerlinNoise &noise)
   {
      // Parametri esistenti
      float baseFrequency = 0.01f;
      int baseHeight = 128;
      int amplitude = 200;
      int octaves = 5;
      float persistence = 0.5f;
      float biomeFrequency = 0.001f;
      const int WATER_LEVEL = 110;
      const int BEACH_RANGE = 2; // Range di altezza per la spiaggia sopra il livello dell'acqua

      // Nuovo parametro per il rumore della sabbia
      float sandNoiseFrequency = 0.05f; // Frequenza più alta per variazioni più piccole

      for (int x = 0; x < CHUNK_SIZE; x++)
      {
         for (int z = 0; z < CHUNK_SIZE; z++)
         {
            int globalX = static_cast<int>(pos.x * CHUNK_SIZE) + x;
            int globalZ = static_cast<int>(pos.z * CHUNK_SIZE) + z;

            float biomeValue = noise.getNoise(globalX * biomeFrequency, 0.0f, globalZ * biomeFrequency);
            biomeValue = (biomeValue + 1.0f) / 2.0f;

            int localBaseHeight = static_cast<int>(baseHeight * (0.7f + 0.3f * biomeValue));
            int localAmplitude = static_cast<int>(amplitude * (0.1f + 0.9f * biomeValue));

            float totalNoise = 0.0f;
            float maxAmplitude = 0.0f;
            float frequency = baseFrequency;
            float amplitudeLayer = 1.0f;

            for (int i = 0; i < octaves; ++i)
            {
               totalNoise += noise.getNoise(globalX * frequency, 0.0f, globalZ * frequency) * amplitudeLayer;
               maxAmplitude += amplitudeLayer;
               amplitudeLayer *= persistence;
               frequency *= 2.0f;
            }

            totalNoise /= maxAmplitude;
            int surfaceHeight = localBaseHeight + static_cast<int>(totalNoise * localAmplitude);

            if (surfaceHeight < 5)
               surfaceHeight = 5;
            if (surfaceHeight >= CHUNK_HEIGHT)
               surfaceHeight = CHUNK_HEIGHT - 1;

            // Calcola il rumore per la distribuzione della sabbia
            float sandNoise = noise.getNoise(globalX * sandNoiseFrequency, 0.0f, globalZ * sandNoiseFrequency);
            sandNoise = (sandNoise + 1.0f) / 2.0f; // Normalizza a [0,1]

            for (int y = 0; y < CHUNK_HEIGHT; y++)
            {
               if (y > surfaceHeight)
               {
                  // Se siamo sopra il terreno ma sotto il livello dell'acqua, metti acqua
                  if (y <= WATER_LEVEL)
                  {
                     blocks[x][z][y] = Block(BlockType::WATER, Point3D(globalX, y, globalZ));
                  }
                  else
                  {
                     blocks[x][z][y] = Block(BlockType::AIR, Point3D(globalX, y, globalZ));
                  }
               }
               else if (y == surfaceHeight)
               {
                  // Se siamo al livello della superficie
                  if (y <= WATER_LEVEL + BEACH_RANGE && y >= WATER_LEVEL - BEACH_RANGE)
                  {
                     // Usa il sandNoise per decidere se mettere sabbia o erba
                     if (sandNoise > 0.4f) // Regola questa soglia per più o meno sabbia
                     {
                        blocks[x][z][y] = Block(BlockType::SAND, Point3D(globalX, y, globalZ));
                     }
                     else
                     {
                        // Se siamo sotto il livello dell'acqua, mettiamo terra invece che erba
                        if (y < WATER_LEVEL)
                        {
                           blocks[x][z][y] = Block(BlockType::DIRT, Point3D(globalX, y, globalZ));
                        }
                        else
                        {
                           blocks[x][z][y] = Block(BlockType::GRASS, Point3D(globalX, y, globalZ));
                        }
                     }
                  }
                  else if (y < WATER_LEVEL)
                  {
                     // Sotto il livello dell'acqua, usa sempre sabbia
                     blocks[x][z][y] = Block(BlockType::SAND, Point3D(globalX, y, globalZ));
                  }
                  else
                  {
                     blocks[x][z][y] = Block(BlockType::GRASS, Point3D(globalX, y, globalZ));
                  }
               }
               else if (y >= surfaceHeight - 3)
               {
                  // Anche per gli strati sotto la superficie, usa il noise per decidere
                  if (surfaceHeight <= WATER_LEVEL + BEACH_RANGE && sandNoise > 0.4f)
                  {
                     blocks[x][z][y] = Block(BlockType::SAND, Point3D(globalX, y, globalZ));
                  }
                  else
                  {
                     blocks[x][z][y] = Block(BlockType::DIRT, Point3D(globalX, y, globalZ));
                  }
               }
               else if (y < 5)
               {
                  blocks[x][z][y] = Block(BlockType::BEDROCK, Point3D(globalX, y, globalZ));
               }
               else
               {
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

      // Ora prepara i dati interlacciati (x,y,z,u,v)
      std::vector<float> interleaved;
      size_t numVertices = meshVertices.size() / 3;
      for (size_t i = 0; i < numVertices; i++)
      {
         interleaved.push_back(meshVertices[i * 3 + 0]);
         interleaved.push_back(meshVertices[i * 3 + 1]);
         interleaved.push_back(meshVertices[i * 3 + 2]);
         interleaved.push_back(meshTexCoords[i * 2 + 0]);
         interleaved.push_back(meshTexCoords[i * 2 + 1]);
      }

      // Genera/aggiorna VAO e VBO
      if (vao == 0)
      {
         glGenVertexArrays(1, &vao);
      }
      glBindVertexArray(vao);

      if (vbo == 0)
      {
         glGenBuffers(1, &vbo);
      }
      glBindBuffer(GL_ARRAY_BUFFER, vbo);
      glBufferData(GL_ARRAY_BUFFER, interleaved.size() * sizeof(float), interleaved.data(), GL_STATIC_DRAW);

      // Abilita e definisci l'attributo per la posizione (location 0)
      glEnableVertexAttribArray(0);
      glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)0);
      // Abilita e definisci l'attributo per le coordinate texture (location 1)
      glEnableVertexAttribArray(1);
      glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)(3 * sizeof(float)));

      // Unbind
      glBindBuffer(GL_ARRAY_BUFFER, 0);
      glBindVertexArray(0);
   }

   // Modifica il metodo drawTextured() della classe Chunk per utilizzare anche le coordinate texture:
   void drawTextured() const
   {
      if (vao == 0)
         return; // Nessun dato caricato

      glBindTexture(GL_TEXTURE_2D, blockTexture);
      glBindVertexArray(vao);
      // Considerando che ogni quadrilatero è formato da 4 vertici,
      // il numero totale di vertici è:
      int totalVertices = static_cast<int>(meshVertices.size() / 3);

      glBindVertexArray(0); // Disabilita momentaneamente il VAO

      glEnableClientState(GL_VERTEX_ARRAY);
      glEnableClientState(GL_TEXTURE_COORD_ARRAY);

      glBindBuffer(GL_ARRAY_BUFFER, vbo);
      glVertexPointer(3, GL_FLOAT, 5 * sizeof(float), (void *)0);
      glTexCoordPointer(2, GL_FLOAT, 5 * sizeof(float), (void *)(3 * sizeof(float)));

      glDrawArrays(GL_QUADS, 0, totalVertices);

      glBindBuffer(GL_ARRAY_BUFFER, 0);
      glDisableClientState(GL_TEXTURE_COORD_ARRAY);
      glDisableClientState(GL_VERTEX_ARRAY);
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
   Camera camera;
   std::unordered_map<Point2D, Chunk> chunksMap;
   int generationSeed;
   Point3D spawnPoint;
   std::string currentWorldName; // Add this as a class member

   // Determina le coordinate del chunk in cui cade un punto nel mondo
   Point2D getChunkCoordinates(const Point3D &pos) const
   {
      return Point2D(std::floor(pos.x / CHUNK_SIZE), std::floor(pos.z / CHUNK_SIZE));
   }

   // Stub per aggiornare i chunk visibili in base alla camera e alla render distance.
   // Puoi estendere questa funzione per generare nuovi chunk man mano che la camera si muove.
   void updateVisibleChunks(const Camera &camera, int renderDistance)
   {
      Point2D currentChunkCoords = getChunkCoordinates(camera.pos);
      std::unordered_set<Point2D> newChunkCoords;

      for (int dx = -renderDistance; dx <= renderDistance; ++dx)
      {
         for (int dz = -renderDistance; dz <= renderDistance; ++dz)
         {
            Point2D chunkCoords(currentChunkCoords.x + dx, currentChunkCoords.z + dz);
            newChunkCoords.insert(chunkCoords);

            if (chunksMap.find(chunkCoords) == chunksMap.end())
            {
               Chunk chunk(chunkCoords);
               if (loadChunk(chunkCoords, chunk))
               {
                  chunksMap[chunkCoords] = chunk;
               }
               else
               {
                  generateChunk(chunkCoords);                  
               }
            }
         }
      }

      // Rimuovi i chunk che non sono più necessari
      for (auto it = chunksMap.begin(); it != chunksMap.end();)
      {
         if (newChunkCoords.find(it->first) == newChunkCoords.end())
         {
            unloadChunk(it->first);
            it = chunksMap.erase(it);
         }
         else
         {
            ++it;
         }
      }
   }

   // Metodo per generare un singolo chunk
   void generateChunk(const Point2D &pos)
   {
      PerlinNoise noise(generationSeed);
      Chunk chunk(pos);
      chunk.generate(noise);
      chunk.generateMesh();
      chunksMap[pos] = chunk;
      saveChunk(pos, chunk);
   }

   void unloadChunk(const Point2D &pos)
   {
      // Dealloca la memoria del chunk se necessario


      // In questo caso, non c'è nulla di specifico da fare, ma puoi aggiungere logica qui se necessario
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
            saveChunk(chunkPos, chunk); // Save the chunk immediately after generation
         }
      }
   }

   // New function prototype:
   void placeBlock(const Point3D &pos, BlockType type);
   void initializeWorld(const std::string &worldName)
   {
      currentWorldName = worldName;
      fs::path worldPath = fs::path("worlds") / worldName;
      fs::create_directories(worldPath);
      fs::create_directories(worldPath / "chunks");

      // Save initial world info
      saveWorldInfo();
   }

   void saveWorldInfo()
   {
      if (currentWorldName.empty())
         return;

      fs::path worldPath = fs::path("worlds") / currentWorldName;
      std::ofstream worldInfo(worldPath / "world.info");
      worldInfo << "seed " << generationSeed << "\n";
      worldInfo.close();
   }

   void saveChunk(const Point2D &pos, const Chunk &chunk)
   {
      if (currentWorldName.empty())
         return;

      fs::path worldPath = fs::path("worlds") / currentWorldName;
      std::stringstream chunkFileName;
      chunkFileName << "chunk_" << pos.x << "_" << pos.z << ".dat";

      std::ofstream chunkFile(worldPath / "chunks" / chunkFileName.str(), std::ios::binary);

      // Save chunk data
      for (int x = 0; x < CHUNK_SIZE; x++)
      {
         for (int z = 0; z < CHUNK_SIZE; z++)
         {
            for (int y = 0; y < CHUNK_HEIGHT; y++)
            {
               int blockType = static_cast<int>(chunk.blocks[x][z][y].type);
               chunkFile.write(reinterpret_cast<char *>(&blockType), sizeof(int));
            }
         }
      }
      chunkFile.close();
   }

   // Add this new method to the World class
   bool loadWorld(const std::string &worldName)
   {
      fs::path worldPath = fs::path("worlds") / worldName;
      if (!fs::exists(worldPath))
      {
         std::cerr << "World '" << worldName << "' does not exist." << std::endl;
         return false;
      }

      // Load world info
      std::ifstream worldInfo(worldPath / "world.info");
      std::string token;
      worldInfo >> token >> generationSeed;
      worldInfo.close();

      currentWorldName = worldName;

      // Clear existing chunks
      chunksMap.clear();

      // Load all chunks from the chunks directory
      fs::path chunksPath = worldPath / "chunks";
      for (const auto &entry : fs::directory_iterator(chunksPath))
      {
         if (entry.path().extension() == ".dat")
         {
            std::string filename = entry.path().filename().string();
            int x, z;
            if (sscanf(filename.c_str(), "chunk_%d_%d.dat", &x, &z) == 2)
            {
               Point2D chunkPos(x, z);
               Chunk chunk(chunkPos);

               std::ifstream chunkFile(entry.path(), std::ios::binary);
               for (int x = 0; x < CHUNK_SIZE; x++)
               {
                  for (int z = 0; z < CHUNK_SIZE; z++)
                  {
                     for (int y = 0; y < CHUNK_HEIGHT; y++)
                     {
                        int blockType;
                        chunkFile.read(reinterpret_cast<char *>(&blockType), sizeof(int));
                        chunk.blocks[x][z][y].type = static_cast<BlockType>(blockType);
                        chunk.blocks[x][z][y].pos = Point3D(
                            chunkPos.x * CHUNK_SIZE + x,
                            y,
                            chunkPos.z * CHUNK_SIZE + z);
                     }
                  }
               }
               chunk.generateMesh();
               chunksMap[chunkPos] = chunk;
               //std::cout << "Chunk " << x << ", " << z << " caricato." << std::endl;
            }
         }
      }

      //std::cout << "Sono stati caricati " << chunksMap.size() << " chunks da '" << worldName << "'" << std::endl;
      return true;
   }

   bool loadChunk(const Point2D &pos, Chunk &chunk)
   {
      if (currentWorldName.empty())
         return false;

      fs::path worldPath = fs::path("worlds") / currentWorldName;
      std::stringstream chunkFileName;
      chunkFileName << "chunk_" << pos.x << "_" << pos.z << ".dat";

      std::ifstream chunkFile(worldPath / "chunks" / chunkFileName.str(), std::ios::binary);
      if (!chunkFile.is_open())
         return false;

      for (int x = 0; x < CHUNK_SIZE; x++)
      {
         for (int z = 0; z < CHUNK_SIZE; z++)
         {
            for (int y = 0; y < CHUNK_HEIGHT; y++)
            {
               int blockType;
               chunkFile.read(reinterpret_cast<char *>(&blockType), sizeof(int));
               chunk.blocks[x][z][y].type = static_cast<BlockType>(blockType);
               chunk.blocks[x][z][y].pos = Point3D(
                   pos.x * CHUNK_SIZE + x,
                   y,
                   pos.z * CHUNK_SIZE + z);
            }
         }
      }
      chunkFile.close();
      chunk.generateMesh();
      return true;
   }
};

void World::placeBlock(const Point3D &pos, BlockType type)
{
   // Arrotonda le coordinate della posizione a interi
   int blockX = static_cast<int>(std::round(pos.x));
   int blockY = static_cast<int>(std::round(pos.y));
   int blockZ = static_cast<int>(std::round(pos.z));

   // Calcola le coordinate del chunk usando il floor
   int chunkX = static_cast<int>(std::floor(blockX / static_cast<float>(CHUNK_SIZE)));
   int chunkZ = static_cast<int>(std::floor(blockZ / static_cast<float>(CHUNK_SIZE)));
   Point2D chunkCoords(chunkX, chunkZ);

   auto it = chunksMap.find(chunkCoords);
   if (it == chunksMap.end())
   {
      return;
   }

   // Calcola le coordinate locali all'interno del chunk.
   int localX = blockX - chunkX * CHUNK_SIZE;
   int localZ = blockZ - chunkZ * CHUNK_SIZE;
   int localY = blockY;

   // Controlla i limiti
   if (localX < 0 || localX >= CHUNK_SIZE ||
       localZ < 0 || localZ >= CHUNK_SIZE ||
       localY < 0 || localY >= CHUNK_HEIGHT)
   {
      return;
   }

   // Aggiorna il blocco nel chunk corrente.
   it->second.blocks[localX][localZ][localY] = Block(type, Point3D(blockX, blockY, blockZ));
   it->second.generateMesh();
   saveChunk(chunkCoords, it->second); // Save the chunk after modification

   // Aggiorna la mesh dei chunk adiacenti se il blocco tocca il bordo.
   int directions[6][3] = {
       {-1, 0, 0},
       {1, 0, 0},
       {0, 0, -1},
       {0, 0, 1},
       {0, -1, 0},
       {0, 1, 0} // Anche se il cambio verticale potrebbe non influire su facce laterali
   };

   for (int i = 0; i < 6; i++)
   {
      int nx = blockX + directions[i][0];
      // int ny = blockY + directions[i][1];
      int nz = blockZ + directions[i][2];
      int neighborChunkX = static_cast<int>(std::floor(nx / static_cast<float>(CHUNK_SIZE)));
      int neighborChunkZ = static_cast<int>(std::floor(nz / static_cast<float>(CHUNK_SIZE)));
      // Se il blocco adiacente appartiene ad un chunk diverso, aggiorna la sua mesh.
      if (neighborChunkX != chunkX || neighborChunkZ != chunkZ)
      {
         Point2D neighborCoords(neighborChunkX, neighborChunkZ);
         auto neighborIt = chunksMap.find(neighborCoords);
         if (neighborIt != chunksMap.end())
         {
            neighborIt->second.generateMesh();
         }
      }
   }
}

class UIRenderer
{
public:
   void drawText(const std::string &text, Point2D pos, void *font)
   {
      glDisable(GL_TEXTURE_2D); // Disabilita il texturing per evitare che colori e texture influenzino il testo
      glDisable(GL_LIGHTING);
      glColor3f(1.0f, 1.0f, 1.0f); // Forza il colore bianco

      glRasterPos2f(pos.x, pos.z);
      for (char c : text)
      {
         glutBitmapCharacter(font, c);
      }
      glEnable(GL_TEXTURE_2D); // Riabilita il texturing
      glEnable(GL_LIGHTING);
   }
};

// Aggiungi questa funzione insieme alle altre funzioni helper
std::string blockTypeToString(BlockType type)
{
   switch (type)
   {
   case BlockType::TEST:
      return "Test";
   case BlockType::AIR:
      return "Air";
   case BlockType::GRASS:
      return "Grass";
   case BlockType::DIRT:
      return "Dirt";
   case BlockType::STONE:
      return "Stone";
   case BlockType::SAND:
      return "Sand";
   case BlockType::WATER:
      return "Water";
   case BlockType::BEDROCK:
      return "Bedrock";
   case BlockType::WOOD:
      return "Wood";
   case BlockType::LEAVES:
      return "Leaves";
   case BlockType::PLANKS:
      return "Planks";
   case BlockType::COBBLESTONE:
      return "Cobblestone";
   case BlockType::BRICKS:
      return "Bricks";
   default:
      return "Unknown";
   }
}

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
void processMouse(int button, int state, int x, int y);
void mouseFunc(int button, int state, int x, int y);

void updateBlockHighlight();

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
World world;
UIRenderer ui;
bool showChunkBorder = false;
bool showData = true;
bool showTopFace = true;
bool enableFaceOptimization = true;
bool wireframeMode = false;         // Variabile globale per la modalità wireframe
bool enableFastMode = false;        // Variabile globale per la modalità Fats
int lastMouseX = 0, lastMouseY = 0; // Variabili globali per tracciare la posizione precedente del mouse
// Variabili globali per il calcolo degli FPS
std::chrono::steady_clock::time_point lastFrameTime = std::chrono::steady_clock::now();
float fps = 0.0f;

// Aggiungi all'inizio, accanto alle altre variabili globali:
bool cameraMovementEnabled = false;
bool showBlockHighlight = false; // Variabile globale per l'evidenziazione del blocco
Point3D highlightedBlockPos;     // Posizione del blocco evidenziato
Point3D previewBlockPos;

BlockType selectedBlockType = BlockType::GRASS; // Tipo di blocco selezionato

// ================================
// FUNZIONE PRINCIPALE
// ================================
int main(int argc, char **argv)
{
   int seed = 1;                            // Default seed value
   std::string worldName = "default_world"; // Default world name
   bool loadExisting = false;

   // Process command line arguments
   for (int i = 1; i < argc; i++)
   {
      std::string arg = argv[i];
      if (arg == "--setseed" && i + 1 < argc)
      {
         try
         {
            seed = std::stoi(argv[i + 1]);
            i++; // Skip next argument
         }
         catch (const std::exception &)
         {
            std::cerr << "Seed non valido. Utilizzo il seed predefinito (1)." << std::endl;
         }
      }
      else if (arg == "--newworld" && i + 1 < argc)
      {
         worldName = argv[i + 1];
         i++; // Skip next argument
      }
      else if (arg == "--openworld" && i + 1 < argc)
      {
         worldName = argv[i + 1];
         loadExisting = true;
         i++; // Skip next argument
      }
   }

   glutInit(&argc, argv);
   glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
   glutInitWindowSize(400, 300);
   glutCreateWindow("MineGLaft");

   GLenum err = glewInit();
   if (err != GLEW_OK)
   {
      std::cerr << "Inizializzazione di GLEW fallita: " << glewGetErrorString(err) << std::endl;
      return EXIT_FAILURE;
   }

   // Initialize OpenGL settings
   glEnable(GL_DEPTH_TEST);
   glClearColor(0.0f, 1.0f, 1.0f, 1.0f);
   glDisable(GL_LIGHTING);
   glEnable(GL_TEXTURE_2D);
   loadTextures();
   world.camera.reset();

   if (loadExisting)
   {
      // Try to load existing world
      if (!world.loadWorld(worldName))
      {
         std::cerr << "Caricamento del mondo non riuscito." << std::endl;
         return EXIT_FAILURE;
      }
   }
   else
   {
      // Generate new world
      world.generationSeed = seed;
      world.initializeWorld(worldName);
      PerlinNoise noise(world.generationSeed);
      world.generateChunkGrid(12, noise);
   }

   // Rest of initialization
   checkWorldIntegrity();

   // Register callbacks
   glutDisplayFunc(display);
   glutKeyboardFunc(keyboard);
   glutMouseFunc(processMouse);
   glutMotionFunc(mouseMotion);
   glutPassiveMotionFunc(mouseMotion);
   glutMouseFunc(mouseFunc);

   glutMainLoop();
   return 0;
}

// ================================
// IMPLEMENTAZIONE DELLE FUNZIONI
// ================================

/*
void init()
{
   std::cout << "Inizializzazione..." << std::endl;

   glEnable(GL_DEPTH_TEST);
   glClearColor(0.0f, 1.0f, 1.0f, 1.0f);
   glDisable(GL_LIGHTING);
   glEnable(GL_TEXTURE_2D);

   loadTextures();

   camera.reset();

   PerlinNoise noise(world.generationSeed);
   world.generateChunkGrid(32, noise);

   std::cout << "Inizializzazione completata." << std::endl;
}
*/

// Funzione per convertire gradi in radianti
float toRadians(float degrees)
{
   return degrees * M_PI / 180.0f;
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
   gluPerspective(45.0f, 1.0f * glutGet(GLUT_WINDOW_WIDTH) / glutGet(GLUT_WINDOW_HEIGHT), 0.1f, 10000.0f);

   glMatrixMode(GL_MODELVIEW);
   glLoadIdentity();

   float cosPitch = cos(toRadians(world.camera.rot.xRot));
   float sinPitch = sin(toRadians(world.camera.rot.xRot));
   float cosYaw = cos(toRadians(world.camera.rot.yRot));
   float sinYaw = sin(toRadians(world.camera.rot.yRot));

   float lookX = world.camera.pos.x + cosPitch * cosYaw;
   float lookY = world.camera.pos.y + sinPitch;
   float lookZ = world.camera.pos.z + cosPitch * sinYaw;

   gluLookAt(
       world.camera.pos.x, world.camera.pos.y, world.camera.pos.z,
       lookX, lookY, lookZ,
       0.0f, 1.0f, 0.0f);

   // Aggiorna i chunk visibili
   world.updateVisibleChunks(world.camera, RENDER_DISTANCE);

   for (const auto &chunkPair : world.chunksMap)
   {
      const Chunk &chunk = chunkPair.second;
      chunk.drawTextured();

      if (showChunkBorder)
      {
         // Salva lo stato corrente del colore e dei parametri delle linee
         glPushAttrib(GL_CURRENT_BIT | GL_LINE_BIT);

         glColor3f(0.0f, 0.0f, 0.0f); // Imposta il colore del bordo a nero
         glLineWidth(2.0f);
         glBegin(GL_LINES);
         float lineHeight = static_cast<float>(CHUNK_HEIGHT);

         // Calcola le coordinate globali del bordo del chunk
         float globX = chunk.pos.x * CHUNK_SIZE;
         float globZ = chunk.pos.z * CHUNK_SIZE;
         float xMin = globX - 0.5f;
         float xMax = globX + CHUNK_SIZE - 0.5f;
         float zMin = globZ - 0.5f;
         float zMax = globZ + CHUNK_SIZE - 0.5f;

         // Disegna i 4 pilastri verticali agli angoli
         glVertex3f(xMin, 0.0f, zMin);
         glVertex3f(xMin, lineHeight, zMin);

         glVertex3f(xMax, 0.0f, zMin);
         glVertex3f(xMax, lineHeight, zMin);

         glVertex3f(xMax, 0.0f, zMax);
         glVertex3f(xMax, lineHeight, zMax);

         glVertex3f(xMin, 0.0f, zMax);
         glVertex3f(xMin, lineHeight, zMax);
         glEnd();

         // Ripristina lo stato precedente (incluso il colore corrente)
         glPopAttrib();
      }
   }

   // Aggiorna la preview dell'evidenziazione ogni frame
   updateBlockHighlight();

   if (showBlockHighlight)
   {
      glPushAttrib(GL_CURRENT_BIT | GL_LINE_BIT);
      glColor3f(0.0f, 0.0f, 0.0f); // Colore nero per il wireframe
      glLineWidth(6.0f);
      glBegin(GL_LINES);

      // Calcola i limiti del cubo (lato 1)
      float xMin = highlightedBlockPos.x - 0.5f;
      float xMax = highlightedBlockPos.x + 0.5f;
      float yMin = highlightedBlockPos.y - 0.5f;
      float yMax = highlightedBlockPos.y + 0.5f;
      float zMin = highlightedBlockPos.z - 0.5f;
      float zMax = highlightedBlockPos.z + 0.5f;

      // Bordo inferiore
      glVertex3f(xMin, yMin, zMin);
      glVertex3f(xMax, yMin, zMin);
      glVertex3f(xMax, yMin, zMin);
      glVertex3f(xMax, yMin, zMax);
      glVertex3f(xMax, yMin, zMax);
      glVertex3f(xMin, yMin, zMax);
      glVertex3f(xMin, yMin, zMax);
      glVertex3f(xMin, yMin, zMin);

      // Bordo superiore
      glVertex3f(xMin, yMax, zMin);
      glVertex3f(xMax, yMax, zMin);
      glVertex3f(xMax, yMax, zMin);
      glVertex3f(xMax, yMax, zMax);
      glVertex3f(xMax, yMax, zMax);
      glVertex3f(xMin, yMax, zMax);
      glVertex3f(xMin, yMax, zMax);
      glVertex3f(xMin, yMax, zMin);

      // Collega gli spigoli verticali
      glVertex3f(xMin, yMin, zMin);
      glVertex3f(xMin, yMax, zMin);
      glVertex3f(xMax, yMin, zMin);
      glVertex3f(xMax, yMax, zMin);
      glVertex3f(xMax, yMin, zMax);
      glVertex3f(xMax, yMax, zMax);
      glVertex3f(xMin, yMin, zMax);
      glVertex3f(xMin, yMax, zMax);

      glEnd();
      glPopAttrib();
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
      glVertex2f(0, glutGet(GLUT_WINDOW_HEIGHT) - 100);   // Alto sinistro
      glVertex2f(0, glutGet(GLUT_WINDOW_HEIGHT));         // Basso sinistro
      glVertex2f(350, glutGet(GLUT_WINDOW_HEIGHT));       // Basso destro
      glVertex2f(350, glutGet(GLUT_WINDOW_HEIGHT) - 100); // Alto destro
      glEnd();

      // Riabilita lo Z-buffer dopo aver disegnato il rettangolo
      glEnable(GL_DEPTH_TEST);

      // Disabilita il blending dopo aver disegnato il rettangolo
      glDisable(GL_BLEND);

      // Imposta il colore del testo a bianco
      glColor3f(1.0f, 1.0f, 1.0f);

      // Disegna i testi sopra il rettangolo
      ui.drawText("FPS: " + std::to_string(static_cast<int>(fps)), Point2D(10, glutGet(GLUT_WINDOW_HEIGHT) - 15), GLUT_BITMAP_HELVETICA_12);
      ui.drawText("Rotazione Camera: (" + std::to_string(world.camera.rot.xRot) + ", " + std::to_string(world.camera.rot.yRot) + ", " + std::to_string(world.camera.rot.zRot) + ")", Point2D(10, glutGet(GLUT_WINDOW_HEIGHT) - 30), GLUT_BITMAP_HELVETICA_12);
      ui.drawText("Posizione: (" + std::to_string(world.camera.pos.x) + ", " + std::to_string(world.camera.pos.y) + ", " + std::to_string(world.camera.pos.z) + ")", Point2D(10, glutGet(GLUT_WINDOW_HEIGHT) - 45), GLUT_BITMAP_HELVETICA_12);
      Point2D chunkCoords = world.getChunkCoordinates(world.camera.pos);
      ui.drawText("Chunk corrente: (" + std::to_string(chunkCoords.x) + "," + std::to_string(chunkCoords.z) + ")", Point2D(10, glutGet(GLUT_WINDOW_HEIGHT) - 60), GLUT_BITMAP_HELVETICA_12);
      ui.drawText("Seed: " + std::to_string(world.generationSeed), Point2D(10, glutGet(GLUT_WINDOW_HEIGHT) - 75), GLUT_BITMAP_HELVETICA_12);
      ui.drawText("Blocco selezionato: " + blockTypeToString(selectedBlockType) + "(" + std::to_string(static_cast<int>(selectedBlockType)) + ")", Point2D(10, glutGet(GLUT_WINDOW_HEIGHT) - 90), GLUT_BITMAP_HELVETICA_12);

      // Ripristina le impostazioni OpenGL
      glPopMatrix();
      glMatrixMode(GL_PROJECTION);
      glPopMatrix();
      glMatrixMode(GL_MODELVIEW);
   }

   // Draw a small grey crosshair at the center of the screen
   glMatrixMode(GL_PROJECTION);
   glPushMatrix();
   glLoadIdentity();
   glOrtho(0, glutGet(GLUT_WINDOW_WIDTH), 0, glutGet(GLUT_WINDOW_HEIGHT), -1, 1);
   glMatrixMode(GL_MODELVIEW);
   glPushMatrix();
   glLoadIdentity();

   glDisable(GL_TEXTURE_2D);
   glColor3f(0.2f, 0.2f, 0.2f); // Set a light grey color
   glLineWidth(4.0f);
   int centerX = glutGet(GLUT_WINDOW_WIDTH) / 2;
   int centerY = glutGet(GLUT_WINDOW_HEIGHT) / 2;
   int halfSize = 15; // Half size of the cross arms
   glBegin(GL_LINES);
   // Horizontal line
   glVertex2i(centerX - halfSize, centerY);
   glVertex2i(centerX + halfSize, centerY);
   // Vertical line
   glVertex2i(centerX, centerY - halfSize);
   glVertex2i(centerX, centerY + halfSize);
   glEnd();

   glPopMatrix();
   glMatrixMode(GL_PROJECTION);
   glPopMatrix();
   glMatrixMode(GL_MODELVIEW);

   // Ripristina il colore bianco in modo che le texture non siano modulate in grigio
   glColor3f(1.0f, 1.0f, 1.0f);

   // Riabilita il texturing dopo il disegno della UI
   glEnable(GL_TEXTURE_2D);

   glutSwapBuffers();
}

void keyboard(unsigned char key, int, int)
{
   float horizontalStep;
   float verticalStep;

   if (enableFastMode)
   {
      horizontalStep = 16.0f;
      verticalStep = 16.0f;
   }
   else
   {
      horizontalStep = 0.5f;
      verticalStep = 1.0f;
   }
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
   case 'e':
      enableFastMode = !enableFastMode;
      break;
   case 'w':
      world.camera.pos.x += horizontalStep * cos(toRadians(world.camera.rot.yRot));
      world.camera.pos.z += horizontalStep * sin(toRadians(world.camera.rot.yRot));
      moved = true;
      break;
   case 's':
      world.camera.pos.x -= horizontalStep * cos(toRadians(world.camera.rot.yRot));
      world.camera.pos.z -= horizontalStep * sin(toRadians(world.camera.rot.yRot));
      moved = true;
      break;
   case 'd':
      world.camera.pos.x -= horizontalStep * sin(toRadians(world.camera.rot.yRot));
      world.camera.pos.z += horizontalStep * cos(toRadians(world.camera.rot.yRot));
      moved = true;
      break;
   case 'a':
      world.camera.pos.x += horizontalStep * sin(toRadians(world.camera.rot.yRot));
      world.camera.pos.z -= horizontalStep * cos(toRadians(world.camera.rot.yRot));
      moved = true;
      break;
   case ' ':
      world.camera.pos.y += verticalStep;
      moved = true;
      break;
   case 'S':
      world.camera.pos.y -= verticalStep;
      moved = true;
      break;
   case 'r':
      world.camera.reset();
      moved = true;
      break;
   case 'b':
      showChunkBorder = !showChunkBorder;
      break;
   case 'n':
      showData = !showData;
      break;
      /*
         case 'o': // Save world
            world.saveWorld("my_world", camera);
            std::cout << "World saved!" << std::endl;
            break;

         case 'p': // Load world
            if (world.loadWorld("my_world", camera))
            {
               std::cout << "World loaded!" << std::endl;
            }
            else
            {
               std::cout << "Failed to load world!" << std::endl;
            }
            glutPostRedisplay();
            break;
      */
   }

   if (moved)
   {
      world.updateVisibleChunks(world.camera, RENDER_DISTANCE); // Aggiorna i chunk visibili
   }

   glutPostRedisplay();
}

void processMouse(int button, int state, int x, int y)
{
   lastMouseX = x;
   lastMouseY = y;

   if (button == GLUT_LEFT_BUTTON)
   {
      if (state == GLUT_UP)
      {
         if (showBlockHighlight)
         {
            world.placeBlock(previewBlockPos, selectedBlockType);
            showBlockHighlight = false;
         }
      }
   }
   else if (button == GLUT_RIGHT_BUTTON)
   {
      if (state == GLUT_UP)
      {
         if (showBlockHighlight)
         {
            world.placeBlock(highlightedBlockPos, BlockType::AIR);
            showBlockHighlight = false;
         }
      }
   }

   glutPostRedisplay();
}

void mouseMotion(int x, int y)
{
   if (cameraMovementEnabled)
   {
      int deltaX = x - lastMouseX;
      int deltaY = y - lastMouseY;

      world.camera.rot.yRot += static_cast<float>(deltaX) * 0.25f; // Rotazione orizzontale
      world.camera.rot.xRot -= static_cast<float>(deltaY) * 0.25f; // Rotazione verticale

      // Limita la rotazione verticale
      world.camera.rot.xRot = std::max(-89.9f, std::min(89.9f, world.camera.rot.xRot));
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

void mouseFunc(int button, int state, int x, int y)
{
   if (state == GLUT_UP)
   {
      if (button == 3)
      { // Mouse wheel up
         // Blocco successivo
         int currentType = static_cast<int>(selectedBlockType);
         currentType = (currentType + 1) % static_cast<int>(BlockType::BLOCK_COUNT);
         selectedBlockType = static_cast<BlockType>(currentType);
      }
      else if (button == 4)
      { // Mouse wheel down
         // Blocco precedente
         int currentType = static_cast<int>(selectedBlockType);
         currentType = (currentType - 1 + static_cast<int>(BlockType::BLOCK_COUNT)) % static_cast<int>(BlockType::BLOCK_COUNT);
         selectedBlockType = static_cast<BlockType>(currentType);
      }
   }
   // Gestisci gli altri pulsanti del mouse
   processMouse(button, state, x, y);
}

void updateBlockHighlight()
{
   // Calcola la direzione di vista della telecamera
   float cosPitch = cos(toRadians(world.camera.rot.xRot));
   float sinPitch = sin(toRadians(world.camera.rot.xRot));
   float cosYaw = cos(toRadians(world.camera.rot.yRot));
   float sinYaw = sin(toRadians(world.camera.rot.yRot));
   Point3D viewDir(cosPitch * cosYaw, sinPitch, cosPitch * sinYaw);

   const float step = 0.1f;
   const float maxDistance = 5.0f;
   bool foundSurface = false;
   Point3D lastAirPos = world.camera.pos;

   for (float t = step; t <= maxDistance; t += step)
   {
      Point3D currentPos = world.camera.pos + viewDir * t;
      int blockX = static_cast<int>(std::round(currentPos.x));
      int blockY = static_cast<int>(std::round(currentPos.y));
      int blockZ = static_cast<int>(std::round(currentPos.z));

      // Calcola le coordinate del chunk
      int chunkX = static_cast<int>(std::floor(blockX / static_cast<float>(CHUNK_SIZE)));
      int chunkZ = static_cast<int>(std::floor(blockZ / static_cast<float>(CHUNK_SIZE)));
      Point2D chunkCoords(chunkX, chunkZ);
      auto it = world.chunksMap.find(chunkCoords);
      if (it == world.chunksMap.end())
         continue;

      // Calcola le coordinate locali nel chunk
      int localX = blockX - chunkX * CHUNK_SIZE;
      int localZ = blockZ - chunkZ * CHUNK_SIZE;
      int localY = blockY;

      if (localX < 0 || localX >= CHUNK_SIZE ||
          localZ < 0 || localZ >= CHUNK_SIZE ||
          localY < 0 || localY >= CHUNK_HEIGHT)
      {
         continue;
      }

      // Se troviamo un blocco non vuoto, salviamo la posizione per l'evidenziazione
      if (it->second.blocks[localX][localZ][localY].type != BlockType::AIR)
      {
         foundSurface = true;
         highlightedBlockPos = Point3D(blockX, blockY, blockZ);
         previewBlockPos = lastAirPos;
         break;
      }
      else
      {
         lastAirPos = currentPos;
      }
   }
   showBlockHighlight = foundSurface;
}
