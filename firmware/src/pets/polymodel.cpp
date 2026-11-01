#include "polymodel.h"

#include <algorithm>

using Vec3 = PolyModel::Vec3;

static const float CAMERA_DIST = 420;
static const float AMBIENT = 0.62f, DIFFUSE = 0.42f;
static const Vec3 LIGHT = {-0.40f, 0.70f, 0.59f};  // arriba, a la izquierda y delante

PolyModel::PolyModel() {
  verts_ = new Vert[MAX_VERTS];
  tris_ = new Tri[MAX_TRIS];
  proj_ = new Projected[MAX_VERTS];
  order_ = new Order[MAX_TRIS];
}

PolyModel::~PolyModel() {
  delete[] verts_;
  delete[] tris_;
  delete[] proj_;
  delete[] order_;
}

int PolyModel::addPart(int parent, float px, float py, float pz, float bias, bool flat) {
  if (partCount_ >= MAX_PARTS) return partCount_ - 1;
  Part& p = parts_[partCount_];
  p.parent = parent;
  p.pivot = {px, py, pz};
  p.bias = bias;
  p.flat = flat;
  return partCount_++;
}

int PolyModel::addVert(int part, const Vec3& p, const Vec3& n) {
  if (vertCount_ >= MAX_VERTS) return 0;
  verts_[vertCount_] = {p, n, (uint8_t)part};
  return vertCount_++;
}

void PolyModel::addTri(int part, int a, int b, int c, uint8_t color) {
  if (triCount_ >= MAX_TRIS) return;
  tris_[triCount_++] = {(uint16_t)a, (uint16_t)b, (uint16_t)c, color, (uint8_t)part};
}

void PolyModel::lathe(int part, const Ring* rings, int count, int segments) {
  const float yTop = rings[0].y, yBottom = rings[count - 1].y;
  const float yMid = (yTop + yBottom) / 2, half = max((yTop - yBottom) / 2, 1.0f);
  const int first = vertCount_;

  for (int i = 0; i < count; i++) {
    const Ring& r = rings[i];
    const float s = constrain((r.y - yMid) / half, -1.0f, 1.0f) * 0.8f;
    for (int j = 0; j < segments; j++) {
      // Medio segmento de desfase: queda una cara plana mirando al frente
      float a = (j + 0.5f) * TWO_PI / segments;
      float c = cosf(a), n = sinf(a);
      float len = sqrtf(c * c + s * s + n * n);
      addVert(part, {r.ox + c * r.rx, r.y, r.oz + n * r.rz}, {c / len, s / len, n / len});
    }
  }

  for (int i = 0; i + 1 < count; i++) {
    for (int j = 0; j < segments; j++) {
      int j2 = (j + 1) % segments;
      int a = first + i * segments + j, b = first + i * segments + j2;
      int c = a + segments, d = b + segments;
      addTri(part, a, b, d, rings[i].color);
      addTri(part, a, d, c, rings[i].color);
    }
  }

  const Ring& top = rings[0];
  const Ring& bottom = rings[count - 1];
  int t = addVert(part, {top.ox, top.y, top.oz}, {0, 1, 0});
  int b = addVert(part, {bottom.ox, bottom.y, bottom.oz}, {0, -1, 0});
  int lastRing = first + (count - 1) * segments;
  for (int j = 0; j < segments; j++) {
    int j2 = (j + 1) % segments;
    addTri(part, t, first + j2, first + j, top.color);
    addTri(part, b, lastRing + j, lastRing + j2, count >= 2 ? rings[count - 2].color : top.color);
  }
}

void PolyModel::poly(int part, const Vec3* points, int count, uint8_t color, Vec3 normal) {
  int first = vertCount_;
  for (int i = 0; i < count; i++) addVert(part, points[i], normal);
  for (int i = 1; i + 1 < count; i++) addTri(part, first, first + i, first + i + 1, color);
}

void PolyModel::box(int part, float x0, float x1, float y0, float y1, float z0, float z1, uint8_t color) {
  const Vec3 front[] = {{x0, y0, z1}, {x1, y0, z1}, {x1, y1, z1}, {x0, y1, z1}};
  const Vec3 back[] = {{x1, y0, z0}, {x0, y0, z0}, {x0, y1, z0}, {x1, y1, z0}};
  const Vec3 right[] = {{x1, y0, z1}, {x1, y0, z0}, {x1, y1, z0}, {x1, y1, z1}};
  const Vec3 left[] = {{x0, y0, z0}, {x0, y0, z1}, {x0, y1, z1}, {x0, y1, z0}};
  const Vec3 top[] = {{x0, y1, z1}, {x1, y1, z1}, {x1, y1, z0}, {x0, y1, z0}};
  const Vec3 bottom[] = {{x0, y0, z0}, {x1, y0, z0}, {x1, y0, z1}, {x0, y0, z1}};
  poly(part, front, 4, color, {0, 0, 1});
  poly(part, back, 4, color, {0, 0, -1});
  poly(part, right, 4, color, {1, 0, 0});
  poly(part, left, 4, color, {-1, 0, 0});
  poly(part, top, 4, color, {0, 1, 0});
  poly(part, bottom, 4, color, {0, -1, 0});
}

void PolyModel::setColor(uint8_t i, uint8_t r, uint8_t g, uint8_t b) {
  colors_[i][0] = r;
  colors_[i][1] = g;
  colors_[i][2] = b;
}

// R = rotación Y * rotación X * rotación Z
static void buildRotation(float rx, float ry, float rz, float* m) {
  float sx = sinf(rx), cx = cosf(rx), sy = sinf(ry), cy = cosf(ry), sz = sinf(rz), cz = cosf(rz);
  m[0] = cy * cz + sy * sx * sz;
  m[1] = -cy * sz + sy * sx * cz;
  m[2] = sy * cx;
  m[3] = cx * sz;
  m[4] = cx * cz;
  m[5] = -sx;
  m[6] = -sy * cz + cy * sx * sz;
  m[7] = sy * sz + cy * sx * cz;
  m[8] = cy * cx;
}

static Vec3 rotate(const float* m, const Vec3& v) {
  return {m[0] * v.x + m[1] * v.y + m[2] * v.z, m[3] * v.x + m[4] * v.y + m[5] * v.z,
          m[6] * v.x + m[7] * v.y + m[8] * v.z};
}

void PolyModel::screenPos(int part, const Vec3& local, int& x, int& y) const {
  const Part& p = parts_[part];
  Vec3 w = rotate(p.R, {local.x * p.worldScale, local.y * p.worldScale, local.z * p.worldScale});
  w = {w.x + p.pos.x, w.y + p.pos.y, w.z + p.pos.z};
  float persp = CAMERA_DIST / (CAMERA_DIST - w.z * view_.scale);
  x = (int)lroundf(view_.x + w.x * view_.scale * persp);
  y = (int)lroundf(view_.floorY - w.y * view_.scale * persp);
}

void PolyModel::prepare(const View& view) {
  view_ = view;
  // Jerarquía: los padres siempre se añaden antes que sus hijos
  for (int i = 0; i < partCount_; i++) {
    Part& p = parts_[i];
    float local[9];
    buildRotation(p.rx, p.ry, p.rz, local);
    if (p.parent < 0) {
      memcpy(p.R, local, sizeof(local));
      p.pos = p.pivot;
      p.worldScale = p.scale;
      p.shown = p.visible;
    } else {
      const Part& q = parts_[p.parent];
      for (int r = 0; r < 3; r++)
        for (int c = 0; c < 3; c++)
          p.R[r * 3 + c] = q.R[r * 3] * local[c] + q.R[r * 3 + 1] * local[c + 3] + q.R[r * 3 + 2] * local[c + 6];
      Vec3 o = rotate(q.R, {p.pivot.x * q.worldScale, p.pivot.y * q.worldScale, p.pivot.z * q.worldScale});
      p.pos = {q.pos.x + o.x, q.pos.y + o.y, q.pos.z + o.z};
      p.worldScale = q.worldScale * p.scale;
      p.shown = p.visible && q.shown;
    }
  }

  minX_ = minY_ = 32767;
  maxX_ = maxY_ = -32768;
  for (int i = 0; i < vertCount_; i++) {
    const Vert& v = verts_[i];
    const Part& p = parts_[v.part];
    if (!p.shown) continue;

    Vec3 w = rotate(p.R, {v.p.x * p.worldScale, v.p.y * p.worldScale, v.p.z * p.worldScale});
    w = {w.x + p.pos.x, w.y + p.pos.y, w.z + p.pos.z};

    Vec3 n = rotate(p.R, v.n);
    float diffuse = max(0.0f, n.x * LIGHT.x + n.y * LIGHT.y + n.z * LIGHT.z);
    float light = min(1.0f, AMBIENT + DIFFUSE * diffuse);

    float persp = CAMERA_DIST / (CAMERA_DIST - w.z * view.scale);
    Projected& out = proj_[i];
    // Sin decimales a propósito: los vértices saltan de píxel en píxel
    out.x = (int16_t)lroundf(view.x + w.x * view.scale * persp);
    out.y = (int16_t)lroundf(view.floorY - w.y * view.scale * persp);
    out.z = w.z;
    out.light = (uint8_t)(light * 255);

    if (!p.flat) {
      minX_ = min<int>(minX_, out.x);
      maxX_ = max<int>(maxX_, out.x);
      minY_ = min<int>(minY_, out.y);
      maxY_ = max<int>(maxY_, out.y);
    }
  }

  visibleCount_ = 0;
  for (int i = 0; i < triCount_; i++) {
    const Tri& t = tris_[i];
    const Part& p = parts_[t.part];
    if (!p.shown) continue;
    const Projected& a = proj_[t.a];
    const Projected& b = proj_[t.b];
    const Projected& c = proj_[t.c];
    // La Y de pantalla va hacia abajo: lo que mira a la cámara da área negativa
    int32_t area = (b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y);
    if (area >= 0) continue;
    order_[visibleCount_++] = {(a.z + b.z + c.z) / 3 + p.bias, (uint16_t)i};
  }
  std::sort(order_, order_ + visibleCount_, [](const Order& l, const Order& r) { return l.key < r.key; });
}

void PolyModel::draw(Gfx& g) const {
  const int top = g.top, bottom = g.top + g.height;
  for (int i = 0; i < visibleCount_; i++) {
    const Tri& t = tris_[order_[i].tri];
    const Projected& a = proj_[t.a];
    const Projected& b = proj_[t.b];
    const Projected& c = proj_[t.c];
    if (max(max(a.y, b.y), c.y) < top || min(min(a.y, b.y), c.y) >= bottom) continue;

    const uint8_t* col = colors_[t.color];
    const bool flat = parts_[t.part].flat;
    auto shade = [&](const Projected& v) {
      uint16_t l = flat ? 256 : v.light;
      return PsxVert{v.x, v.y, (uint8_t)(col[0] * l >> 8), (uint8_t)(col[1] * l >> 8), (uint8_t)(col[2] * l >> 8)};
    };
    psxTriangle(g, shade(a), shade(b), shade(c), !flat);
  }
}

// Matriz de dithering de la PlayStation
static const int8_t DITHER[4][4] = {{-4, 0, -3, 1}, {2, -2, 3, -1}, {-3, 1, -4, 0}, {3, -1, 2, -2}};

void psxTriangle(Gfx& g, const PsxVert& pa, const PsxVert& pb, const PsxVert& pc, bool dither) {
  const PsxVert* v0 = &pa;
  const PsxVert* v1 = &pb;
  const PsxVert* v2 = &pc;
  if (v1->y < v0->y) std::swap(v0, v1);
  if (v2->y < v1->y) std::swap(v1, v2);
  if (v1->y < v0->y) std::swap(v0, v1);

  const int32_t x0 = v0->x, y0 = v0->y, x1 = v1->x, y1 = v1->y, x2 = v2->x, y2 = v2->y;
  if (y0 == y2) return;
  const int yStart = max<int32_t>(y0, g.top), yEnd = min<int32_t>(y2, g.top + g.height);
  if (yStart >= yEnd) return;

  const int64_t denom = (int64_t)(x1 - x0) * (y2 - y0) - (int64_t)(x2 - x0) * (y1 - y0);
  if (denom == 0) return;

  // Cada canal es un plano: valor = v0 + dx * (x - x0) + dy * (y - y0)
  auto gradX = [&](int c0, int c1, int c2) {
    return (int32_t)((((int64_t)(c1 - c0) * (y2 - y0) - (int64_t)(c2 - c0) * (y1 - y0)) * 65536) / denom);
  };
  auto gradY = [&](int c0, int c1, int c2) {
    return (int32_t)((((int64_t)(c2 - c0) * (x1 - x0) - (int64_t)(c1 - c0) * (x2 - x0)) * 65536) / denom);
  };
  const int32_t rdx = gradX(v0->r, v1->r, v2->r), rdy = gradY(v0->r, v1->r, v2->r);
  const int32_t gdx = gradX(v0->g, v1->g, v2->g), gdy = gradY(v0->g, v1->g, v2->g);
  const int32_t bdx = gradX(v0->b, v1->b, v2->b), bdy = gradY(v0->b, v1->b, v2->b);

  const int32_t slopeLong = ((x2 - x0) * 65536) / (y2 - y0);
  const int32_t slopeTop = y1 > y0 ? ((x1 - x0) * 65536) / (y1 - y0) : 0;
  const int32_t slopeBottom = y2 > y1 ? ((x2 - x1) * 65536) / (y2 - y1) : 0;

  // En triángulos muy finos el plano se dispara: nunca salir del color de sus vértices
  const int rMin = min(min(v0->r, v1->r), v2->r), rMax = max(max(v0->r, v1->r), v2->r);
  const int gMin = min(min(v0->g, v1->g), v2->g), gMax = max(max(v0->g, v1->g), v2->g);
  const int bMin = min(min(v0->b, v1->b), v2->b), bMax = max(max(v0->b, v1->b), v2->b);

  uint16_t* buffer = (uint16_t*)g.s.getPointer();
  const int width = g.s.width();

  for (int y = yStart; y < yEnd; y++) {
    int32_t xa = (x0 << 16) + slopeLong * (y - y0);
    int32_t xb = y < y1 ? (x0 << 16) + slopeTop * (y - y0) : (x1 << 16) + slopeBottom * (y - y1);
    if (xa > xb) std::swap(xa, xb);
    int xl = max((xa + 0xFFFF) >> 16, (int32_t)0);
    int xr = min((xb + 0xFFFF) >> 16, (int32_t)width);
    if (xl >= xr) continue;

    int32_t r = (v0->r << 16) + rdx * (xl - x0) + rdy * (y - y0);
    int32_t gr = (v0->g << 16) + gdx * (xl - x0) + gdy * (y - y0);
    int32_t b = (v0->b << 16) + bdx * (xl - x0) + bdy * (y - y0);
    uint16_t* row = buffer + (y - g.top) * width;
    const int8_t* pattern = DITHER[y & 3];

    for (int x = xl; x < xr; x++) {
      int d = dither ? pattern[x & 3] : 0;
      int cr = constrain(constrain(r >> 16, rMin, rMax) + d, 0, 255);
      int cg = constrain(constrain(gr >> 16, gMin, gMax) + d, 0, 255);
      int cb = constrain(constrain(b >> 16, bMin, bMax) + d, 0, 255);
      uint16_t c = ((cr & 0xF8) << 8) | ((cg & 0xFC) << 3) | (cb >> 3);
      row[x] = (uint16_t)((c >> 8) | (c << 8));  // el sprite guarda los bytes al revés
      r += rdx;
      gr += gdx;
      b += bdx;
    }
  }
}
