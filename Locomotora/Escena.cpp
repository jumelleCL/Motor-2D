#include "Escena.h"
#include <fstream>
#include <filesystem>
#include <functional>
#include <SDL3/SDL_image.h>
#include <sstream>
#include <cmath>

using namespace Locomotora;

static SDL_Texture* CargarTextura(SDL_Renderer* r, const std::string& path) {
    SDL_Texture* tex = IMG_LoadTexture(r, path.c_str());
    if (tex) {
        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
        SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_LINEAR);
    }
    return tex;
}

static SDL_Texture* ObtenerTexturaColor(SDL_Renderer* r, Uint8 color[4]) {
    static SDL_Texture* texGris = nullptr;
    static SDL_Texture* texRojo = nullptr;
    SDL_Texture*& tex = (color[0] == 200) ? texGris : texRojo;
    if (!tex) {
        tex = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, 1, 1);
        if (tex) {
            SDL_SetRenderTarget(r, tex);
            SDL_SetRenderDrawColor(r, color[0], color[1], color[2], color[3]);
            SDL_RenderClear(r);
            SDL_SetRenderTarget(r, nullptr);
            SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
        }
    }
    return tex;
}

static Punt PosicionMundial(const Nodo* n) {
    Punt pos = n->posicion;
    for (auto* p = n->padre; p; p = p->padre) {
        pos.x += p->posicion.x;
        pos.y += p->posicion.y;
    }
    return pos;
}

static Vector EscalaMundial(const Nodo* n) {
    Vector esc = n->escala;
    for (auto* p = n->padre; p; p = p->padre) {
        esc.x *= p->escala.x;
        esc.y *= p->escala.y;
    }
    return esc;
}

static float RotacionMundial(const Nodo* n) {
    float rot = n->rotacion;
    for (auto* p = n->padre; p; p = p->padre)
        rot += p->rotacion;
    return rot;
}

static SDL_FRect RectanguloMundial(const Nodo* n) {
    Punt pos = PosicionMundial(n);
    Vector esc = EscalaMundial(n);
    return { pos.x, pos.y, n->size.x * esc.x, n->size.y * esc.y };
}

static SDL_FRect ColisionMundial(const Nodo* n) {
    if (!n->collision.enabled) return { 0,0,0,0 };
    SDL_FRect rect = RectanguloMundial(n);
    rect.x += n->collision.offset.x;
    rect.y += n->collision.offset.y;
    rect.w = n->collision.size.x * (rect.w / n->size.x);
    rect.h = n->collision.size.y * (rect.h / n->size.y);
    return rect;
}

struct Vec2 { float x, y; };
struct OBB {
    Vec2 center;
    Vec2 half;
    float angle;
};

static OBB ObtenerOBB(const Nodo* n) {
    SDL_FRect r = ColisionMundial(n);
    OBB obb;
    obb.center.x = r.x + r.w * 0.5f;
    obb.center.y = r.y + r.h * 0.5f;
    obb.half.x = r.w * 0.5f;
    obb.half.y = r.h * 0.5f;
    obb.angle = RotacionMundial(n) + n->collision.rotation;
    return obb;
}

static void ProyectarOBB(const OBB& obb, const Vec2& axis, float& min, float& max) {
    float rad = obb.angle * 3.14159265358979f / 180.0f;
    float cosA = cosf(rad), sinA = sinf(rad);
    Vec2 ux = { cosA, sinA };
    Vec2 uy = { -sinA, cosA };
    Vec2 vertices[4] = {
        { -obb.half.x, -obb.half.y }, {  obb.half.x, -obb.half.y },
        {  obb.half.x,  obb.half.y }, { -obb.half.x,  obb.half.y }
    };
    min = INFINITY; max = -INFINITY;
    for (int i = 0; i < 4; ++i) {
        float vx = vertices[i].x * ux.x - vertices[i].y * uy.x + obb.center.x;
        float vy = vertices[i].x * ux.y - vertices[i].y * uy.y + obb.center.y;
        float proy = vx * axis.x + vy * axis.y;
        if (proy < min) min = proy;
        if (proy > max) max = proy;
    }
}

static bool ColisionOBB(const OBB& a, const OBB& b) {
    float radA = a.angle * 3.14159265358979f / 180.0f;
    float radB = b.angle * 3.14159265358979f / 180.0f;
    Vec2 axes[4] = {
        { cosf(radA), sinf(radA) }, { -sinf(radA), cosf(radA) },
        { cosf(radB), sinf(radB) }, { -sinf(radB), cosf(radB) }
    };
    for (int i = 0; i < 4; ++i) {
        float minA, maxA, minB, maxB;
        ProyectarOBB(a, axes[i], minA, maxA);
        ProyectarOBB(b, axes[i], minB, maxB);
        if (maxA < minB || maxB < minA) return false;
    }
    return true;
}

static bool EsDescendiente(const Nodo* hijo, const Nodo* padre) {
    while (hijo) {
        if (hijo == padre) return true;
        hijo = hijo->padre;
    }
    return false;
}

static Nodo* BuscarColliderPropio(Nodo* n) {
    if (!n) return nullptr;
    if (n->collision.enabled) return n;
    for (auto* h : n->hijos)
        if (auto* c = BuscarColliderPropio(h)) return c;
    return nullptr;
}

Escena::Escena() { raiz = new Nodo(); raiz->nombre = "Nivel"; }
Escena::~Escena() { LiberarNodo(raiz); }

void Escena::LiberarNodo(Nodo* nodo) {
    if (!nodo) return;
    for (auto* h : nodo->hijos) LiberarNodo(h);
    if (nodo->textura) SDL_DestroyTexture(nodo->textura);
    delete nodo;
}

Nodo* Escena::CrearNodo(Nodo* padre, const std::string& nombre) {
    if (!padre) padre = raiz;
    auto* n = new Nodo();
    n->nombre = nombre;
    n->padre = padre;
    padre->hijos.push_back(n);
    modificado = true;
    return n;
}

void Escena::netejar() {
    while (!raiz->hijos.empty()) {
        Nodo* hijo = raiz->hijos.back();
        raiz->hijos.pop_back();
        LiberarNodo(hijo);
    }
}

void Escena::Update(float dt, const bool* teclas) {
    if (!raiz || !teclas) return;

    std::vector<Nodo*> moviles, colisores;
    std::function<void(Nodo*)> recolectar = [&](Nodo* n) {
        if (!n) return;
        if (n->movement) moviles.push_back(n);
        if (n->collision.enabled) colisores.push_back(n);
        for (auto* h : n->hijos) recolectar(h);
        };
    recolectar(raiz);

    for (auto* mover : moviles) {
        auto* hitbox = BuscarColliderPropio(mover);
        if (!hitbox) hitbox = mover;

        float dx = 0, dy = 0, paso = mover->speed * dt;
        if (teclas[SDL_SCANCODE_W] || teclas[SDL_SCANCODE_UP])   dy -= paso;
        if (teclas[SDL_SCANCODE_S] || teclas[SDL_SCANCODE_DOWN]) dy += paso;
        if (teclas[SDL_SCANCODE_A] || teclas[SDL_SCANCODE_LEFT]) dx -= paso;
        if (teclas[SDL_SCANCODE_D] || teclas[SDL_SCANCODE_RIGHT])dx += paso;

        auto test = [&](float ox, float oy) -> bool {
            mover->posicion.x += ox;
            mover->posicion.y += oy;
            OBB moverOBB = ObtenerOBB(hitbox);
            for (auto* otro : colisores) {
                if (otro == hitbox) continue;
                if (EsDescendiente(otro, mover)) continue;
                if (ColisionOBB(moverOBB, ObtenerOBB(otro))) {
                    mover->posicion.x -= ox;
                    mover->posicion.y -= oy;
                    return true;
                }
            }
            modificado = true;
            return false;
            };

        if (dx != 0) test(dx, 0);
        if (dy != 0) test(0, dy);
    }
}

static void DibujarNodo(SDL_Renderer* r, Nodo* n, bool editor, const std::filesystem::path& base) {
    if (!n || (!n->visible && !editor)) return;
    SDL_FRect rect = RectanguloMundial(n);

    if (!n->textura && !n->asset.empty()) {
        std::string fullPath = (base / n->asset).string();
        n->textura = CargarTextura(r, fullPath);
    }

    if (n->textura) {
        SDL_RenderTextureRotated(r, n->textura, nullptr, &rect, n->rotacion, nullptr, SDL_FLIP_NONE);
    }
    else {
        Uint8 gris[4] = { 200,200,200,255 };
        SDL_Texture* tex = ObtenerTexturaColor(r, gris);
        if (tex) SDL_RenderTextureRotated(r, tex, nullptr, &rect, n->rotacion, nullptr, SDL_FLIP_NONE);
        else {
            SDL_SetRenderDrawColor(r, 200, 200, 200, 255);
            SDL_RenderFillRect(r, &rect);
        }
    }

    for (auto* h : n->hijos) DibujarNodo(r, h, editor, base);

    if (editor && n->collision.enabled) {
        SDL_FRect col = ColisionMundial(n);
        float rot = RotacionMundial(n) + n->collision.rotation;
        float cx = col.x + col.w / 2, cy = col.y + col.h / 2;
        float sinR = sinf(rot * 3.14159265359f / 180.0f);
        float cosR = cosf(rot * 3.14159265359f / 180.0f);
        float hw = col.w / 2, hh = col.h / 2;
        SDL_FPoint pts[4] = {
            { -hw, -hh }, {  hw, -hh }, {  hw,  hh }, { -hw,  hh }
        };
        for (int i = 0; i < 4; ++i) {
            float x = pts[i].x * cosR - pts[i].y * sinR + cx;
            float y = pts[i].x * sinR + pts[i].y * cosR + cy;
            pts[i].x = x; pts[i].y = y;
        }
        SDL_SetRenderDrawColor(r, 255, 50, 50, 255);
        for (int i = 0; i < 4; ++i)
            SDL_RenderLine(r, pts[i].x, pts[i].y, pts[(i + 1) % 4].x, pts[(i + 1) % 4].y);
    }
}

void Escena::Render(SDL_Renderer* renderer, bool modoEditor) {
    if (!raiz) return;
    for (auto* h : raiz->hijos)
        DibujarNodo(renderer, h, modoEditor, rutaBase);
}
void Escena::Guardar(const std::string& ruta) const {
    std::ofstream f(ruta);
    if (!f) return;
    f << "escena=" << nombre << "\n";
    f << "rutaBase=" << rutaBase << "\n";
    f << "nodos=[";
    for (size_t i = 0; i < raiz->hijos.size(); ++i) {
        std::function<void(const Nodo*)> escribir = [&](const Nodo* n) {
            f << "{";
            f << "nombre=" << n->nombre << ";";
            f << "pos=" << n->posicion.x << "," << n->posicion.y << ";";
            f << "escala=" << n->escala.x << "," << n->escala.y << ";";
            f << "size=" << n->size.x << "," << n->size.y << ";";
            f << "rot=" << n->rotacion << ";";
            f << "visible=" << (n->visible ? "1" : "0") << ";";
            f << "movement=" << (n->movement ? "1" : "0") << ";";
            f << "speed=" << n->speed << ";";
            f << "asset=" << n->asset << ";";
            f << "collision_enabled=" << (n->collision.enabled ? "1" : "0") << ";";
            f << "collision_offset=" << n->collision.offset.x << "," << n->collision.offset.y << ";";
            f << "collision_size=" << n->collision.size.x << "," << n->collision.size.y << ";";
            f << "collision_rot=" << n->collision.rotation << ";";
            f << "hijos=[";
            for (size_t j = 0; j < n->hijos.size(); ++j) {
                escribir(n->hijos[j]);
                if (j < n->hijos.size() - 1) f << ",";
            }
            f << "]}";
            };
        escribir(raiz->hijos[i]);
        if (i < raiz->hijos.size() - 1) f << ",";
    }
    f << "]\n";
}

bool Escena::Cargar(const std::string& ruta) {
    std::ifstream f(ruta);
    if (!f) return false;
    netejar();

    std::stringstream buffer;
    buffer << f.rdbuf();
    std::string contenido = buffer.str();

    size_t p = contenido.find("escena=");
    if (p != std::string::npos) {
        p += 7;
        size_t end = contenido.find('\n', p);
        nombre = contenido.substr(p, end - p);
    }
    p = contenido.find("rutaBase=");
    if (p != std::string::npos) {
        p += 9;
        size_t end = contenido.find('\n', p);
        rutaBase = contenido.substr(p, end - p);
    }

    p = contenido.find("nodos=[");
    if (p == std::string::npos) return false;
    p += 7;
    size_t end = contenido.find_last_of(']');
    if (end == std::string::npos) return false;
    std::string nodosStr = contenido.substr(p, end - p);

    std::function<Nodo* (const std::string&, size_t&, Nodo*)> leerNodo =
        [&](const std::string& str, size_t& i, Nodo* padre) -> Nodo* {
        while (i < str.size() && isspace(str[i])) i++;
        if (i >= str.size() || str[i] != '{') return nullptr;
        i++;

        Nodo* nodo = new Nodo();
        nodo->padre = padre;

        while (i < str.size()) {
            while (i < str.size() && isspace(str[i])) i++;
            if (i >= str.size()) break;
            if (str[i] == '}') { i++; break; }

            std::string clave;
            while (i < str.size() && str[i] != '=' && str[i] != '}') {
                clave += str[i];
                i++;
            }
            if (str[i] == '}') { i++; break; }
            i++;

            std::string valor;
            if (clave == "hijos") {
                if (i < str.size() && str[i] == '[') {
                    int nivel = 1;
                    valor += '[';
                    i++;
                    while (i < str.size() && nivel > 0) {
                        if (str[i] == '[') nivel++;
                        else if (str[i] == ']') nivel--;
                        valor += str[i];
                        i++;
                    }
                }
            }
            else {
                while (i < str.size() && str[i] != ';') {
                    valor += str[i];
                    i++;
                }
                i++;
            }

            auto trim = [](std::string& s) {
                size_t a = s.find_first_not_of(" \t\r\n");
                if (a == std::string::npos) s.clear();
                else s = s.substr(a);
                size_t b = s.find_last_not_of(" \t\r\n");
                if (b != std::string::npos) s = s.substr(0, b + 1);
                };
            trim(clave);
            trim(valor);

            if (clave == "nombre") nodo->nombre = valor;
            else if (clave == "asset") nodo->asset = valor;
            else if (clave == "visible") nodo->visible = (valor == "1" || valor == "true");
            else if (clave == "movement") nodo->movement = (valor == "1" || valor == "true");
            else if (clave == "speed") nodo->speed = valor.empty() ? 0 : std::stof(valor);
            else if (clave == "rot") nodo->rotacion = valor.empty() ? 0 : std::stof(valor);
            else if (clave == "collision_enabled") nodo->collision.enabled = (valor == "1" || valor == "true");
            else if (clave == "collision_rot") nodo->collision.rotation = valor.empty() ? 0 : std::stof(valor);
            else if (clave == "pos" || clave == "escala" || clave == "size" ||
                clave == "collision_offset" || clave == "collision_size") {
                size_t coma = valor.find(',');
                if (coma != std::string::npos) {
                    float a = std::stof(valor.substr(0, coma));
                    float b = std::stof(valor.substr(coma + 1));
                    if (clave == "pos") { nodo->posicion.x = a; nodo->posicion.y = b; }
                    else if (clave == "escala") { nodo->escala.x = a; nodo->escala.y = b; }
                    else if (clave == "size") { nodo->size.x = a; nodo->size.y = b; }
                    else if (clave == "collision_offset") { nodo->collision.offset.x = a; nodo->collision.offset.y = b; }
                    else if (clave == "collision_size") { nodo->collision.size.x = a; nodo->collision.size.y = b; }
                }
            }
            else if (clave == "hijos") {
                if (valor.size() >= 2 && valor[0] == '[' && valor.back() == ']') {
                    std::string inner = valor.substr(1, valor.size() - 2);
                    size_t pos = 0;
                    while (pos < inner.size()) {
                        while (pos < inner.size() && (inner[pos] == ',' || isspace(inner[pos]))) pos++;
                        if (pos < inner.size() && inner[pos] == '{') {
                            Nodo* hijo = leerNodo(inner, pos, nodo);
                            if (hijo) nodo->hijos.push_back(hijo);
                        }
                        else break;
                    }
                }
            }
        }
        return nodo;
        };

    size_t pos = 0;
    while (pos < nodosStr.size()) {
        while (pos < nodosStr.size() && (nodosStr[pos] == ',' || isspace(nodosStr[pos]))) pos++;
        if (pos < nodosStr.size() && nodosStr[pos] == '{') {
            Nodo* n = leerNodo(nodosStr, pos, raiz);
            if (n) raiz->hijos.push_back(n);
        }
        else break;
    }

    modificado = false;
    return true;
}