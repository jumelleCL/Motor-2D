#include "Escena.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <vector>
#include <functional>
#include <cstring>
#include <cctype>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincodec.h>
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "uuid.lib")
#pragma comment(lib, "windowscodecs.lib")
#endif

using namespace Locomotora;

void Escena::LiberarNodo(Nodo* nodo)
{
    if (!nodo) return;
    for (auto* hijo : nodo->hijos)
        LiberarNodo(hijo);
    if (nodo->textura)
        SDL_DestroyTexture(nodo->textura);
    delete nodo;
}

static void LiberarHijos(Nodo* nodo)
{
    if (!nodo) return;
    for (auto* hijo : nodo->hijos)
        Escena::LiberarNodo(hijo);
    nodo->hijos.clear();
}

static bool RectangulosColisionan(const SDL_FRect& a, const SDL_FRect& b)
{
    return a.x < b.x + b.w && a.x + a.w > b.x && a.y < b.y + b.h && a.y + a.h > b.y;
}

static Punt PosicionMundial(const Nodo* nodo)
{
    Punt pos = nodo->posicion;
    const Nodo* actual = nodo->padre;
    while (actual)
    {
        pos.x += actual->posicion.x;
        pos.y += actual->posicion.y;
        actual = actual->padre;
    }
    return pos;
}

static Vector EscalaMundial(const Nodo* nodo)
{
    Vector esc = nodo->escala;
    const Nodo* actual = nodo->padre;
    while (actual)
    {
        esc.x *= actual->escala.x;
        esc.y *= actual->escala.y;
        actual = actual->padre;
    }
    return esc;
}

static SDL_FRect RectanguloMundial(const Nodo* nodo)
{
    Punt pos = PosicionMundial(nodo);
    Vector esc = EscalaMundial(nodo);
    return SDL_FRect{ pos.x, pos.y, nodo->size.x * esc.x, nodo->size.y * esc.y };
}

static SDL_FRect RectanguloColisionMundial(const Nodo* nodo)
{
    if (!nodo->collision.enabled) return SDL_FRect{ 0,0,0,0 };
    Punt pos = PosicionMundial(nodo);
    pos.x += nodo->collision.offset.x;
    pos.y += nodo->collision.offset.y;
    Vector esc = EscalaMundial(nodo);
    return SDL_FRect{ pos.x, pos.y, nodo->collision.size.x * esc.x, nodo->collision.size.y * esc.y };
}

static Nodo* BuscarColliderPropio(Nodo* nodo)
{
    if (!nodo) return nullptr;
    if (nodo->collision.enabled) return nodo;
    for (auto* hijo : nodo->hijos)
        if (Nodo* collider = BuscarColliderPropio(hijo))
            return collider;
    return nullptr;
}

static void RecolectarNodosMovilesYColisionadores(Nodo* nodo, std::vector<Nodo*>& moviles, std::vector<Nodo*>& colisionadores)
{
    if (!nodo) return;
    if (nodo->movement) moviles.push_back(nodo);
    if (nodo->collision.enabled) colisionadores.push_back(nodo);
    for (auto* hijo : nodo->hijos)
        RecolectarNodosMovilesYColisionadores(hijo, moviles, colisionadores);
}

static bool EsDescendiente(const Nodo* posibleHijo, const Nodo* posibleAncestro)
{
    const Nodo* actual = posibleHijo;
    while (actual)
    {
        if (actual == posibleAncestro) return true;
        actual = actual->padre;
    }
    return false;
}

#ifdef _WIN32
static std::wstring Utf8ToWide(const std::string& texto)
{
    if (texto.empty()) return {};
    int size = MultiByteToWideChar(CP_UTF8, 0, texto.c_str(), -1, nullptr, 0);
    std::wstring wide(size - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, texto.c_str(), -1, wide.data(), size);
    return wide;
}

static SDL_Texture* CargarTexturaImagen(SDL_Renderer* renderer, const std::string& ruta)
{
    SDL_Texture* textura = nullptr;
    IWICImagingFactory* fabrica = nullptr;
    IWICBitmapDecoder* decodificador = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICFormatConverter* convertidor = nullptr;
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool coInicializado = SUCCEEDED(hr);
    hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&fabrica));
    if (SUCCEEDED(hr))
    {
        std::wstring wpath = Utf8ToWide(ruta);
        hr = fabrica->CreateDecoderFromFilename(wpath.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decodificador);
        if (SUCCEEDED(hr)) hr = decodificador->GetFrame(0, &frame);
        if (SUCCEEDED(hr)) hr = fabrica->CreateFormatConverter(&convertidor);
        if (SUCCEEDED(hr)) hr = convertidor->Initialize(frame, GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeMedianCut);
        if (SUCCEEDED(hr))
        {
            UINT ancho = 0, alto = 0;
            frame->GetSize(&ancho, &alto);
            std::vector<unsigned char> pixeles((size_t)ancho * (size_t)alto * 4);
            hr = convertidor->CopyPixels(nullptr, ancho * 4, (UINT)pixeles.size(), pixeles.data());
            if (SUCCEEDED(hr))
            {
                textura = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_BGRA8888, SDL_TEXTUREACCESS_STATIC, (int)ancho, (int)alto);
                if (textura)
                {
                    SDL_UpdateTexture(textura, nullptr, pixeles.data(), (int)(ancho * 4));
                    SDL_SetTextureScaleMode(textura, SDL_SCALEMODE_LINEAR);
                    SDL_SetTextureBlendMode(textura, SDL_BLENDMODE_BLEND);
                }
            }
        }
    }
    if (convertidor) convertidor->Release();
    if (frame) frame->Release();
    if (decodificador) decodificador->Release();
    if (fabrica) fabrica->Release();
    if (coInicializado) CoUninitialize();
    if (!textura)
    {
        SDL_Surface* superficie = SDL_LoadBMP(ruta.c_str());
        if (superficie)
        {
            textura = SDL_CreateTextureFromSurface(renderer, superficie);
            if (textura) SDL_SetTextureBlendMode(textura, SDL_BLENDMODE_BLEND);
            SDL_DestroySurface(superficie);
        }
    }
    if (!textura)
        SDL_Log("Error cargando textura: %s", ruta.c_str());
    return textura;
}
#else
static SDL_Texture* CargarTexturaImagen(SDL_Renderer* renderer, const std::string& ruta)
{
    SDL_Surface* superficie = SDL_LoadBMP(ruta.c_str());
    if (!superficie) return nullptr;
    SDL_Texture* textura = SDL_CreateTextureFromSurface(renderer, superficie);
    if (textura) SDL_SetTextureBlendMode(textura, SDL_BLENDMODE_BLEND);
    SDL_DestroySurface(superficie);
    return textura;
}
#endif

Escena::Escena()
{
    raiz = new Nodo();
    raiz->nombre = "Nivel";
    raiz->padre = nullptr;
}

Escena::~Escena()
{
    if (raiz)
        LiberarNodo(raiz);
}

Nodo* Escena::CrearNodo(Nodo* padre, const std::string& nombre)
{
    if (!padre) padre = raiz;
    Nodo* nuevo = new Nodo();
    nuevo->nombre = nombre;
    nuevo->padre = padre;
    padre->hijos.push_back(nuevo);
    modificado = true;
    return nuevo;
}

void Escena::netejar()
{
    if (raiz)
        LiberarHijos(raiz);
}

void Escena::Update(float deltaTime, const bool* teclas)
{
    if (!raiz || !teclas) return;
    std::vector<Nodo*> nodosMoviles, nodosColisionadores;
    RecolectarNodosMovilesYColisionadores(raiz, nodosMoviles, nodosColisionadores);

    auto Colisiona = [&](Nodo* mover, Nodo* hitbox)
        {
            SDL_FRect rectMover = RectanguloColisionMundial(hitbox);
            for (auto* colisionador : nodosColisionadores)
            {
                if (colisionador == hitbox) continue;
                if (EsDescendiente(colisionador, mover)) continue;
                SDL_FRect rectCol = RectanguloColisionMundial(colisionador);
                if (RectangulosColisionan(rectMover, rectCol)) return true;
            }
            return false;
        };

    for (auto* mover : nodosMoviles)
    {
        Nodo* hitbox = BuscarColliderPropio(mover);
        if (!hitbox) hitbox = mover;

        float desplazamientoX = 0.0f, desplazamientoY = 0.0f;
        float paso = mover->speed * deltaTime;

        if (teclas[SDL_SCANCODE_W] || teclas[SDL_SCANCODE_UP]) desplazamientoY -= paso;
        if (teclas[SDL_SCANCODE_S] || teclas[SDL_SCANCODE_DOWN]) desplazamientoY += paso;
        if (teclas[SDL_SCANCODE_A] || teclas[SDL_SCANCODE_LEFT]) desplazamientoX -= paso;
        if (teclas[SDL_SCANCODE_D] || teclas[SDL_SCANCODE_RIGHT]) desplazamientoX += paso;

        if (desplazamientoX != 0.0f)
        {
            mover->posicion.x += desplazamientoX;
            if (Colisiona(mover, hitbox))
                mover->posicion.x -= desplazamientoX;
            else
                modificado = true;
        }
        if (desplazamientoY != 0.0f)
        {
            mover->posicion.y += desplazamientoY;
            if (Colisiona(mover, hitbox))
                mover->posicion.y -= desplazamientoY;
            else
                modificado = true;
        }
    }
}

static void DibujarNodo(SDL_Renderer* renderer, Nodo* nodo, bool modoEditor, const std::filesystem::path& rutaBase)
{
    if (!nodo) return;
    if (!nodo->visible && !modoEditor) return;

    SDL_FRect rect = RectanguloMundial(nodo);

    if (!nodo->textura && !nodo->asset.empty())
    {
        std::string rutaCompleta = (rutaBase / nodo->asset).string();
        nodo->textura = CargarTexturaImagen(renderer, rutaCompleta);
    }

    if (nodo->textura)
    {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_RenderTextureRotated(renderer, nodo->textura, nullptr, &rect, nodo->rotacion, nullptr, SDL_FLIP_NONE);
    }
    else
    {
        SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
        SDL_RenderFillRect(renderer, &rect);
    }

    for (auto* hijo : nodo->hijos)
        DibujarNodo(renderer, hijo, modoEditor, rutaBase);

    if (modoEditor && nodo->collision.enabled)
    {
        SDL_FRect rectColision = RectanguloColisionMundial(nodo);
        SDL_SetRenderDrawColor(renderer, 255, 50, 50, 255);
        SDL_RenderRect(renderer, &rectColision);
    }
}

void Escena::Render(SDL_Renderer* renderer, bool modoEditor)
{
    if (!raiz) return;
    std::filesystem::path base = rutaBase.empty() ? std::filesystem::path() : std::filesystem::path(rutaBase);
    for (auto* hijo : raiz->hijos)
        DibujarNodo(renderer, hijo, modoEditor, base);
}

void Escena::Guardar(const std::string& ruta) const
{
    std::ofstream archivo(ruta);
    if (!archivo.is_open()) return;
    std::function<void(const Nodo*, std::ostream&)> escribirNodo;
    escribirNodo = [&](const Nodo* nodo, std::ostream& os)
        {
            os << "{";
            os << "\"nombre\":\"" << nodo->nombre << "\",";
            os << "\"pos\":[" << nodo->posicion.x << "," << nodo->posicion.y << "],";
            os << "\"escala\":[" << nodo->escala.x << "," << nodo->escala.y << "],";
            os << "\"rot\":" << nodo->rotacion << ",";
            os << "\"collision_enabled\":" << (nodo->collision.enabled ? "true" : "false") << ",";
            os << "\"collision_offset\":[" << nodo->collision.offset.x << "," << nodo->collision.offset.y << "],";
            os << "\"collision_size\":[" << nodo->collision.size.x << "," << nodo->collision.size.y << "],";
            os << "\"collision_rot\":" << nodo->collision.rotation << ",";
            os << "\"movement\":" << (nodo->movement ? "true" : "false") << ",";
            os << "\"visible\":" << (nodo->visible ? "true" : "false") << ",";
            os << "\"speed\":" << nodo->speed << ",";
            os << "\"asset\":\"" << nodo->asset << "\",";
            os << "\"hijos\":[";
            for (size_t i = 0; i < nodo->hijos.size(); ++i)
            {
                escribirNodo(nodo->hijos[i], os);
                if (i < nodo->hijos.size() - 1) os << ",";
            }
            os << "]";
            os << "}";
        };
    archivo << "{\"nombre\":\"" << nombre << "\",\"raiz\":{";
    archivo << "\"hijos\":[";
    for (size_t i = 0; i < raiz->hijos.size(); ++i)
    {
        escribirNodo(raiz->hijos[i], archivo);
        if (i < raiz->hijos.size() - 1) archivo << ",";
    }
    archivo << "]}}";
}

static std::string ExtraerString(const std::string& texto, size_t inicio, size_t& fin)
{
    size_t a = texto.find('"', inicio);
    if (a == std::string::npos) return "";
    size_t b = texto.find('"', a + 1);
    if (b == std::string::npos) return "";
    fin = b + 1;
    return texto.substr(a + 1, b - a - 1);
}

static float ExtraerNumero(const std::string& texto, size_t inicio, size_t& fin)
{
    size_t a = inicio;
    while (a < texto.size() && (texto[a] == ' ' || texto[a] == '\t' || texto[a] == ':' || texto[a] == ','))
        a++;
    size_t b = a;
    while (b < texto.size() && (isdigit(texto[b]) || texto[b] == '.' || texto[b] == '-'))
        b++;
    if (b == a) { fin = a; return 0.0f; }
    fin = b;
    return std::stof(texto.substr(a, b - a));
}

static bool ExtraerBooleano(const std::string& texto, size_t inicio, size_t& fin)
{
    size_t a = inicio;
    while (a < texto.size() && (texto[a] == ' ' || texto[a] == '\t' || texto[a] == ':' || texto[a] == ','))
        a++;
    if (texto.compare(a, 4, "true") == 0) { fin = a + 4; return true; }
    if (texto.compare(a, 5, "false") == 0) { fin = a + 5; return false; }
    fin = a;
    return false;
}

static void ExtraerArray2(const std::string& texto, size_t inicio, float& x, float& y, size_t& fin)
{
    size_t a = texto.find('[', inicio);
    if (a == std::string::npos) { fin = inicio; return; }
    size_t b = texto.find(',', a + 1);
    if (b == std::string::npos) { fin = a + 1; return; }
    x = std::stof(texto.substr(a + 1, b - a - 1));
    size_t c = texto.find(']', b + 1);
    if (c == std::string::npos) { fin = b + 1; return; }
    y = std::stof(texto.substr(b + 1, c - b - 1));
    fin = c + 1;
}

static Nodo* ParsearNodo(const std::string& textoNodo, size_t& pos, Nodo* padre)
{
    if (pos >= textoNodo.size() || textoNodo[pos] != '{')
        return nullptr;
    pos++;
    Nodo* nodo = new Nodo();
    nodo->padre = padre;
    while (pos < textoNodo.size() && textoNodo[pos] != '}')
    {
        while (pos < textoNodo.size() && (textoNodo[pos] == ' ' || textoNodo[pos] == '\t' || textoNodo[pos] == ','))
            pos++;
        if (textoNodo[pos] == '"')
        {
            size_t finClave;
            std::string clave = ExtraerString(textoNodo, pos, finClave);
            pos = finClave;
            while (pos < textoNodo.size() && (textoNodo[pos] == ' ' || textoNodo[pos] == ':' || textoNodo[pos] == '\t'))
                pos++;
            if (clave == "nombre")
                nodo->nombre = ExtraerString(textoNodo, pos, pos);
            else if (clave == "asset")
                nodo->asset = ExtraerString(textoNodo, pos, pos);
            else if (clave == "collision_enabled")
                nodo->collision.enabled = ExtraerBooleano(textoNodo, pos, pos);
            else if (clave == "collision_offset")
                ExtraerArray2(textoNodo, pos, nodo->collision.offset.x, nodo->collision.offset.y, pos);
            else if (clave == "collision_size")
                ExtraerArray2(textoNodo, pos, nodo->collision.size.x, nodo->collision.size.y, pos);
            else if (clave == "collision_rot")
                nodo->collision.rotation = ExtraerNumero(textoNodo, pos, pos);
            else if (clave == "movement")
                nodo->movement = ExtraerBooleano(textoNodo, pos, pos);
            else if (clave == "visible")
                nodo->visible = ExtraerBooleano(textoNodo, pos, pos);
            else if (clave == "speed")
                nodo->speed = ExtraerNumero(textoNodo, pos, pos);
            else if (clave == "rot")
                nodo->rotacion = ExtraerNumero(textoNodo, pos, pos);
            else if (clave == "pos")
                ExtraerArray2(textoNodo, pos, nodo->posicion.x, nodo->posicion.y, pos);
            else if (clave == "escala")
                ExtraerArray2(textoNodo, pos, nodo->escala.x, nodo->escala.y, pos);
            else if (clave == "hijos")
            {
                size_t inicioArray = textoNodo.find('[', pos);
                if (inicioArray != std::string::npos)
                {
                    size_t arrPos = inicioArray + 1;
                    while (arrPos < textoNodo.size() && textoNodo[arrPos] != ']')
                    {
                        while (arrPos < textoNodo.size() && (textoNodo[arrPos] == ' ' || textoNodo[arrPos] == ',' || textoNodo[arrPos] == '\n'))
                            arrPos++;
                        if (textoNodo[arrPos] == '{')
                        {
                            Nodo* hijo = ParsearNodo(textoNodo, arrPos, nodo);
                            if (hijo) nodo->hijos.push_back(hijo);
                        }
                        else break;
                    }
                    pos = arrPos + 1;
                }
                else pos++;
            }
            else
            {
                size_t dummy;
                if (textoNodo[pos] == '"')
                    ExtraerString(textoNodo, pos, dummy);
                else if (textoNodo[pos] == '[')
                {
                    size_t finArray = textoNodo.find(']', pos);
                    if (finArray != std::string::npos) pos = finArray + 1;
                    else pos++;
                }
                else
                    ExtraerNumero(textoNodo, pos, dummy);
            }
        }
        else
            pos++;
    }
    if (pos < textoNodo.size() && textoNodo[pos] == '}')
        pos++;
    return nodo;
}

bool Escena::Cargar(const std::string& ruta)
{
    std::ifstream archivo(ruta);
    if (!archivo.is_open()) return false;
    netejar();
    std::stringstream buffer;
    buffer << archivo.rdbuf();
    std::string contenido = buffer.str();
    size_t posRaiz = contenido.find("\"raiz\"");
    if (posRaiz == std::string::npos) return false;
    size_t inicioHijos = contenido.find('[', posRaiz);
    if (inicioHijos == std::string::npos) return false;
    size_t pos = inicioHijos + 1;
    while (pos < contenido.size() && contenido[pos] != ']')
    {
        while (pos < contenido.size() && (contenido[pos] == ' ' || contenido[pos] == ',' || contenido[pos] == '\n'))
            pos++;
        if (contenido[pos] == '{')
        {
            Nodo* nodo = ParsearNodo(contenido, pos, raiz);
            if (nodo) raiz->hijos.push_back(nodo);
        }
        else break;
    }
    modificado = false;
    return true;
}