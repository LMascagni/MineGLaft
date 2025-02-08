#ifndef BASE_LIB_H
#define BASE_LIB_H

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

#endif // BASE_LIB_H
