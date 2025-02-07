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
#include <algorithm> // Per std::max e std::min

// ================================
// COSTANTI GLOBALI
// ================================
const float FIXED_ASPECT_RATIO = 16.0f / 9.0f;
const int CHUNK_SIZE = 16;      // Dimensioni orizzontali di un chunk
const int CHUNK_HEIGHT = 100;   // Altezza massima di un chunk
const int RENDER_DISTANCE = 10;  // Distanza di rendering

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
        std::shuffle(permutation.begin(), permutation.begin()+256, generator);
        // Duplica la tabella per evitare controlli di wrapping
        for(int i=0; i<256; i++)
        {
            permutation[256+i] = permutation[i];
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
    bool operator==(const Point3D& other) const
    {
        return (x == other.x && y == other.y && z == other.z);
    }

    // Operatore != per confrontare due Point3D
    bool operator!=(const Point3D& other) const
    {
        return !(*this == other);
    }

    // Operatore + per sommare due Point3D
    Point3D operator+(const Point3D& other) const
    {
        return Point3D(x + other.x, y + other.y, z + other.z);
    }

    // Operatore * per moltiplicare un Point3D per uno scalare
    Point3D operator*(float scalar) const
    {
        return Point3D(x * scalar, y * scalar, z * scalar);
    }

    // Operatore * per moltiplicare uno scalare per un Point3D
    friend Point3D operator*(float scalar, const Point3D& point)
    {
        return Point3D(point.x * scalar, point.y * scalar, point.z * scalar);
    }
};

class Point2D
{
public:
    float x, z;
    Point2D(float xPos = 0.0f, float zPos = 0.0f) : x(xPos), z(zPos) {}

    bool operator==(const Point2D& other) const
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
    Color(float red = 1.0f, float green = 1.0f, float blue = 1.0f, float alpha = 1.0f) :
        r(red), g(green), b(blue), a(alpha) {}

    void apply() const
    {
        glColor4f(r, g, b, a); // Use glColor4f instead of glColor3f to include alpha
    }
};

// Classe Camera
class Camera {
public:
    Point3D pos;
    Rotation rot;

    Camera(Point3D initialPos = Point3D(0.0f, 0.0f, 0.0f), Rotation initialRot = Rotation(0.0f, 0.0f, 0.0f))
        : pos(initialPos), rot(initialRot) {}

    void reset();
    bool isFaceVisible(const Point3D& blockPos, const Point3D& faceNormalVect) const;
};

// Dichiarazione anticipata della classe Chunk
class Chunk;

// Enumerazione globale per i tipi di blocco
enum class BlockType
{
    _VOID,
    TEST,
    AIR,
    DIRT,
    STONE,
    SAND,
    WATER,
    BEDROCK
};

// Enum per identificare le facce del cubo
enum class FaceType
{
    FRONT,   // Fronte
    BACK,    // Dietro
    LEFT,    // Sinistra
    RIGHT,   // Destra
    TOP,     // Sopra
    BOTTOM   // Sotto
};

struct Face
{
    bool isVisible;
    Color color;

    Face(bool visible = true, const Color& faceColor = Color(1.0f, 1.0f, 1.0f)) : isVisible(visible), color(faceColor) {}
};

// Classe Blocco
// Classe Blocco
class Block
{
public:
    Point3D pos;
    float scale;
    Rotation rot;
    BlockType type;
    Face faces[6];
    bool isTransparent; // Nuovo campo per indicare se il blocco � trasparente

    Block(Point3D position = Point3D(0.0f, 0.0f, 0.0f), float scaleFactor = 0.0f, Rotation rotation = Rotation(0.0f, 0.0f, 0.0f), BlockType blockType = BlockType::_VOID)
        : pos(position), scale(scaleFactor), rot(rotation), type(blockType), isTransparent(false) // Inizializza isTransparent a false di default
    {
        switch (type)
        {
        case BlockType::STONE:
        {
            faces[static_cast<int>(FaceType::FRONT)]  = Face(true, Color(0.5f, 0.5f, 0.5f)); // Grigio
            faces[static_cast<int>(FaceType::BACK)]   = Face(true, Color(0.5f, 0.5f, 0.5f)); // Grigio
            faces[static_cast<int>(FaceType::LEFT)]   = Face(true, Color(0.5f, 0.5f, 0.5f)); // Grigio
            faces[static_cast<int>(FaceType::RIGHT)]  = Face(true, Color(0.5f, 0.5f, 0.5f)); // Grigio
            faces[static_cast<int>(FaceType::TOP)]    = Face(true, Color(0.5f, 0.5f, 0.5f)); // Grigio
            faces[static_cast<int>(FaceType::BOTTOM)] = Face(true, Color(0.5f, 0.5f, 0.5f)); // Grigio
            isTransparent = false; // Pietra non � trasparente
            break;
        }
        case BlockType::DIRT:
        {
            faces[static_cast<int>(FaceType::FRONT)]  = Face(true, Color(0.5f, 0.25f, 0.0f)); // Marrone
            faces[static_cast<int>(FaceType::BACK)]   = Face(true, Color(0.5f, 0.25f, 0.0f)); // Marrone
            faces[static_cast<int>(FaceType::LEFT)]   = Face(true, Color(0.5f, 0.25f, 0.0f)); // Marrone
            faces[static_cast<int>(FaceType::RIGHT)]  = Face(true, Color(0.5f, 0.25f, 0.0f)); // Marrone
            faces[static_cast<int>(FaceType::TOP)]    = Face(true, Color(0.0f, 0.50f, 0.0f)); // Verde
            faces[static_cast<int>(FaceType::BOTTOM)] = Face(true, Color(0.5f, 0.25f, 0.0f)); // Marrone
            isTransparent = false; // Terra non � trasparente
            break;
        }
        case BlockType::TEST:
        {
            faces[static_cast<int>(FaceType::FRONT)]  = Face(true, Color(1.0f, 0.0f, 1.0f)); // Magenta
            faces[static_cast<int>(FaceType::BACK)]   = Face(true, Color(0.0f, 1.0f, 0.0f)); // Verde
            faces[static_cast<int>(FaceType::LEFT)]   = Face(true, Color(1.0f, 0.0f, 0.0f)); // Rosso
            faces[static_cast<int>(FaceType::RIGHT)]  = Face(true, Color(0.0f, 1.0f, 1.0f)); // Ciano
            faces[static_cast<int>(FaceType::TOP)]    = Face(true, Color(0.0f, 0.0f, 1.0f)); // Blu
            faces[static_cast<int>(FaceType::BOTTOM)] = Face(true, Color(1.0f, 1.0f, 0.0f)); // Giallo
            isTransparent = false; // Blocco di test non � trasparente
            break;
        }
        case BlockType::BEDROCK:
        {
            faces[static_cast<int>(FaceType::FRONT)]  = Face(true, Color(0.2f, 0.2f, 0.2f)); // Grigio scuro
            faces[static_cast<int>(FaceType::BACK)]   = Face(true, Color(0.2f, 0.2f, 0.2f)); // Grigio scuro
            faces[static_cast<int>(FaceType::LEFT)]   = Face(true, Color(0.2f, 0.2f, 0.2f)); // Grigio scuro
            faces[static_cast<int>(FaceType::RIGHT)]  = Face(true, Color(0.2f, 0.2f, 0.2f)); // Grigio scuro
            faces[static_cast<int>(FaceType::TOP)]    = Face(true, Color(0.2f, 0.2f, 0.2f)); // Grigio scuro
            faces[static_cast<int>(FaceType::BOTTOM)] = Face(true, Color(0.2f, 0.2f, 0.2f)); // Grigio scuro
            isTransparent = false; // Bedrock non � trasparente
            break;
        }
        case BlockType::AIR:
        {
            faces[static_cast<int>(FaceType::FRONT)]  = Face(false, Color(1.0f, 1.0f, 1.0f)); // Non visibile
            faces[static_cast<int>(FaceType::BACK)]   = Face(false, Color(1.0f, 1.0f, 1.0f)); // Non visibile
            faces[static_cast<int>(FaceType::LEFT)]   = Face(false, Color(1.0f, 1.0f, 1.0f)); // Non visibile
            faces[static_cast<int>(FaceType::RIGHT)]  = Face(false, Color(1.0f, 1.0f, 1.0f)); // Non visibile
            faces[static_cast<int>(FaceType::TOP)]    = Face(false, Color(1.0f, 1.0f, 1.0f)); // Non visibile
            faces[static_cast<int>(FaceType::BOTTOM)] = Face(false, Color(1.0f, 1.0f, 1.0f)); // Non visibile
            isTransparent = true; // L'aria � trasparente
            break;
        }
        case BlockType::SAND:
        {
            faces[static_cast<int>(FaceType::FRONT)]  = Face(true, Color(0.9f, 0.8f, 0.5f)); // Giallo chiaro
            faces[static_cast<int>(FaceType::BACK)]   = Face(true, Color(0.9f, 0.8f, 0.5f));
            faces[static_cast<int>(FaceType::LEFT)]   = Face(true, Color(0.9f, 0.8f, 0.5f));
            faces[static_cast<int>(FaceType::RIGHT)]  = Face(true, Color(0.9f, 0.8f, 0.5f));
            faces[static_cast<int>(FaceType::TOP)]    = Face(true, Color(1.0f, 0.9f, 0.6f)); // Sopra pi� chiaro
            faces[static_cast<int>(FaceType::BOTTOM)] = Face(true, Color(0.9f, 0.8f, 0.5f));
            isTransparent = false; // La sabbia non � trasparente
            break;
        }
        case BlockType::WATER:
        {
            faces[static_cast<int>(FaceType::FRONT)]  = Face(true, Color(0.0f, 0.3f, 0.8f, 0.5f)); // Blu semitrasparente
            faces[static_cast<int>(FaceType::BACK)]   = Face(true, Color(0.0f, 0.3f, 0.8f, 0.5f));
            faces[static_cast<int>(FaceType::LEFT)]   = Face(true, Color(0.0f, 0.3f, 0.8f, 0.5f));
            faces[static_cast<int>(FaceType::RIGHT)]  = Face(true, Color(0.0f, 0.3f, 0.8f, 0.5f));
            faces[static_cast<int>(FaceType::TOP)]    = Face(true, Color(0.0f, 0.3f, 0.8f, 0.5f));
            faces[static_cast<int>(FaceType::BOTTOM)] = Face(true, Color(0.0f, 0.3f, 0.8f, 0.5f));
            isTransparent = false; // L'acqua � trasparente
            break;
        }
        default:
        {
            faces[static_cast<int>(FaceType::FRONT)]  = Face(false, Color(1.0f, 1.0f, 1.0f)); // Non visibile
            faces[static_cast<int>(FaceType::BACK)]   = Face(false, Color(1.0f, 1.0f, 1.0f)); // Non visibile
            faces[static_cast<int>(FaceType::LEFT)]   = Face(false, Color(1.0f, 1.0f, 1.0f)); // Non visibile
            faces[static_cast<int>(FaceType::RIGHT)]  = Face(false, Color(1.0f, 1.0f, 1.0f)); // Non visibile
            faces[static_cast<int>(FaceType::TOP)]    = Face(false, Color(1.0f, 1.0f, 1.0f)); // Non visibile
            faces[static_cast<int>(FaceType::BOTTOM)] = Face(false, Color(1.0f, 1.0f, 1.0f)); // Non visibile
            isTransparent = true; // Tipo di blocco sconosciuto considerato trasparente
            break;
        }
        }
    }

    void draw(const Chunk& chunk) const;
    void setFaceVisibility(FaceType face, bool isVisible);
};

// Classe Chunk
class Chunk
{
public:
    Point2D pos;
    std::vector<std::vector<std::vector<Block>>> blocks;

    // Costruttore predefinito
    Chunk();

    // Costruttore con posizione
    Chunk(Point2D position, int seed);

    void draw() const;
    bool isBlock(Point3D pos) const;
    void addBlock(const Point3D& blockPos, BlockType type);
    void updateAdjacentBlocks(const Point3D& blockPos);
    bool hasVisibleFace(const Block& block) const;
};

namespace std
{
template <>
struct hash<Point2D>
{
    size_t operator()(const Point2D& p) const
    {
        // Combinazione hash delle coordinate x e z
        return hash<float>()(p.x) ^ (hash<float>()(p.z) << 1);
    }
};
}

class World {
public:
    std::unordered_map<Point2D, Chunk> chunksMap;
    int generationSeed;
    Point3D spawnPoint;

    void addChunk(Point2D pos);
    Point2D getChunkCoordinates(const Point3D& pos) const;
    void placeBlock(const Point3D& blockPos, BlockType type);
    void updateVisibleChunks(const Camera& camera, int renderDistance);
    Chunk* getChunk(Point2D pos);
};

class UIRenderer {
public:
    static void drawText(const std::string& text, Point2D pos, void* font = GLUT_BITMAP_HELVETICA_10);
};

// ================================
// PROTOTIPI DELLE FUNZIONI
// ================================
void init();
float toRadians(float degrees);
bool isFaceVisible(const Point3D& blockPos, const Point3D& faceNormalVect);

void display();

void keyboard(unsigned char key, int x, int y);
void processMouse(int button, int state, int x, int y);
void motionMouse(int x, int y);
void passiveMouse(int x, int y);
void resetMousePosition();

void checkWorldIntegrity();

// ================================
// VARIABILI GLOBALI
// ================================
Camera camera;
World world; // Dichiarazione della variabile globale
bool showChunkBorder = false;
bool showData = true;
bool showTopFace = true;
bool enableFaceOptimization = true;
int lastMouseX = 0, lastMouseY = 0; // Variabili globali per tracciare la posizione precedente del mouse
// Variabili globali per il calcolo degli FPS
std::chrono::steady_clock::time_point lastFrameTime = std::chrono::steady_clock::now();
float fps = 0.0f;

// ================================
// FUNZIONE PRINCIPALE
// ================================
int main(int argc, char** argv)
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
    glutMouseFunc(processMouse);       // Gestione clic del mouse
    glutMotionFunc(motionMouse);       // Gestione movimento del mouse con pulsante premuto
    glutPassiveMotionFunc(passiveMouse); // Gestione movimento del mouse senza pulsante premuto

    // Avvia il ciclo principale di GLUT
    glutMainLoop();
    return 0;
}

// ================================
// IMPLEMENTAZIONE DELLE FUNZIONI
// ================================

void init()
{
    glEnable(GL_DEPTH_TEST); // Abilita il test di profondit�
    glClearColor(0.0f, 1.0f, 1.0f, 1.0f); // Colore di sfondo grigio scuro

    // Abilita l'illuminazione e la prima sorgente di luce
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);

    // Configura la sorgente di luce
    float lightPosition[] = { 50.0f, 50.0f, 50.0f, 1.0f }; // Posizione della luce
    float ambientLight[] = { 0.2f, 0.2f, 0.2f, 1.0f };     // Luce ambiente
    float diffuseLight[] = { 0.8f, 0.8f, 0.8f, 1.0f };     // Luce diffusa
    float specularLight[] = { 0.5f, 0.5f, 0.5f, 1.0f };    // Luce speculare

    glLightfv(GL_LIGHT0, GL_POSITION, lightPosition);
    glLightfv(GL_LIGHT0, GL_AMBIENT, ambientLight);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuseLight);
    glLightfv(GL_LIGHT0, GL_SPECULAR, specularLight);

    camera.reset();
    world.generationSeed = 1234; // Seme per la generazione del mondo

    int dim = RENDER_DISTANCE + 2;
    for (int i = -dim; i <= dim; i++)
    {
        for (int j = -dim; j <= dim; j++)
        {
            world.addChunk(Point2D(i, j));
        }
    }
}

void checkWorldIntegrity()
{
    std::cout << "controllo validit� del mondo..." << std::endl;

    for (const auto& [chunkPos, chunk] : world.chunksMap)
    {
        for (int x = 0; x < CHUNK_SIZE; ++x)
        {
            for (int z = 0; z < CHUNK_SIZE; ++z)
            {
                for (int y = 0; y < CHUNK_HEIGHT; ++y)
                {
                    const Block& block = chunk.blocks[x][z][y];
                    if (block.type == BlockType::_VOID)
                    {
                        std::cerr << "Blocco vuoto trovato in: " << chunk.pos.x + x << ", " << y << ", " << chunk.pos.z + z << std::endl;
                    }
                }
            }
        }
    }

    std::cout << "controllo validit� del mondo effettuata" << std::endl;
}

void Camera::reset() {
    pos = Point3D(-4 * CHUNK_SIZE, 10 * CHUNK_SIZE, -4 * CHUNK_SIZE);
    rot = Rotation(-30.0f, 45.0f, 0.0f);
    glutPostRedisplay();
}

bool Camera::isFaceVisible(const Point3D& blockPos, const Point3D& faceNormalVect) const {
    Point3D directionVect(blockPos.x - pos.x, blockPos.y - pos.y, blockPos.z - pos.z);
    directionVect.normalize();
    float dotProduct = directionVect.x * faceNormalVect.x + directionVect.y * faceNormalVect.y + directionVect.z * faceNormalVect.z;
    return dotProduct < 0.0f;
}

void Block::draw(const Chunk& chunk) const
{
    if (type == BlockType::_VOID || type == BlockType::AIR)
        return; // Non disegna i blocchi vuoti o d'aria

    glPushMatrix();
    glTranslatef(pos.x, pos.y, pos.z);
    glScalef(scale, scale, scale);

    if (isTransparent && type == BlockType::WATER)
    {
        glScalef(1.0f, 1.0f, 1.0f); // Acqua sottostante: cubo completo
    }
    else
    {
        glScalef(scale, scale, scale); // Scala uniforme per gli altri blocchi
    }

    if (isTransparent)
    {
        glEnable(GL_BLEND); // Abilita il blending per la trasparenza
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    glBegin(GL_QUADS);

    // Fronte
    if (faces[static_cast<int>(FaceType::FRONT)].isVisible && (camera.isFaceVisible(pos, Point3D(0.0f, 0.0f, 1.0f)) || !enableFaceOptimization))
    {
        const Color& faceColor = faces[static_cast<int>(FaceType::FRONT)].color;
        float emission[] = { faceColor.r * 0.8f, faceColor.g * 0.8f, faceColor.b * 0.8f, 1.0f }; // Usa il colore della faccia con un fattore di emissione ridotto
        float ambient[] = { faceColor.r * 0.2f, faceColor.g * 0.2f, faceColor.b * 0.2f, 1.0f }; // Piccola componente ambientale
        float diffuse[] = { faceColor.r * 0.5f, faceColor.g * 0.5f, faceColor.b * 0.5f, 1.0f }; // Piccola componente diffusa
        glMaterialfv(GL_FRONT, GL_EMISSION, emission); // Imposta emissione
        glMaterialfv(GL_FRONT, GL_AMBIENT, ambient);   // Imposta ambiente
        glMaterialfv(GL_FRONT, GL_DIFFUSE, diffuse);   // Imposta diffuso
        glNormal3f(0.0f, 0.0f, 1.0f); // Normale per la faccia frontale
        glVertex3f(-0.5f, -0.5f, 0.5f);
        glVertex3f(0.5f, -0.5f, 0.5f);
        glVertex3f(0.5f, 0.5f, 0.5f);
        glVertex3f(-0.5f, 0.5f, 0.5f);
    }

    // Dietro
    if (faces[static_cast<int>(FaceType::BACK)].isVisible && (camera.isFaceVisible(pos, Point3D(0.0f, 0.0f, -1.0f)) || !enableFaceOptimization))
    {
        const Color& faceColor = faces[static_cast<int>(FaceType::BACK)].color;
        float emission[] = { faceColor.r * 0.8f, faceColor.g * 0.8f, faceColor.b * 0.8f, 1.0f };
        float ambient[] = { faceColor.r * 0.2f, faceColor.g * 0.2f, faceColor.b * 0.2f, 1.0f };
        float diffuse[] = { faceColor.r * 0.5f, faceColor.g * 0.5f, faceColor.b * 0.5f, 1.0f };
        glMaterialfv(GL_FRONT, GL_EMISSION, emission);
        glMaterialfv(GL_FRONT, GL_AMBIENT, ambient);
        glMaterialfv(GL_FRONT, GL_DIFFUSE, diffuse);
        glNormal3f(0.0f, 0.0f, -1.0f); // Normale per la faccia posteriore
        glVertex3f(-0.5f, -0.5f, -0.5f);
        glVertex3f(0.5f, -0.5f, -0.5f);
        glVertex3f(0.5f, 0.5f, -0.5f);
        glVertex3f(-0.5f, 0.5f, -0.5f);
    }

    // Sinistra
    if (faces[static_cast<int>(FaceType::LEFT)].isVisible && (camera.isFaceVisible(pos, Point3D(-1.0f, 0.0f, 0.0f)) || !enableFaceOptimization))
    {
        const Color& faceColor = faces[static_cast<int>(FaceType::LEFT)].color;
        float emission[] = { faceColor.r * 0.8f, faceColor.g * 0.8f, faceColor.b * 0.8f, 1.0f };
        float ambient[] = { faceColor.r * 0.2f, faceColor.g * 0.2f, faceColor.b * 0.2f, 1.0f };
        float diffuse[] = { faceColor.r * 0.5f, faceColor.g * 0.5f, faceColor.b * 0.5f, 1.0f };
        glMaterialfv(GL_FRONT, GL_EMISSION, emission);
        glMaterialfv(GL_FRONT, GL_AMBIENT, ambient);
        glMaterialfv(GL_FRONT, GL_DIFFUSE, diffuse);
        glNormal3f(-1.0f, 0.0f, 0.0f); // Normale per la faccia sinistra
        glVertex3f(-0.5f, -0.5f, -0.5f);
        glVertex3f(-0.5f, -0.5f, 0.5f);
        glVertex3f(-0.5f, 0.5f, 0.5f);
        glVertex3f(-0.5f, 0.5f, -0.5f);
    }

    // Destra
    if (faces[static_cast<int>(FaceType::RIGHT)].isVisible && (camera.isFaceVisible(pos, Point3D(1.0f, 0.0f, 0.0f)) || !enableFaceOptimization))
    {
        const Color& faceColor = faces[static_cast<int>(FaceType::RIGHT)].color;
        float emission[] = { faceColor.r * 0.8f, faceColor.g * 0.8f, faceColor.b * 0.8f, 1.0f };
        float ambient[] = { faceColor.r * 0.2f, faceColor.g * 0.2f, faceColor.b * 0.2f, 1.0f };
        float diffuse[] = { faceColor.r * 0.5f, faceColor.g * 0.5f, faceColor.b * 0.5f, 1.0f };
        glMaterialfv(GL_FRONT, GL_EMISSION, emission);
        glMaterialfv(GL_FRONT, GL_AMBIENT, ambient);
        glMaterialfv(GL_FRONT, GL_DIFFUSE, diffuse);
        glNormal3f(1.0f, 0.0f, 0.0f); // Normale per la faccia destra
        glVertex3f(0.5f, -0.5f, -0.5f);
        glVertex3f(0.5f, -0.5f, 0.5f);
        glVertex3f(0.5f, 0.5f, 0.5f);
        glVertex3f(0.5f, 0.5f, -0.5f);
    }

    // Sopra
    if (faces[static_cast<int>(FaceType::TOP)].isVisible && (camera.isFaceVisible(pos, Point3D(0.0f, 1.0f, 0.0f)) || !enableFaceOptimization) && showTopFace)
    {
        const Color& faceColor = faces[static_cast<int>(FaceType::TOP)].color;
        float emission[] = { faceColor.r * 0.8f, faceColor.g * 0.8f, faceColor.b * 0.8f, 1.0f };
        float ambient[] = { faceColor.r * 0.2f, faceColor.g * 0.2f, faceColor.b * 0.2f, 1.0f };
        float diffuse[] = { faceColor.r * 0.5f, faceColor.g * 0.5f, faceColor.b * 0.5f, 1.0f };
        glMaterialfv(GL_FRONT, GL_EMISSION, emission);
        glMaterialfv(GL_FRONT, GL_AMBIENT, ambient);
        glMaterialfv(GL_FRONT, GL_DIFFUSE, diffuse);
        glNormal3f(0.0f, 1.0f, 0.0f); // Normale per la faccia superiore
        glVertex3f(-0.5f, 0.5f, -0.5f);
        glVertex3f(0.5f, 0.5f, -0.5f);
        glVertex3f(0.5f, 0.5f, 0.5f);
        glVertex3f(-0.5f, 0.5f, 0.5f);
    }

    // Sotto
    if (faces[static_cast<int>(FaceType::BOTTOM)].isVisible && (camera.isFaceVisible(pos, Point3D(0.0f, -1.0f, 0.0f)) || !enableFaceOptimization))
    {
        const Color& faceColor = faces[static_cast<int>(FaceType::BOTTOM)].color;
        float emission[] = { faceColor.r * 0.8f, faceColor.g * 0.8f, faceColor.b * 0.8f, 1.0f };
        float ambient[] = { faceColor.r * 0.2f, faceColor.g * 0.2f, faceColor.b * 0.2f, 1.0f };
        float diffuse[] = { faceColor.r * 0.5f, faceColor.g * 0.5f, faceColor.b * 0.5f, 1.0f };
        glMaterialfv(GL_FRONT, GL_EMISSION, emission);
        glMaterialfv(GL_FRONT, GL_AMBIENT, ambient);
        glMaterialfv(GL_FRONT, GL_DIFFUSE, diffuse);
        glNormal3f(0.0f, -1.0f, 0.0f); // Normale per la faccia inferiore
        glVertex3f(-0.5f, -0.5f, -0.5f);
        glVertex3f(0.5f, -0.5f, -0.5f);
        glVertex3f(0.5f, -0.5f, 0.5f);
        glVertex3f(-0.5f, -0.5f, 0.5f);
    }

    glEnd();
    glPopMatrix();
}

void Block::setFaceVisibility(FaceType face, bool isVisible)
{
    faces[static_cast<int>(face)].isVisible = isVisible;
}

void World::addChunk(Point2D pos)
{
    world.chunksMap[pos] = Chunk(pos, world.generationSeed);
    //std::cout << "Chunk(" << pos.x << ", " << pos.z << ") aggiunto." << std::endl;
}
// Costruttore predefinito
Chunk::Chunk() : pos(Point2D(0, 0)) {}

// Costruttore del Chunk con generazione basata su Perlin Noise
Chunk::Chunk(Point2D position, int seed) : pos(Point2D(position.x * CHUNK_SIZE, position.z * CHUNK_SIZE))
{
    PerlinNoise noise(seed); // Crea un'istanza di PerlinNoise con il seme specificato
    blocks.resize(CHUNK_SIZE, std::vector<std::vector<Block>>(CHUNK_SIZE, std::vector<Block>(CHUNK_HEIGHT)));

    float globalWaterLevel = CHUNK_HEIGHT / 3 +1; // Livello globale dell'acqua ("livello del mare")

    for (int x = 0; x < CHUNK_SIZE; ++x)
    {
        for (int z = 0; z < CHUNK_SIZE; ++z)
        {
            for (int y = 0; y < CHUNK_HEIGHT; ++y)
            {
                float posX = pos.x + x;
                float posY = static_cast<float>(y);
                float posZ = pos.z + z;

                float scaleNoiseFactor = 0.02f;
                // Calcola il valore del Perlin Noise
                float noiseValue = noise.getNoise((posX) * scaleNoiseFactor, posY * scaleNoiseFactor * 5, (posZ) * scaleNoiseFactor);

                BlockType blockType;

                if (posY > CHUNK_HEIGHT / 3 + noiseValue * 10.0f + 2)
                {
                    blockType = BlockType::AIR; // Aria sopra il terreno
                }
                else if (posY < 2)
                {
                    blockType = BlockType::BEDROCK; // Bedrock sotto il terreno
                }
                else if (posY > CHUNK_HEIGHT / 3 + noiseValue * 10.0f - 6) // spessore del terreno in superficie
                {
                    blockType = BlockType::DIRT; // Terra in superficie
                }
                else
                {
                    blockType = BlockType::STONE; // Pietra nel mezzo
                }

                blocks[x][z][y] = Block(Point3D(posX, posY, posZ), 1.0f, Rotation(0.0f, 0.0f, 0.0f), blockType);
            }
        }
    }

    // Generazione dei laghi in depressioni del terreno
    for (int x = 0; x < CHUNK_SIZE; ++x)
    {
        for (int z = 0; z < CHUNK_SIZE; ++z)
        {
            for (int y = 0; y < CHUNK_HEIGHT; ++y)
            {
                float posX = pos.x + x;
                float posY = static_cast<float>(y);
                float posZ = pos.z + z;

                if (posY < globalWaterLevel && blocks[x][z][y].type == BlockType::AIR)
                {
                    blocks[x][z][y] = Block(Point3D(posX, posY, posZ), 1.0f, Rotation(0.0f, 0.0f, 0.0f), BlockType::WATER);
                }
            }
        }
    }

    // Aggiungi sabbia intorno e sotto i blocchi d'acqua
    for (int x = 0; x < CHUNK_SIZE; ++x)
    {
        for (int z = 0; z < CHUNK_SIZE; ++z)
        {
            for (int y = 0; y < CHUNK_HEIGHT; ++y)
            {
                if (blocks[x][z][y].type == BlockType::WATER)
                {
                    // Controlla i blocchi sotto l'acqua
                    if (y > 0 && blocks[x][z][y - 1].type != BlockType::WATER)
                    {
                        blocks[x][z][y - 1] = Block(Point3D(pos.x + x, y - 1, pos.z + z), 1.0f, Rotation(0.0f, 0.0f, 0.0f), BlockType::SAND);
                    }

                    // Controlla i blocchi attorno all'acqua (sinistra, destra, davanti, dietro)
                    if (x > 0 && blocks[x - 1][z][y].type != BlockType::WATER)
                    {
                        blocks[x - 1][z][y] = Block(Point3D(pos.x + x - 1, y, pos.z + z), 1.0f, Rotation(0.0f, 0.0f, 0.0f), BlockType::SAND);
                    }
                    if (x < CHUNK_SIZE - 1 && blocks[x + 1][z][y].type != BlockType::WATER)
                    {
                        blocks[x + 1][z][y] = Block(Point3D(pos.x + x + 1, y, pos.z + z), 1.0f, Rotation(0.0f, 0.0f, 0.0f), BlockType::SAND);
                    }
                    if (z > 0 && blocks[x][z - 1][y].type != BlockType::WATER)
                    {
                        blocks[x][z - 1][y] = Block(Point3D(pos.x + x, y, pos.z + z - 1), 1.0f, Rotation(0.0f, 0.0f, 0.0f), BlockType::SAND);
                    }
                    if (z < CHUNK_SIZE - 1 && blocks[x][z + 1][y].type != BlockType::WATER)
                    {
                        blocks[x][z + 1][y] = Block(Point3D(pos.x + x, y, pos.z + z + 1), 1.0f, Rotation(0.0f, 0.0f, 0.0f), BlockType::SAND);
                    }
                }
            }
        }
    }

    // Aggiorna la visibilit� delle facce
    for (int x = 0; x < CHUNK_SIZE; ++x)
    {
        for (int z = 0; z < CHUNK_SIZE; ++z)
        {
            for (int y = 0; y < CHUNK_HEIGHT; ++y)
            {
                Block& currentBlock = blocks[x][z][y];
                if (currentBlock.isTransparent) continue;

                currentBlock.setFaceVisibility(FaceType::FRONT, !isBlock(Point3D(pos.x + x, y, pos.z + z + 1)));
                currentBlock.setFaceVisibility(FaceType::BACK, !isBlock(Point3D(pos.x + x, y, pos.z + z - 1)));
                currentBlock.setFaceVisibility(FaceType::LEFT, !isBlock(Point3D(pos.x + x - 1, y, pos.z + z)));
                currentBlock.setFaceVisibility(FaceType::RIGHT, !isBlock(Point3D(pos.x + x + 1, y, pos.z + z)));
                currentBlock.setFaceVisibility(FaceType::TOP, !isBlock(Point3D(pos.x + x, y + 1, pos.z + z)));
                currentBlock.setFaceVisibility(FaceType::BOTTOM, !isBlock(Point3D(pos.x + x, y - 1, pos.z + z)));

                updateAdjacentBlocks(Point3D(pos.x + x, y, pos.z + z));
            }
        }
    }
}

// Metodo per disegnare il chunk
void Chunk::draw() const
{
    // Disegna prima i blocchi opachi
    for (int x = 0; x < CHUNK_SIZE; ++x)
    {
        for (int z = 0; z < CHUNK_SIZE; ++z)
        {
            for (int y = 0; y < CHUNK_HEIGHT; ++y)
            {
                const Block& block = blocks[x][z][y];
                if (!block.isTransparent && hasVisibleFace(block))
                {
                    block.draw(*this);
                }
            }
        }
    }
}

bool Chunk::hasVisibleFace(const Block& block) const
{
    for (int i = 0; i < 6; ++i)
    {
        if (block.faces[i].isVisible)
        {
            return true;
        }
    }
    return false;
}

void Chunk::addBlock(const Point3D& blockPos, BlockType type)
{
    // Verifica se la posizione del blocco � valida nel chunk
    int localX = static_cast<int>(blockPos.x - pos.x);
    int localZ = static_cast<int>(blockPos.z - pos.z);
    int localY = static_cast<int>(blockPos.y);

    if (localX < 0 || localX >= CHUNK_SIZE ||
            localZ < 0 || localZ >= CHUNK_SIZE ||
            localY < 0 || localY >= CHUNK_HEIGHT)
    {
        return; // Posizione fuori dai limiti del chunk
    }

    // Crea il nuovo blocco
    Block newBlock(blockPos, 1.0f, Rotation(0.0f, 0.0f, 0.0f), type);

    // Imposta la visibilit� delle facce del nuovo blocco
    if (isBlock(Point3D(blockPos.x, blockPos.y, blockPos.z + 1))) newBlock.setFaceVisibility(FaceType::FRONT, false);
    if (isBlock(Point3D(blockPos.x, blockPos.y, blockPos.z - 1))) newBlock.setFaceVisibility(FaceType::BACK, false);
    if (isBlock(Point3D(blockPos.x - 1, blockPos.y, blockPos.z))) newBlock.setFaceVisibility(FaceType::LEFT, false);
    if (isBlock(Point3D(blockPos.x + 1, blockPos.y, blockPos.z))) newBlock.setFaceVisibility(FaceType::RIGHT, false);
    if (isBlock(Point3D(blockPos.x, blockPos.y + 1, blockPos.z))) newBlock.setFaceVisibility(FaceType::TOP, false);
    if (isBlock(Point3D(blockPos.x, blockPos.y - 1, blockPos.z))) newBlock.setFaceVisibility(FaceType::BOTTOM, false);

    // Aggiungi il blocco al chunk
    blocks[localX][localZ][localY] = newBlock;

    // Aggiorna la visibilit� delle facce dei blocchi adiacenti
    updateAdjacentBlocks(blockPos);
}

void Chunk::updateAdjacentBlocks(const Point3D& blockPos)
{
    std::vector<Point3D> directions =
    {
        Point3D(0, 0, 1),  // Fronte
        Point3D(0, 0, -1), // Dietro
        Point3D(-1, 0, 0), // Sinistra
        Point3D(1, 0, 0),  // Destra
        Point3D(0, 1, 0),  // Sopra
        Point3D(0, -1, 0)  // Sotto
    };

    static const std::array<FaceType, 6> oppositeFaces =
    {
        FaceType::BACK, FaceType::FRONT, FaceType::RIGHT, FaceType::LEFT, FaceType::BOTTOM, FaceType::TOP
    };

    auto updateBlock = [&](Block& block, size_t directionIndex, const Point3D& adjPos)
    {
        if (isBlock(adjPos))
        {
            block.setFaceVisibility(oppositeFaces[directionIndex], false);
        }
        else
        {
            block.setFaceVisibility(oppositeFaces[directionIndex], true);
        }
    };

    for (size_t i = 0; i < directions.size(); ++i)
    {
        Point3D adjacentPos = blockPos + directions[i];
        Point2D adjacentChunkCoords = world.getChunkCoordinates(adjacentPos);

        // Recupera il chunk adiacente dalla mappa
        Chunk* adjacentChunk = world.getChunk(adjacentChunkCoords);
        if (!adjacentChunk)
        {
            continue; // Il chunk adiacente non esiste
        }

        int adjLocalX = static_cast<int>(adjacentPos.x - (adjacentChunkCoords.x * CHUNK_SIZE));
        int adjLocalZ = static_cast<int>(adjacentPos.z - (adjacentChunkCoords.z * CHUNK_SIZE));
        int adjLocalY = static_cast<int>(adjacentPos.y);

        if (adjLocalX >= 0 && adjLocalX < CHUNK_SIZE &&
                adjLocalZ >= 0 && adjLocalZ < CHUNK_SIZE &&
                adjLocalY >= 0 && adjLocalY < CHUNK_HEIGHT)
        {
            Block& currentBlock = blocks[static_cast<int>(blockPos.x - pos.x)][static_cast<int>(blockPos.z - pos.z)][static_cast<int>(blockPos.y)];
            Block& adjacentBlock = adjacentChunk->blocks[adjLocalX][adjLocalZ][adjLocalY];

            // Se il blocco adiacente � trasparente, mostra la faccia
            if (adjacentBlock.isTransparent)
            {
                currentBlock.setFaceVisibility(static_cast<FaceType>(i), true);
            }
            else
            {
                currentBlock.setFaceVisibility(static_cast<FaceType>(i), false);
            }

            // Aggiorna la faccia opposta nel blocco adiacente
            updateBlock(adjacentBlock, i, blockPos);
        }
    }
}


Point2D World::getChunkCoordinates(const Point3D& pos) const {
    int chunkX = static_cast<int>(std::floor(pos.x / CHUNK_SIZE));
    int chunkZ = static_cast<int>(std::floor(pos.z / CHUNK_SIZE));
    return Point2D(chunkX, chunkZ);
}

Chunk* World::getChunk(Point2D pos) {
    auto it = chunksMap.find(pos);
    return (it != chunksMap.end()) ? &it->second : nullptr;
}

void World::placeBlock(const Point3D& blockPos, BlockType type) {
    Point2D chunkCoords = getChunkCoordinates(blockPos);
    Chunk* targetChunk = getChunk(chunkCoords);

    if (!targetChunk) {
        addChunk(chunkCoords);
        targetChunk = getChunk(chunkCoords);
    }

    if (targetChunk) {
        targetChunk->addBlock(blockPos, type);
        std::cout << "Blocco piazzato in (" << blockPos.x << ", " << blockPos.y << ", " << blockPos.z << ")" << std::endl;
    } else {
        std::cerr << "Impossibile creare il chunk per piazzare il blocco." << std::endl;
    }
}

void World::updateVisibleChunks(const Camera& camera, int renderDistance) {
    Point2D currentChunkCoords = getChunkCoordinates(camera.pos);

    for (int i = -renderDistance; i <= renderDistance; ++i) {
        for (int j = -renderDistance; j <= renderDistance; ++j) {
            Point2D chunkPos(currentChunkCoords.x + i, currentChunkCoords.z + j);
            if (chunksMap.find(chunkPos) == chunksMap.end()) {
                addChunk(chunkPos);
            }
        }
    }
}

bool Chunk::isBlock(Point3D blockPos) const
{
    // Calcola le coordinate relative al chunk corrente
    int localX = static_cast<int>(blockPos.x - pos.x);
    int localZ = static_cast<int>(blockPos.z - pos.z);
    int localY = static_cast<int>(blockPos.y);

    // Verifica se il blocco � all'interno del chunk corrente
    if (localX >= 0 && localX < CHUNK_SIZE &&
            localZ >= 0 && localZ < CHUNK_SIZE &&
            localY >= 0 && localY < CHUNK_HEIGHT)
    {
        return (!blocks[localX][localZ][localY].isTransparent);
    }

    // Se il blocco non � nel chunk corrente, calcola le coordinate del chunk adiacente
    Point2D adjacentChunkCoords = world.getChunkCoordinates(blockPos);
    Chunk* targetChunk = world.getChunk(adjacentChunkCoords);

    // Se il chunk adiacente non esiste, considera il blocco come non presente
    if (!targetChunk)
    {
        return false;
    }

    // Calcola le coordinate locali nel chunk adiacente
    int adjLocalX = static_cast<int>(blockPos.x - (adjacentChunkCoords.x * CHUNK_SIZE));
    int adjLocalZ = static_cast<int>(blockPos.z - (adjacentChunkCoords.z * CHUNK_SIZE));

    // Verifica se le coordinate locali sono valide nel chunk adiacente
    if (adjLocalX >= 0 && adjLocalX < CHUNK_SIZE &&
            adjLocalZ >= 0 && adjLocalZ < CHUNK_SIZE &&
            blockPos.y >= 0 && blockPos.y < CHUNK_HEIGHT)
    {
        Block& targetBlock = targetChunk->blocks[adjLocalX][adjLocalZ][static_cast<int>(blockPos.y)];
        return (!targetBlock.isTransparent);
    }

    // Se il blocco non � valido, restituisci false
    return false;
}

// Funzione per convertire gradi in radianti
float toRadians(float degrees)
{
    return degrees * M_PI / 180.0f;
}

void UIRenderer::drawText(const std::string& text, Point2D pos, void* font) {
    glDisable(GL_LIGHTING);
    glColor3f(1.0f, 1.0f, 1.0f);
    glRasterPos2f(pos.x, pos.z);

    for (char c : text) {
        glutBitmapCharacter(font, c);
    }

    glEnable(GL_LIGHTING);
}

void display()
{
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
        0.0f, 1.0f, 0.0f
    );

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
                const Chunk& chunk = it->second;

                // Disegna il chunk
                chunk.draw();

                if (showChunkBorder)
                {
                    glDisable(GL_LIGHTING); // Disabilita la luce
                    glColor3f(0, 0, 0); // Imposta il colore delle linee a grigio
                    glLineWidth(2.0f);
                    glBegin(GL_LINES);
                    float lineHeight = 500.0f;
                    float xMin = chunk.pos.x - 0.5f;
                    float xMax = chunk.pos.x + CHUNK_SIZE - 0.5f;
                    float zMin = chunk.pos.z - 0.5f;
                    float zMax = chunk.pos.z + CHUNK_SIZE - 0.5f;

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
        glVertex2f(0, glutGet(GLUT_WINDOW_HEIGHT) - 80); // Alto sinistro
        glVertex2f(0, glutGet(GLUT_WINDOW_HEIGHT));      // Basso sinistro
        glVertex2f(300, glutGet(GLUT_WINDOW_HEIGHT));    // Basso destro
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
        UIRenderer::drawText("Chunk corrente: (" + std::to_string(chunkCoords.x) + ", " + std::to_string(chunkCoords.z) + ")", Point2D(10, glutGet(GLUT_WINDOW_HEIGHT) - 60));

        // Ripristina le impostazioni OpenGL
        glPopMatrix();
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
    }

    glutSwapBuffers();
}

void keyboard(unsigned char key, int, int)
{
    float horizontalStep = 10.0f;
    float verticalStep = 5.0f;
    bool moved = false;

    switch (key)
    {
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
    case 'e':
        showTopFace = !showTopFace;
        break;
    case 'm':
        enableFaceOptimization = !enableFaceOptimization;
        break;
    case 'p':
        //world.placeBlock();
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

// Funzione per gestire il movimento del mouse con pulsante premuto
void motionMouse(int x, int y)
{
    int deltaX = x - lastMouseX;
    int deltaY = y - lastMouseY;

    camera.rot.yRot += static_cast<float>(deltaX) * 0.25f; // Rotazione orizzontale
    camera.rot.xRot -= static_cast<float>(deltaY) * 0.25f; // Rotazione verticale

    camera.rot.xRot = std::max(-89.9f, std::min(89.9f, camera.rot.xRot)); // Limita la rotazione verticale

    lastMouseX = x;
    lastMouseY = y;

    resetMousePosition(); // Reimposta il cursore al centro
    glutPostRedisplay();
}

// Funzione per gestire il movimento del mouse passivo
void passiveMouse(int x, int y)
{
    // Puoi implementare una logica simile a motionMouse() qui, se necessario
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
