// ================================
// LIBRERIE
// ================================
#include <GL/glut.h>
#include <cmath>
#include <vector>
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
const int RENDER_DISTANCE = 5;  // Distanza di rendering

// ================================
// STRUTTURE E CLASSI
// ================================
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
    float r, g, b;
    Color(float red = 1.0f, float green = 1.0f, float blue = 1.0f) : r(red), g(green), b(blue) {}
    void apply() const
    {
        glColor3f(r, g, b);
    }
};

// Classe Camera
class Camera
{
public:
    Point3D pos;
    Rotation rot;
    Camera(Point3D initialPos = Point3D(0.0f, 0.0f, 0.0f), Rotation initialRot = Rotation(0.0f, 0.0f, 0.0f)) : pos(initialPos), rot(initialRot) {}
    void reset();
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
    STONE
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
class Block
{
public:
    Point3D pos;
    float scale;
    Rotation rot;
    BlockType type;
    Face faces[6];

    Block(Point3D position = Point3D(0.0f, 0.0f, 0.0f), float scaleFactor = 0.0f, Rotation rotation = Rotation(0.0f, 0.0f, 0.0f), BlockType blockType = BlockType::_VOID) : pos(position), scale(scaleFactor), rot(rotation), type(blockType)
    {
        // Inizializza le facce con valori predefiniti
        faces[static_cast<int>(FaceType::FRONT)]  = Face(true, Color(1.0f, 0.0f, 1.0f)); // Fronte (Magenta)
        faces[static_cast<int>(FaceType::BACK)]   = Face(true, Color(0.0f, 1.0f, 0.0f)); // Dietro (Verde)
        faces[static_cast<int>(FaceType::LEFT)]   = Face(true, Color(1.0f, 0.0f, 0.0f)); // Sinistra (Rosso)
        faces[static_cast<int>(FaceType::RIGHT)]  = Face(true, Color(0.0f, 1.0f, 1.0f)); // Destra (Ciano)
        faces[static_cast<int>(FaceType::TOP)]    = Face(true, Color(0.0f, 0.0f, 1.0f)); // Sopra (Blu)
        faces[static_cast<int>(FaceType::BOTTOM)] = Face(true, Color(1.0f, 1.0f, 0.0f)); // Sotto (Giallo)
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
    Chunk(Point2D position);
    void draw() const;
    bool isBlock(Point3D pos) const;
    void addBlock(const Point3D& blockPos, BlockType type);
    void updateAdjacentBlocks(const Point3D& blockPos);
};

// ================================
// PROTOTIPI DELLE FUNZIONI
// ================================
void init();
void addChunk(Point2D pos);
float toRadians(float degrees);
bool isFaceVisible(const Point3D& blockPos, const Point3D& faceNormalVect);
void display();
void drawText(const std::string& text, Point2D pos, void* font = GLUT_BITMAP_HELVETICA_18);
Point2D getChunkCoordinates(const Point3D pos);
Chunk* getChunk(Point2D pos);
void keyboard(unsigned char key, int x, int y);
void processMouse(int button, int state, int x, int y);
void motionMouse(int x, int y);
void passiveMouse(int x, int y);
void resetMousePosition();
void placeBlock();

// ================================
// VARIABILI GLOBALI
// ================================
Camera camera;
std::vector<Chunk> chunks;

bool showChunkBorder = false;
bool showData = false;

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
    glEnable(GL_DEPTH_TEST); // Abilita il test di profondità
    glClearColor(0.2f, 0.2f, 0.2f, 1.0f); // Colore di sfondo grigio scuro
    camera.reset();

    // Aggiungi chunk con coordinate positive e negative
    for (int i = -2; i <= 2; i++)
    {
        for (int j = -2; j <= 2; j++)
        {
            addChunk(Point2D(i, j));
        }
    }
}

void Camera::reset()
{
    pos = Point3D(-48.0f, 110.0f, -48.0f);
    rot = Rotation(-30.0f, 45.0f, 0.0f);
    glutPostRedisplay();
}

void Block::draw(const Chunk& chunk) const
{
    if (type == BlockType::_VOID || type == BlockType::AIR)
        return; // Non disegna i blocchi vuoti o d'aria

    glPushMatrix();
    glTranslatef(pos.x, pos.y, pos.z);
    glScalef(scale, scale, scale);
    glRotatef(rot.xRot, 1.0f, 0.0f, 0.0f); // Rotazione attorno all'asse X
    glRotatef(rot.yRot, 0.0f, 1.0f, 0.0f); // Rotazione attorno all'asse Y
    glRotatef(rot.zRot, 0.0f, 0.0f, 1.0f); // Rotazione attorno all'asse Z
/*
    int bloccoX = static_cast<int>(pos.x - chunk.pos.x);
    int bloccoZ = static_cast<int>(pos.z - chunk.pos.z);
    int bloccoY = static_cast<int>(pos.y);
*/
    glBegin(GL_QUADS);

    // Fronte (Magenta)
    if (faces[static_cast<int>(FaceType::FRONT)].isVisible && isFaceVisible(pos, Point3D(0.0f, 0.0f, 1.0f)))
    {
        faces[static_cast<int>(FaceType::FRONT)].color.apply();
        glVertex3f(-0.5f, -0.5f, 0.5f);
        glVertex3f(0.5f, -0.5f, 0.5f);
        glVertex3f(0.5f, 0.5f, 0.5f);
        glVertex3f(-0.5f, 0.5f, 0.5f);
    }

    // Dietro (Verde)
    if (faces[static_cast<int>(FaceType::BACK)].isVisible && isFaceVisible(pos, Point3D(0.0f, 0.0f, -1.0f)))
    {
        faces[static_cast<int>(FaceType::BACK)].color.apply();
        glVertex3f(-0.5f, -0.5f, -0.5f);
        glVertex3f(0.5f, -0.5f, -0.5f);
        glVertex3f(0.5f, 0.5f, -0.5f);
        glVertex3f(-0.5f, 0.5f, -0.5f);
    }

    // Sinistra (Rosso)
    if (faces[static_cast<int>(FaceType::LEFT)].isVisible && isFaceVisible(pos, Point3D(-1.0f, 0.0f, 0.0f)))
    {
        faces[static_cast<int>(FaceType::LEFT)].color.apply();
        glVertex3f(-0.5f, -0.5f, -0.5f);
        glVertex3f(-0.5f, -0.5f, 0.5f);
        glVertex3f(-0.5f, 0.5f, 0.5f);
        glVertex3f(-0.5f, 0.5f, -0.5f);
    }

    // Destra (Ciano)
    if (faces[static_cast<int>(FaceType::RIGHT)].isVisible && isFaceVisible(pos, Point3D(1.0f, 0.0f, 0.0f)))
    {
        faces[static_cast<int>(FaceType::RIGHT)].color.apply();
        glVertex3f(0.5f, -0.5f, -0.5f);
        glVertex3f(0.5f, -0.5f, 0.5f);
        glVertex3f(0.5f, 0.5f, 0.5f);
        glVertex3f(0.5f, 0.5f, -0.5f);
    }

    // Sopra (Blu)
    if (faces[static_cast<int>(FaceType::TOP)].isVisible && isFaceVisible(pos, Point3D(0.0f, 1.0f, 0.0f)))
    {
        faces[static_cast<int>(FaceType::TOP)].color.apply();
        glVertex3f(-0.5f, 0.5f, -0.5f);
        glVertex3f(0.5f, 0.5f, -0.5f);
        glVertex3f(0.5f, 0.5f, 0.5f);
        glVertex3f(-0.5f, 0.5f, 0.5f);
    }

    // Sotto (Giallo)
    if (faces[static_cast<int>(FaceType::BOTTOM)].isVisible && isFaceVisible(pos, Point3D(0.0f, -1.0f, 0.0f)))
    {
        faces[static_cast<int>(FaceType::BOTTOM)].color.apply();
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

bool isFaceVisible(const Point3D& blockPos, const Point3D& faceNormalVect)
{
    Point3D directionVect(blockPos.x - camera.pos.x, blockPos.y - camera.pos.y, blockPos.z - camera.pos.z);
    directionVect.normalize(); // Normalize the direction vector
    float dotProduct = directionVect.x * faceNormalVect.x + directionVect.y * faceNormalVect.y + directionVect.z * faceNormalVect.z;
    return dotProduct < 0.0f; // Face is visible if the dot product is positive
}

void addChunk(Point2D pos)
{
    chunks.emplace_back(pos);
}

// Costruttore di Chunk
Chunk::Chunk(Point2D position) : pos(Point2D(position.x * CHUNK_SIZE, position.z * CHUNK_SIZE))
{
    blocks.resize(CHUNK_SIZE, std::vector<std::vector<Block>>(CHUNK_SIZE, std::vector<Block>(CHUNK_HEIGHT)));

    for (int x = 0; x < CHUNK_SIZE; ++x)
    {
        for (int z = 0; z < CHUNK_SIZE; ++z)
        {
            for (int y = 0; y < CHUNK_HEIGHT; ++y)
            {
                float posX = pos.x + x;
                float posY = static_cast<float>(y);
                float posZ = pos.z + z;

                BlockType blockType = (posY > (CHUNK_HEIGHT / 2) + x + z) ? BlockType::AIR : BlockType::TEST;

                // Crea il blocco
                Block newBlock(Point3D(posX, posY, posZ), 1.0f, Rotation(0.0f, 0.0f, 0.0f), blockType);

                if (blockType != BlockType::AIR && blockType != BlockType::_VOID)
                {
                    // Imposta la visibilità delle facce basandosi sui blocchi adiacenti
                    newBlock.setFaceVisibility(FaceType::FRONT, !isBlock(Point3D(posX, posY, posZ + 1)));
                    newBlock.setFaceVisibility(FaceType::BACK, !isBlock(Point3D(posX, posY, posZ - 1)));
                    newBlock.setFaceVisibility(FaceType::LEFT, !isBlock(Point3D(posX - 1, posY, posZ)));
                    newBlock.setFaceVisibility(FaceType::RIGHT, !isBlock(Point3D(posX + 1, posY, posZ)));
                    newBlock.setFaceVisibility(FaceType::TOP, !isBlock(Point3D(posX, posY + 1, posZ)));
                    newBlock.setFaceVisibility(FaceType::BOTTOM, !isBlock(Point3D(posX, posY - 1, posZ)));

                    // Aggiorna le facce dei blocchi adiacenti
                    updateAdjacentBlocks(Point3D(posX, posY, posZ));
                }

                blocks[x][z][y] = newBlock;
            }
        }
    }
}

// Metodo per disegnare il chunk
void Chunk::draw() const
{
    for (int x = 0; x < CHUNK_SIZE; ++x)
    {
        for (int z = 0; z < CHUNK_SIZE; ++z)
        {
            for (int y = 0; y < CHUNK_HEIGHT; ++y)
            {
                const Block& block = blocks[x][z][y];

                // Verifica se il blocco ha almeno una faccia visibile
                bool hasVisibleFace = false;
                for (int i = 0; i < 6; ++i)
                {
                    if (block.faces[i].isVisible)
                    {
                        hasVisibleFace = true;
                        break;
                    }
                }

                if (hasVisibleFace)
                {
                    block.draw(*this);
                }
            }
        }
    }
}

void Chunk::addBlock(const Point3D& blockPos, BlockType type)
{
    // Verifica se la posizione del blocco è valida nel chunk
    int localX = static_cast<int>(blockPos.x - pos.x);
    int localZ = static_cast<int>(blockPos.z - pos.z);
    int localY = static_cast<int>(blockPos.y);

    if (localX < 0 || localX >= CHUNK_SIZE || localZ < 0 || localZ >= CHUNK_SIZE || localY < 0 || localY >= CHUNK_HEIGHT)
    {
        return; // Posizione fuori dai limiti del chunk
    }

    // Crea il nuovo blocco
    Block newBlock(blockPos, 1.0f, Rotation(0.0f, 0.0f, 0.0f), type);

    // Imposta la visibilità delle facce del nuovo blocco
    if (isBlock(Point3D(blockPos.x, blockPos.y, blockPos.z + 1))) newBlock.setFaceVisibility(FaceType::FRONT, false);
    if (isBlock(Point3D(blockPos.x, blockPos.y, blockPos.z - 1))) newBlock.setFaceVisibility(FaceType::BACK, false);
    if (isBlock(Point3D(blockPos.x - 1, blockPos.y, blockPos.z))) newBlock.setFaceVisibility(FaceType::LEFT, false);
    if (isBlock(Point3D(blockPos.x + 1, blockPos.y, blockPos.z))) newBlock.setFaceVisibility(FaceType::RIGHT, false);
    if (isBlock(Point3D(blockPos.x, blockPos.y + 1, blockPos.z))) newBlock.setFaceVisibility(FaceType::TOP, false);
    if (isBlock(Point3D(blockPos.x, blockPos.y - 1, blockPos.z))) newBlock.setFaceVisibility(FaceType::BOTTOM, false);

    // Aggiungi il blocco al chunk
    blocks[localX][localZ][localY] = newBlock;

    // Aggiorna la visibilità delle facce dei blocchi adiacenti
    updateAdjacentBlocks(blockPos);
}

void Chunk::updateAdjacentBlocks(const Point3D& blockPos)
{
    std::vector<Point3D> directions = {
        Point3D(0, 0, 1),  // Fronte
        Point3D(0, 0, -1), // Dietro
        Point3D(-1, 0, 0), // Sinistra
        Point3D(1, 0, 0),  // Destra
        Point3D(0, 1, 0),  // Sopra
        Point3D(0, -1, 0)  // Sotto
    };

    for (const auto& dir : directions)
    {
        Point3D adjPos(blockPos.x + dir.x, blockPos.y + dir.y, blockPos.z + dir.z);

        // Verifica se il blocco adiacente esiste
        if (isBlock(adjPos))
        {
            int adjLocalX = static_cast<int>(adjPos.x - pos.x);
            int adjLocalZ = static_cast<int>(adjPos.z - pos.z);
            int adjLocalY = static_cast<int>(adjPos.y);

            // Normalizza le coordinate locali
            adjLocalX = ((adjLocalX + CHUNK_SIZE) % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
            adjLocalZ = ((adjLocalZ + CHUNK_SIZE) % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;

            if (adjLocalX >= 0 && adjLocalX < CHUNK_SIZE &&
                adjLocalZ >= 0 && adjLocalZ < CHUNK_SIZE &&
                adjLocalY >= 0 && adjLocalY < CHUNK_HEIGHT)
            {
                Block& adjBlock = blocks[adjLocalX][adjLocalZ][adjLocalY];

                // Determina la faccia da nascondere/sblocchiare
                if (dir == Point3D(0, 0, 1)) adjBlock.setFaceVisibility(FaceType::BACK, false);
                if (dir == Point3D(0, 0, -1)) adjBlock.setFaceVisibility(FaceType::FRONT, false);
                if (dir == Point3D(-1, 0, 0)) adjBlock.setFaceVisibility(FaceType::RIGHT, false);
                if (dir == Point3D(1, 0, 0)) adjBlock.setFaceVisibility(FaceType::LEFT, false);
                if (dir == Point3D(0, 1, 0)) adjBlock.setFaceVisibility(FaceType::BOTTOM, false);
                if (dir == Point3D(0, -1, 0)) adjBlock.setFaceVisibility(FaceType::TOP, false);
            }
        }
    }
}

// Funzione per ottenere un chunk specifico
Chunk* getChunk(Point2D pos)
{
    for (auto& chunk : chunks)
    {
        Point2D currentChunk = getChunkCoordinates(Point3D(chunk.pos.x, 0.0f, chunk.pos.z));
        if (currentChunk.x == pos.x && currentChunk.z == pos.z)
        {
            return &chunk;
        }
    }
    return nullptr; // Chunk non trovato
}

bool Chunk::isBlock(Point3D blockPos) const
{
    // Calcola le coordinate relative al chunk corrente
    int localX = static_cast<int>(blockPos.x - pos.x);
    int localZ = static_cast<int>(blockPos.z - pos.z);
    int localY = static_cast<int>(blockPos.y);

    // Normalizza le coordinate locali per gestire i casi negativi
    localX = ((localX + CHUNK_SIZE) % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;
    localZ = ((localZ + CHUNK_SIZE) % CHUNK_SIZE + CHUNK_SIZE) % CHUNK_SIZE;

    // Verifica se le coordinate locali sono valide
    if (localX >= 0 && localX < CHUNK_SIZE &&
        localZ >= 0 && localZ < CHUNK_SIZE &&
        localY >= 0 && localY < CHUNK_HEIGHT)
    {
        return (blocks[localX][localZ][localY].type != BlockType::_VOID &&
                blocks[localX][localZ][localY].type != BlockType::AIR);
    }

    // Se il blocco non è nel chunk corrente, controlla nei chunk adiacenti
    int globalX = static_cast<int>(pos.x) + localX;
    int globalZ = static_cast<int>(pos.z) + localZ;

    int chunkX = static_cast<int>(std::floor(globalX / static_cast<float>(CHUNK_SIZE)));
    int chunkZ = static_cast<int>(std::floor(globalZ / static_cast<float>(CHUNK_SIZE)));

    Chunk* targetChunk = getChunk(Point2D(chunkX, chunkZ));
    if (!targetChunk)
    {
        return false; // Il chunk non esiste
    }

    // Calcola le coordinate locali nel chunk trovato
    int adjLocalX = ((globalX - (chunkX * CHUNK_SIZE)) + CHUNK_SIZE) % CHUNK_SIZE;
    int adjLocalZ = ((globalZ - (chunkZ * CHUNK_SIZE)) + CHUNK_SIZE) % CHUNK_SIZE;

    // Verifica se le coordinate locali sono valide
    if (adjLocalX >= 0 && adjLocalX < CHUNK_SIZE &&
        adjLocalZ >= 0 && adjLocalZ < CHUNK_SIZE &&
        blockPos.y >= 0 && blockPos.y < CHUNK_HEIGHT)
    {
        return (targetChunk->blocks[adjLocalX][adjLocalZ][static_cast<int>(blockPos.y)].type != BlockType::_VOID &&
                targetChunk->blocks[adjLocalX][adjLocalZ][static_cast<int>(blockPos.y)].type != BlockType::AIR);
    }

    return false; // Blocco non esiste
}

Point2D getChunkCoordinates(const Point3D pos)
{
    int chunkX = static_cast<int>(std::floor(pos.x / CHUNK_SIZE));
    int chunkZ = static_cast<int>(std::floor(pos.z / CHUNK_SIZE));
    return Point2D(chunkX, chunkZ);
}

// Funzione per convertire gradi in radianti
float toRadians(float degrees)
{
    return degrees * M_PI / 180.0f;
}

void drawText(const std::string& text, Point2D pos, void* font)
{
    glRasterPos2f(pos.x, pos.z);
    for (char c : text)
    {
        glutBitmapCharacter(font, c);
    }
}

void display()
{
    // Calcola il tempo trascorso dall'ultimo frame
    auto currentFrameTime = std::chrono::steady_clock::now();
    float deltaTime = std::chrono::duration<float>(currentFrameTime - lastFrameTime).count();
    lastFrameTime = currentFrameTime;

    // Calcola gli FPS
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
        camera.pos.x, camera.pos.y, camera.pos.z, // Posizione della camera
        lookX, lookY, lookZ,                     // Punto di guardia
        0.0f, 1.0f, 0.0f                        // Vettore "up"
    );



    for (const auto& chunk : chunks)
    {
        chunk.draw();

        if(showChunkBorder)
        {
            // Disegna le linee verticali grigie che delimitano i chunk
            glColor3f(0.5f, 0.5f, 0.5f); // Grigio
            glLineWidth(2.0f); // Spessore delle linee

            glBegin(GL_LINES);

            float lineHeight = 500.0f;

            float xMin = chunk.pos.x - 0.5f;
            float xMax = chunk.pos.x + CHUNK_SIZE - 0.5f;
            float zMin = chunk.pos.z - 0.5f;
            float zMax = chunk.pos.z + CHUNK_SIZE - 0.5f;

            // Linea verticale sinistra
            glVertex3f(xMin, 0.0f, zMin);
            glVertex3f(xMin, lineHeight, zMin);

            // Linea verticale destra
            glVertex3f(xMax, 0.0f, zMin);
            glVertex3f(xMax, lineHeight, zMin);

            // Linea verticale anteriore
            glVertex3f(xMax, 0.0f, zMax);
            glVertex3f(xMax, lineHeight, zMax);

            // Linea verticale posteriore
            glVertex3f(xMin, 0.0f, zMax);
            glVertex3f(xMin, lineHeight, zMax);

            glEnd();
        }
    }


    if(showData)
    {
        // Disegna il testo sullo schermo
        glMatrixMode(GL_PROJECTION);
        glPushMatrix(); // Salva la matrice corrente
        glLoadIdentity();
        glOrtho(0, glutGet(GLUT_WINDOW_WIDTH), 0, glutGet(GLUT_WINDOW_HEIGHT), -1, 1); // Modalità ortografica
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix(); // Salva la matrice corrente
        glLoadIdentity();

        glColor3f(1.0f, 1.0f, 1.0f); // Bianco

        drawText("FPS: " + std::to_string(static_cast<int>(fps)), Point2D(10, glutGet(GLUT_WINDOW_HEIGHT) - 30));

        drawText("Rotazione Camera: (" + std::to_string(camera.rot.xRot) + ", " + std::to_string(camera.rot.yRot) + ", " + std::to_string(camera.rot.zRot) + ")", Point2D(10, 30));
        drawText("Posizione: (" + std::to_string(camera.pos.x) + ", " + std::to_string(camera.pos.y) + ", " + std::to_string(camera.pos.z) + ")", Point2D(10, 60));
        Point2D chunkCoords = getChunkCoordinates(camera.pos);
        drawText("Chunk corrente: (" + std::to_string(chunkCoords.x) + ", " + std::to_string(chunkCoords.z) + ")", Point2D(10, 90));

        glPopMatrix(); // Ripristina la matrice modelview
        glMatrixMode(GL_PROJECTION);
        glPopMatrix(); // Ripristina la matrice proiezione
        glMatrixMode(GL_MODELVIEW);
    }

    glutSwapBuffers();
}

void placeBlock()
{
    // Calcola la direzione del raggio partendo dalla telecamera
    float cosPitch = cos(toRadians(camera.rot.xRot));
    float sinPitch = sin(toRadians(camera.rot.xRot));
    float cosYaw = cos(toRadians(camera.rot.yRot));
    float sinYaw = sin(toRadians(camera.rot.yRot));

    Point3D rayDirection(cosPitch * cosYaw, sinPitch, cosPitch * sinYaw);

    // Normalizza il vettore direzione
    rayDirection.normalize();

    // Definisci una posizione di test lungo il raggio
    Point3D blockPosition = camera.pos + (rayDirection * 5.0f); // Esempio: 5 unità davanti alla telecamera

    // Arrotonda le coordinate del blocco alle posizioni intere
    blockPosition.x = std::round(blockPosition.x);
    blockPosition.y = std::round(blockPosition.y);
    blockPosition.z = std::round(blockPosition.z);

    // Trova il chunk corrispondente
    Point2D chunkCoordinates = getChunkCoordinates(blockPosition);
    Chunk* targetChunk = getChunk(chunkCoordinates);

    if (!targetChunk)
    {
        // Se il chunk non esiste, crealo prima di aggiungere il blocco
        addChunk(chunkCoordinates);
        targetChunk = getChunk(chunkCoordinates);

        if (!targetChunk)
        {
            std::cerr << "Impossibile creare il chunk per piazzare il blocco." << std::endl;
            return;
        }
    }

    // Aggiungi il blocco al chunk
    targetChunk->addBlock(blockPosition, BlockType::TEST);

    std::cout << "Blocco piazzato in (" << blockPosition.x << ", " << blockPosition.y << ", " << blockPosition.z << ")" << std::endl;

    // Richiedi il ridisegno della scena
    glutPostRedisplay();
}

void keyboard(unsigned char key, int, int)
{
    float horizontalStep = 1.0f;
    float verticalStep = 1.0f;

    switch (key)
    {
    case 'w':
        camera.pos.x += horizontalStep * cos(toRadians(camera.rot.yRot));
        camera.pos.z += horizontalStep * sin(toRadians(camera.rot.yRot));
        break;
    case 's':
        camera.pos.x -= horizontalStep * cos(toRadians(camera.rot.yRot));
        camera.pos.z -= horizontalStep * sin(toRadians(camera.rot.yRot));
        break;
    case 'd':
        camera.pos.x -= horizontalStep * sin(toRadians(camera.rot.yRot));
        camera.pos.z += horizontalStep * cos(toRadians(camera.rot.yRot));
        break;
    case 'a':
        camera.pos.x += horizontalStep * sin(toRadians(camera.rot.yRot));
        camera.pos.z -= horizontalStep * cos(toRadians(camera.rot.yRot));
        break;
    case ' ':
        camera.pos.y += verticalStep;
        break;
    case 'S':
        camera.pos.y -= verticalStep;
        break;
    case 'r':
        camera.reset();
        break;
    case 'b':
        showChunkBorder = !showChunkBorder;
        break;
    case 'n':
        showData = !showData;
        break;
    case 'p':
        placeBlock();
        break;
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
