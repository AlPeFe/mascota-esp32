#pragma once
#include <Arduino.h>

#include "../gfx.h"

// Pequeño motor 3D al estilo PlayStation 1:
//  - sin búfer de profundidad: los triángulos se ordenan y se pintan de atrás adelante
//  - vértices redondeados a píxeles enteros (el "temblor" típico al moverse)
//  - sombreado Gouraud con dithering de 4x4
class PolyModel {
 public:
  struct Vec3 {
    float x, y, z;
  };

  // Anillo de un "torno": una sección horizontal de la pieza. Los anillos van
  // de arriba abajo; color es el de la banda entre este anillo y el siguiente.
  struct Ring {
    float y, rx, rz;
    uint8_t color;
    float ox, oz;  // desplazamiento del centro (para piezas curvas o pies)
  };

  struct Part {
    int8_t parent = -1;
    bool visible = true;
    bool shown = true;   // visible, y también sus padres
    bool flat = false;   // sin luz ni dithering: ojos, boca y otras "pegatinas"
    float bias = 0;      // empuja la pieza hacia delante al ordenar
    Vec3 pivot = {0, 0, 0};
    float rx = 0, ry = 0, rz = 0;
    float scale = 1;
    float R[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};  // transformación ya calculada
    Vec3 pos = {0, 0, 0};
    float worldScale = 1;
  };

  // Colocación en pantalla
  struct View {
    float x = 120, floorY = 236, scale = 1;
  };

  PolyModel();
  ~PolyModel();

  int addPart(int parent, float px, float py, float pz, float bias = 0, bool flat = false);
  Part& part(int i) { return parts_[i]; }

  void lathe(int part, const Ring* rings, int count, int segments);
  // Polígono convexo en sentido antihorario visto desde delante.
  void poly(int part, const Vec3* points, int count, uint8_t color, Vec3 normal = {0, 0, 1});
  void box(int part, float x0, float x1, float y0, float y1, float z0, float z1, uint8_t color);

  void setColor(uint8_t i, uint8_t r, uint8_t g, uint8_t b);

  void prepare(const View& view);  // una vez por fotograma
  void draw(Gfx& g) const;         // una vez por franja

  // Posición en pantalla de un punto de una pieza, según lo último preparado
  void screenPos(int part, const Vec3& local, int& x, int& y) const;

  // Caja en pantalla de lo último preparado
  int minX() const { return minX_; }
  int maxX() const { return maxX_; }
  int minY() const { return minY_; }
  int maxY() const { return maxY_; }

  // Cuánto ocupa el modelo: sirve para ajustar los topes y ver si algo se corta
  int vertCount() const { return vertCount_; }
  int triCount() const { return triCount_; }
  int partCount() const { return partCount_; }
  static int maxVerts() { return MAX_VERTS; }
  static int maxTris() { return MAX_TRIS; }
  static int maxParts() { return MAX_PARTS; }

 private:
  static const int MAX_VERTS = 1600;
  static const int MAX_TRIS = 1600;
  static const int MAX_PARTS = 56;
  static const int MAX_COLORS = 32;

  struct Vert {
    Vec3 p, n;
    uint8_t part;
  };
  struct Tri {
    uint16_t a, b, c;
    uint8_t color, part;
  };
  struct Projected {
    int16_t x, y;
    float z;
    uint8_t light;
  };
  struct Order {
    float key;
    uint16_t tri;
  };

  int addVert(int part, const Vec3& p, const Vec3& n);
  void addTri(int part, int a, int b, int c, uint8_t color);

  View view_;
  Vert* verts_;
  Tri* tris_;
  Projected* proj_;
  Order* order_;
  Part parts_[MAX_PARTS];
  uint8_t colors_[MAX_COLORS][3] = {};
  int vertCount_ = 0, triCount_ = 0, partCount_ = 0, visibleCount_ = 0;
  int minX_ = 0, maxX_ = 0, minY_ = 0, maxY_ = 0;
};

// Triángulo relleno en la franja actual, con color interpolado.
struct PsxVert {
  int16_t x, y;
  uint8_t r, g, b;
};
void psxTriangle(Gfx& g, const PsxVert& a, const PsxVert& b, const PsxVert& c, bool dither);
