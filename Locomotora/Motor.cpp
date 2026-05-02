#include "Motor.h"
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"
#include <filesystem>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <functional>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shobjidl_core.h>
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "uuid.lib")
#pragma comment(lib, "shell32.lib")
#endif

using namespace Locomotora;

static std::string Utf8FromWide(const std::wstring& wide)
{
#ifdef _WIN32
    if (wide.empty()) return {};
    int size = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string utf8(size - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, utf8.data(), size, nullptr, nullptr);
    return utf8;
#else
    return {};
#endif
}

#ifdef _WIN32
static std::string SeleccionarImagenWindows()
{
    IFileOpenDialog* dialogo = nullptr;
    std::string resultado;
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    bool coOk = SUCCEEDED(hr);
    hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&dialogo));
    if (SUCCEEDED(hr))
    {
        COMDLG_FILTERSPEC filtros[] = { { L"Imágenes", L"*.png;*.jpg;*.jpeg;*.bmp;*.tga" }, { L"Todos", L"*.*" } };
        dialogo->SetTitle(L"Elegir imagen");
        dialogo->SetFileTypes(2, filtros);
        dialogo->SetOptions(FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST);
        hr = dialogo->Show(nullptr);
        if (SUCCEEDED(hr))
        {
            IShellItem* item = nullptr;
            if (SUCCEEDED(dialogo->GetResult(&item)))
            {
                PWSTR ruta = nullptr;
                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &ruta)))
                {
                    resultado = Utf8FromWide(ruta);
                    CoTaskMemFree(ruta);
                }
                item->Release();
            }
        }
        dialogo->Release();
    }
    if (coOk) CoUninitialize();
    return resultado;
}
#endif

Motor& Motor::Instance()
{
    static Motor instancia;
    return instancia;
}

int Motor::Init()
{
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
        return -1;
    escalaPantalla = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
    ventana = SDL_CreateWindow("Motor", (int)(1280 * escalaPantalla), (int)(720 * escalaPantalla), SDL_WINDOW_RESIZABLE);
    if (!ventana) return -1;
    renderer = SDL_CreateRenderer(ventana, nullptr);
    if (!renderer) return -1;
    SDL_SetRenderVSync(renderer, 1);
    SDL_ShowWindow(ventana);
    ImGui::CreateContext();
    ImGui_ImplSDL3_InitForSDLRenderer(ventana, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);
    return 0;
}

void Motor::liberar()
{
    for (auto& par : niveles)
        delete par.second;
    niveles.clear();
    nivelActivo = nullptr;
}

std::string Motor::RutaNivel(const std::string& nombre) const
{
    return (std::filesystem::path(rutaProyecto) / (nombre + ".txt")).string();
}

bool Motor::CrearProyecto(const std::string& ruta)
{
    if (ruta.empty()) return false;
    CerrarProyecto();
    rutaProyecto = ruta;
    nombreProyecto = std::filesystem::path(ruta).filename().string();
    std::error_code ec;
    std::filesystem::create_directories(rutaProyecto, ec);
    std::filesystem::create_directories(std::filesystem::path(rutaProyecto) / "assets", ec);
    proyectoAbierto = std::filesystem::exists(rutaProyecto);
    if (proyectoAbierto)
        CrearEscena("Nivel1");
    return proyectoAbierto;
}

bool Motor::AbrirProyecto(const std::string& ruta)
{
    if (ruta.empty()) return false;
    if (!std::filesystem::exists(ruta)) return false;
    CerrarProyecto();
    rutaProyecto = ruta;
    nombreProyecto = std::filesystem::path(ruta).filename().string();
    proyectoAbierto = true;
    for (const auto& entrada : std::filesystem::directory_iterator(rutaProyecto))
    {
        if (!entrada.is_regular_file()) continue;
        if (entrada.path().extension() != ".txt") continue;
        if (entrada.path().filename() == "project.txt") continue;
        std::string nombreNivel = entrada.path().stem().string();
        Escena* nuevaEscena = new Escena();
        nuevaEscena->rutaBase = rutaProyecto;
        if (nuevaEscena->Cargar(entrada.path().string()))
        {
            if (nuevaEscena->nombre.empty()) nuevaEscena->nombre = nombreNivel;
            nuevaEscena->raiz->nombre = nuevaEscena->nombre;
            niveles[nombreNivel] = nuevaEscena;
            if (!nivelActivo) nivelActivo = nuevaEscena;
        }
        else
        {
            delete nuevaEscena;
        }
    }
    return true;
}

void Motor::CerrarProyecto()
{
    liberar();
    rutaProyecto.clear();
    nombreProyecto.clear();
    proyectoAbierto = false;
    modoJuego = false;

    nodoSeleccionado = nullptr;
    if (texturaPanel)
    {
        SDL_DestroyTexture(texturaPanel);
        texturaPanel = nullptr;
    }
    anchoTextura = altoTextura = 0;
    rutaArchivoSeleccionado.clear();
    nombreAccionPendiente.clear();
    accionPendiente = AccionPendiente::Ninguna;
    errorNombreDuplicado = false;
    popupEliminarNivel = false;
    nivelAEliminar.clear();
    popupEliminarArchivo = false;
    archivoAEliminar.clear();
    memset(bufferNivel, 0, sizeof(bufferNivel));
    memset(bufferNombreNodo, 0, sizeof(bufferNombreNodo));
}

void Motor::CrearEscena(const std::string& nombre)
{
    if (!proyectoAbierto || nombre.empty()) return;
    if (niveles.count(nombre)) return;
    Escena* nueva = new Escena();
    nueva->nombre = nombre;
    nueva->raiz->nombre = nombre;
    nueva->rutaBase = rutaProyecto;
    niveles[nombre] = nueva;
    nivelActivo = nueva;
    GuardarEscenaActual();
    nivelActivo->modificado = false;
}

void Motor::CambiarEscena(const std::string& nombre)
{
    auto iter = niveles.find(nombre);
    if (iter != niveles.end())
        nivelActivo = iter->second;
}

void Motor::GuardarEscenaActual()
{
    if (!proyectoAbierto || !nivelActivo) return;
    nivelActivo->Guardar(RutaNivel(nivelActivo->nombre));
    nivelActivo->modificado = false;
}

void Motor::EliminarEscena(const std::string& nombre)
{
    if (!proyectoAbierto) return;
    auto iter = niveles.find(nombre);
    if (iter == niveles.end()) return;
    if (nivelActivo == iter->second)
        nivelActivo = nullptr;
    delete iter->second;
    niveles.erase(iter);
    std::filesystem::remove(RutaNivel(nombre));
    if (nivelActivo == nullptr && !niveles.empty())
        nivelActivo = niveles.begin()->second;
}

bool Motor::CopiarAssetAlProyecto(const std::string& origen, std::string& destinoRelativo) const
{
    if (origen.empty() || rutaProyecto.empty()) return false;
    std::filesystem::path src(origen);
    if (!std::filesystem::exists(src)) return false;
    std::filesystem::path carpetaAssets = std::filesystem::path(rutaProyecto) / "assets";
    std::error_code ec;
    std::filesystem::create_directories(carpetaAssets, ec);
    std::filesystem::path dst = carpetaAssets / src.filename();
    int contador = 1;
    while (std::filesystem::exists(dst))
        dst = carpetaAssets / (src.stem().string() + "_" + std::to_string(contador++) + src.extension().string());
    std::filesystem::copy_file(src, dst, std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) return false;
    destinoRelativo = (std::filesystem::path("assets") / dst.filename()).generic_string();
    return true;
}

static void DibujarArbolNodos(Nodo* nodo, Nodo*& seleccionado, Escena* escena)
{
    ImGui::PushID((void*)nodo);
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen;
    if (seleccionado == nodo) flags |= ImGuiTreeNodeFlags_Selected;
    bool abierto = ImGui::TreeNodeEx(nodo->nombre.c_str(), flags);
    if (ImGui::IsItemClicked())
        seleccionado = nodo;
    if (ImGui::BeginPopupContextItem())
    {
        if (ImGui::MenuItem("Eliminar nodo"))
        {
            if (nodo->padre)
            {
                auto& hijos = nodo->padre->hijos;
                hijos.erase(std::remove(hijos.begin(), hijos.end(), nodo), hijos.end());
                Escena::LiberarNodo(nodo);
                seleccionado = escena->raiz;
                escena->modificado = true;
            }
        }
        ImGui::EndPopup();
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("+"))
        escena->CrearNodo(nodo, "Nodo");
    if (abierto)
    {
        for (auto* hijo : nodo->hijos)
            DibujarArbolNodos(hijo, seleccionado, escena);
        ImGui::TreePop();
    }
    ImGui::PopID();
}

static void DibujarEscenaEnTextura(SDL_Renderer* renderer, Escena* escena, SDL_Texture*& textura, int& anchoTex, int& altoTex, ImVec2 tamanoVentana, bool modoEditor)
{
    if (!escena) return;
    if (tamanoVentana.x <= 1 || tamanoVentana.y <= 1) return;
    int nuevoAncho = (int)tamanoVentana.x;
    int nuevoAlto = (int)tamanoVentana.y;
    if (!textura || anchoTex != nuevoAncho || altoTex != nuevoAlto)
    {
        if (textura) SDL_DestroyTexture(textura);
        textura = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_BGRA8888, SDL_TEXTUREACCESS_TARGET, nuevoAncho, nuevoAlto);
        anchoTex = nuevoAncho;
        altoTex = nuevoAlto;
    }
    SDL_SetRenderTarget(renderer, textura);
    SDL_SetRenderDrawColor(renderer, 48, 48, 52, 255);
    SDL_RenderClear(renderer);
    escena->Render(renderer, modoEditor);
    SDL_SetRenderTarget(renderer, nullptr);
}

static void DibujarExploradorArchivos(const std::filesystem::path& directorioActual,
    std::string& rutaSeleccionada,
    const std::map<std::string, Escena*>& niveles,
    const std::function<void(const std::string&)>& alAbrirNivel,
    const std::function<void(const std::string&)>& alEliminarArchivo,
    bool& abrirPopupEliminar, std::string& archivoAEliminar)
{
    ImGui::PushID(directorioActual.string().c_str());
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen;
    bool esRaiz = (directorioActual == std::filesystem::path());
    std::string nombreMostrar = esRaiz ? "Proyecto" : directorioActual.filename().string();
    bool abierto = ImGui::TreeNodeEx(nombreMostrar.c_str(), flags);
    if (abierto)
    {
        std::error_code ec;
        for (const auto& entrada : std::filesystem::directory_iterator(directorioActual, ec))
        {
            if (entrada.is_directory())
            {
                DibujarExploradorArchivos(entrada.path(), rutaSeleccionada, niveles, alAbrirNivel, alEliminarArchivo, abrirPopupEliminar, archivoAEliminar);
            }
            else if (entrada.is_regular_file())
            {
                std::string nombreArchivo = entrada.path().filename().string();
                std::string extension = entrada.path().extension().string();
                bool esNivel = (extension == ".txt");
                bool esAsset = (extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".bmp" || extension == ".tga");
                if (esNivel || esAsset)
                {
                    ImGui::PushID(entrada.path().string().c_str());
                    if (ImGui::Selectable(nombreArchivo.c_str(), rutaSeleccionada == entrada.path().string()))
                    {
                        rutaSeleccionada = entrada.path().string();
                        if (ImGui::IsMouseDoubleClicked(0) && esNivel)
                        {
                            alAbrirNivel(entrada.path().stem().string());
                        }
                    }
                    if (ImGui::BeginPopupContextItem())
                    {
                        if (esNivel)
                        {
                            if (ImGui::MenuItem("Abrir nivel"))
                                alAbrirNivel(entrada.path().stem().string());
                            if (ImGui::MenuItem("Eliminar nivel"))
                            {
                                archivoAEliminar = entrada.path().string();
                                abrirPopupEliminar = true;
                            }
                        }
                        ImGui::EndPopup();
                    }
                    ImGui::PopID();
                }
            }
        }
        ImGui::TreePop();
    }
    ImGui::PopID();
}

void Motor::Run()
{
    static char bufferRutaProyecto[256] = "";
    Uint64 ultimoTiempo = SDL_GetTicks();

    auto EjecutarAccionPendiente = [&]()
        {
            if (accionPendiente == AccionPendiente::CrearNivel)
            {
                CrearEscena(nombreAccionPendiente);
                nodoSeleccionado = nivelActivo ? nivelActivo->raiz : nullptr;
            }
            else if (accionPendiente == AccionPendiente::CambiarNivel)
            {
                CambiarEscena(nombreAccionPendiente);
                nodoSeleccionado = nivelActivo ? nivelActivo->raiz : nullptr;
            }
            accionPendiente = AccionPendiente::Ninguna;
            nombreAccionPendiente.clear();
        };

    auto PedirCambioNivel = [&](const std::string& nombre, AccionPendiente accion)
        {
            if (nivelActivo && nivelActivo->modificado)
            {
                nombreAccionPendiente = nombre;
                accionPendiente = accion;
                ImGui::OpenPopup("Guardar cambios?");
            }
            else
            {
                nombreAccionPendiente = nombre;
                accionPendiente = accion;
                EjecutarAccionPendiente();
            }
        };

    corriendo = true;
    while (corriendo)
    {
        Uint64 ahora = SDL_GetTicks();
        float deltaTime = (ahora - ultimoTiempo) / 1000.0f;
        if (deltaTime > 0.05f) deltaTime = 0.05f;
        ultimoTiempo = ahora;

        SDL_Event evento;
        while (SDL_PollEvent(&evento))
        {
            ImGui_ImplSDL3_ProcessEvent(&evento);
            if (evento.type == SDL_EVENT_QUIT)
                corriendo = false;
        }
        const bool* teclas = SDL_GetKeyboardState(nullptr);

        if (modoJuego && nivelActivo)
        {
            nivelActivo->Update(deltaTime, teclas);
            if (teclas[SDL_SCANCODE_ESCAPE])
                modoJuego = false;
        }

        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        if (!proyectoAbierto)
        {
            ImGui::Begin("Motor");
            ImGui::Text("No hay proyecto abierto");
            ImGui::InputText("Ruta proyecto", bufferRutaProyecto, IM_ARRAYSIZE(bufferRutaProyecto));
            if (ImGui::Button("Crear proyecto"))
            {
                if (CrearProyecto(bufferRutaProyecto))
                    bufferRutaProyecto[0] = '\0';
            }
            ImGui::SameLine();
            if (ImGui::Button("Abrir proyecto"))
            {
                if (AbrirProyecto(bufferRutaProyecto))
                    bufferRutaProyecto[0] = '\0';
            }
            ImGui::End();
        }
        else
        {
            ImGui::Begin("Motor");
            ImGui::Text("Proyecto: %s", nombreProyecto.c_str());
            if (!modoJuego)
            {
                if (ImGui::Button("Play"))
                    modoJuego = true;
                ImGui::SameLine();
                if (ImGui::Button("Nuevo nivel"))
                {
                    bufferNivel[0] = '\0';
                    errorNombreDuplicado = false;
                    ImGui::OpenPopup("Crear nivel");
                }
                ImGui::SameLine();
                if (ImGui::Button("Guardar nivel"))
                    GuardarEscenaActual();
                ImGui::SameLine();
                if (ImGui::Button("Cerrar proyecto"))
                {
                    CerrarProyecto();
                    nodoSeleccionado = nullptr;
                    rutaArchivoSeleccionado.clear();
                    if (texturaPanel)
                    {
                        SDL_DestroyTexture(texturaPanel);
                        texturaPanel = nullptr;
                        anchoTextura = altoTextura = 0;
                    }
                }
            }
            else
            {
                if (ImGui::Button("Stop"))
                    modoJuego = false;
            }

            if (ImGui::BeginPopupModal("Crear nivel", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::InputText("Nombre", bufferNivel, IM_ARRAYSIZE(bufferNivel));
                if (errorNombreDuplicado)
                    ImGui::TextColored(ImVec4(1, 0, 0, 1), "Ya existe un nivel con ese nombre");
                if (ImGui::Button("Crear"))
                {
                    std::string nuevoNombre = bufferNivel;
                    if (nuevoNombre.empty())
                        errorNombreDuplicado = false;
                    else if (niveles.find(nuevoNombre) != niveles.end())
                        errorNombreDuplicado = true;
                    else
                    {
                        errorNombreDuplicado = false;
                        PedirCambioNivel(nuevoNombre, AccionPendiente::CrearNivel);
                        ImGui::CloseCurrentPopup();
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancelar"))
                {
                    errorNombreDuplicado = false;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }

            if (ImGui::BeginPopupModal("Guardar cambios?", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::Text("El nivel actual tiene cambios sin guardar.");
                if (ImGui::Button("Guardar y continuar"))
                {
                    GuardarEscenaActual();
                    EjecutarAccionPendiente();
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("No guardar"))
                {
                    EjecutarAccionPendiente();
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancelar"))
                {
                    accionPendiente = AccionPendiente::Ninguna;
                    nombreAccionPendiente.clear();
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }

            if (popupEliminarNivel)
            {
                ImGui::OpenPopup("Eliminar nivel##confirm");
                popupEliminarNivel = false;
            }
            if (ImGui::BeginPopupModal("Eliminar nivel##confirm", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::Text("¿Eliminar nivel '%s'?", nivelAEliminar.c_str());
                if (ImGui::Button("Si"))
                {
                    EliminarEscena(nivelAEliminar);
                    nivelAEliminar.clear();
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("No"))
                {
                    nivelAEliminar.clear();
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }

            if (popupEliminarArchivo)
            {
                ImGui::OpenPopup("Eliminar archivo##confirm");
                popupEliminarArchivo = false;
            }
            if (ImGui::BeginPopupModal("Eliminar archivo##confirm", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::Text("¿Eliminar archivo '%s'?", std::filesystem::path(archivoAEliminar).filename().string().c_str());
                if (ImGui::Button("Si"))
                {
                    std::string nombreSinExt = std::filesystem::path(archivoAEliminar).stem().string();
                    if (niveles.find(nombreSinExt) != niveles.end())
                        EliminarEscena(nombreSinExt);
                    else
                        std::filesystem::remove(archivoAEliminar);
                    archivoAEliminar.clear();
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("No"))
                {
                    archivoAEliminar.clear();
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }

            if (!modoJuego)
            {
                ImGui::Columns(3, nullptr, true);

                ImGui::BeginChild("IzquierdaArriba", ImVec2(0, 260), true);
                ImGui::Text("Nivel");
                if (nivelActivo && nivelActivo->raiz)
                {
                    if (ImGui::Selectable(nivelActivo->raiz->nombre.c_str(), nodoSeleccionado == nivelActivo->raiz))
                        nodoSeleccionado = nivelActivo->raiz;
                    if (ImGui::Button("Crear hijo del nivel"))
                        nivelActivo->CrearNodo(nivelActivo->raiz, "Nodo");
                    ImGui::Separator();
                    if (ImGui::TreeNode("Arbol de nodos"))
                    {
                        for (auto* hijo : nivelActivo->raiz->hijos)
                            DibujarArbolNodos(hijo, nodoSeleccionado, nivelActivo);
                        ImGui::TreePop();
                    }
                }
                ImGui::EndChild();

                ImGui::BeginChild("IzquierdaAbajo", ImVec2(0, 0), true);
                ImGui::Text("Explorador del Proyecto");
                std::filesystem::path rutaProyectoPath = rutaProyecto;
                if (std::filesystem::exists(rutaProyectoPath))
                {
                    auto alAbrirNivel = [&](const std::string& nombre)
                        {
                            PedirCambioNivel(nombre, AccionPendiente::CambiarNivel);
                        };
                    auto alEliminarArchivo = [&](const std::string& archivo)
                        {
                            archivoAEliminar = archivo;
                            popupEliminarArchivo = true;
                        };
                    DibujarExploradorArchivos(rutaProyectoPath, rutaArchivoSeleccionado, niveles,
                        alAbrirNivel, alEliminarArchivo, popupEliminarArchivo, archivoAEliminar);
                }
                ImGui::EndChild();

                ImGui::NextColumn();

                ImGui::BeginChild("Centro", ImVec2(0, 0), true);
                if (nivelActivo)
                {
                    ImVec2 tamano = ImGui::GetContentRegionAvail();
                    DibujarEscenaEnTextura(renderer, nivelActivo, texturaPanel, anchoTextura, altoTextura, tamano, true);
                    if (texturaPanel)
                        ImGui::Image((ImTextureID)texturaPanel, tamano);
                }
                ImGui::EndChild();

                ImGui::NextColumn();

                ImGui::BeginChild("Derecha", ImVec2(0, 0), true);
                if (nivelActivo && nodoSeleccionado && nodoSeleccionado != nivelActivo->raiz)
                {
                    std::snprintf(bufferNombreNodo, sizeof(bufferNombreNodo), "%s", nodoSeleccionado->nombre.c_str());
                    if (ImGui::InputText("Nombre", bufferNombreNodo, IM_ARRAYSIZE(bufferNombreNodo)))
                    {
                        nodoSeleccionado->nombre = bufferNombreNodo;
                        nivelActivo->modificado = true;
                    }
                    if (ImGui::DragFloat2("Posicion", &nodoSeleccionado->posicion.x, 1.0f))
                        nivelActivo->modificado = true;
                    if (ImGui::DragFloat2("Escala", &nodoSeleccionado->escala.x, 0.01f))
                        nivelActivo->modificado = true;
                    if (ImGui::DragFloat("Rotacion", &nodoSeleccionado->rotacion, 1.0f))
                        nivelActivo->modificado = true;
                    if (ImGui::Checkbox("Movimiento", &nodoSeleccionado->movement))
                        nivelActivo->modificado = true;
                    if (ImGui::Checkbox("Visible", &nodoSeleccionado->visible))
                        nivelActivo->modificado = true;
                    if (ImGui::DragFloat("Speed", &nodoSeleccionado->speed, 10.0f))
                        nivelActivo->modificado = true;

                    if (ImGui::Checkbox("Collision", &nodoSeleccionado->collision.enabled))
                        nivelActivo->modificado = true;
                    if (nodoSeleccionado->collision.enabled)
                    {
                        ImGui::Indent();
                        if (ImGui::DragFloat2("Offset", &nodoSeleccionado->collision.offset.x, 1.0f))
                            nivelActivo->modificado = true;
                        if (ImGui::DragFloat2("Size", &nodoSeleccionado->collision.size.x, 1.0f, 1.0f, 1000.0f))
                            nivelActivo->modificado = true;
                        if (ImGui::DragFloat("Rotation", &nodoSeleccionado->collision.rotation, 1.0f, -360.0f, 360.0f))
                            nivelActivo->modificado = true;
                        ImGui::Unindent();
                    }

                    if (ImGui::Button("Eliminar nodo"))
                    {
                        if (nodoSeleccionado->padre)
                        {
                            auto& hijos = nodoSeleccionado->padre->hijos;
                            hijos.erase(std::remove(hijos.begin(), hijos.end(), nodoSeleccionado), hijos.end());
                            Escena::LiberarNodo(nodoSeleccionado);
                            nodoSeleccionado = nivelActivo->raiz;
                            nivelActivo->modificado = true;
                        }
                    }
                    if (ImGui::BeginCombo("Assets", nodoSeleccionado->asset.empty() ? "Ninguno" : nodoSeleccionado->asset.c_str()))
                    {
                        if (ImGui::Selectable("Ninguno"))
                        {
                            if (nodoSeleccionado->textura)
                            {
                                SDL_DestroyTexture(nodoSeleccionado->textura);
                                nodoSeleccionado->textura = nullptr;
                            }
                            nodoSeleccionado->asset = "";
                            nivelActivo->modificado = true;
                        }
                        std::filesystem::path carpetaAssets = std::filesystem::path(rutaProyecto) / "assets";
                        if (std::filesystem::exists(carpetaAssets))
                        {
                            for (const auto& entrada : std::filesystem::directory_iterator(carpetaAssets))
                            {
                                std::string nombre = entrada.path().filename().string();
                                if (ImGui::Selectable(nombre.c_str(), nodoSeleccionado->asset == "assets/" + nombre))
                                {
                                    if (nodoSeleccionado->textura)
                                    {
                                        SDL_DestroyTexture(nodoSeleccionado->textura);
                                        nodoSeleccionado->textura = nullptr;
                                    }
                                    nodoSeleccionado->asset = "assets/" + nombre;
                                    nivelActivo->modificado = true;
                                }
                            }
                        }
                        if (ImGui::Selectable("<Seleccionar imagen>"))
                        {
#ifdef _WIN32
                            std::string origen = SeleccionarImagenWindows();
#else
                            std::string origen;
#endif
                            if (!origen.empty())
                            {
                                std::string destinoRelativo;
                                if (CopiarAssetAlProyecto(origen, destinoRelativo))
                                {
                                    if (nodoSeleccionado->textura)
                                    {
                                        SDL_DestroyTexture(nodoSeleccionado->textura);
                                        nodoSeleccionado->textura = nullptr;
                                    }
                                    nodoSeleccionado->asset = destinoRelativo;
                                    nivelActivo->modificado = true;
                                }
                            }
                        }
                        ImGui::EndCombo();
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("X"))
                    {
                        if (nodoSeleccionado->textura)
                        {
                            SDL_DestroyTexture(nodoSeleccionado->textura);
                            nodoSeleccionado->textura = nullptr;
                        }
                        nodoSeleccionado->asset = "";
                        nivelActivo->modificado = true;
                    }
                }
                ImGui::EndChild();
                ImGui::Columns(1);
            }
            else
            {
                ImGui::BeginChild("GameView", ImVec2(0, 0), true);
                if (nivelActivo)
                {
                    ImVec2 tamano = ImGui::GetContentRegionAvail();
                    DibujarEscenaEnTextura(renderer, nivelActivo, texturaPanel, anchoTextura, altoTextura, tamano, false);
                    if (texturaPanel)
                        ImGui::Image((ImTextureID)texturaPanel, tamano);
                }
                ImGui::EndChild();
            }
            ImGui::End();
        }
        ImGui::Render();
        SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255);
        SDL_RenderClear(renderer);
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
        SDL_RenderPresent(renderer);
    }

    if (texturaPanel)
    {
        SDL_DestroyTexture(texturaPanel);
        texturaPanel = nullptr;
    }
}

void Motor::Exit()
{
    CerrarProyecto();
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    if (renderer) SDL_DestroyRenderer(renderer);
    if (ventana) SDL_DestroyWindow(ventana);
    SDL_Quit();
}