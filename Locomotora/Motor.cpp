#include "Motor.h"
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"
#include <filesystem>
#include <cstring>
#include <fstream>
#include <algorithm>
#include <functional>

using namespace Locomotora;

Motor& Motor::Instance() {
    static Motor instancia;
    return instancia;
}

int Motor::Init() {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) return -1;

    escalaPantalla = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
    ventana = SDL_CreateWindow("Motor", int(1280 * escalaPantalla), int(720 * escalaPantalla), SDL_WINDOW_RESIZABLE);
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

void Motor::Exit() {
    for (auto& p : niveles) delete p.second;
    niveles.clear();
    nivelActivo = nullptr;
    if (texturaPanel) { SDL_DestroyTexture(texturaPanel); texturaPanel = nullptr; }
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    if (renderer) SDL_DestroyRenderer(renderer);
    if (ventana) SDL_DestroyWindow(ventana);
    SDL_Quit();
}

bool Motor::CrearProyecto(const std::string& ruta) {
    if (ruta.empty()) return false;
    for (auto& p : niveles) delete p.second;
    niveles.clear();
    nivelActivo = nullptr;
    if (texturaPanel) { SDL_DestroyTexture(texturaPanel); texturaPanel = nullptr; }
    anchoTextura = altoTextura = 0;
    modoJuego = false;
    nodoSeleccionado = nullptr;
    rutaArchivoSeleccionado.clear();
    nombreAccionPendiente.clear();
    accionPendiente = AccionPendiente::Ninguna;
    errorNombreDuplicado = false;
    popupEliminarArchivo = false;
    archivoAEliminar.clear();
    memset(bufferNivel, 0, sizeof(bufferNivel));
    memset(bufferNombreNodo, 0, sizeof(bufferNombreNodo));

    rutaProyecto = ruta;
    nombreProyecto = std::filesystem::path(ruta).filename().string();
    std::error_code ec;
    std::filesystem::create_directories(rutaProyecto, ec);
    std::filesystem::create_directories(std::filesystem::path(rutaProyecto) / "assets", ec);
    proyectoAbierto = std::filesystem::exists(rutaProyecto);
    if (proyectoAbierto) CrearEscena("Nivel1");
    return proyectoAbierto;
}

bool Motor::AbrirProyecto(const std::string& ruta) {
    if (ruta.empty() || !std::filesystem::exists(ruta)) return false;
    for (auto& p : niveles) delete p.second;
    niveles.clear();
    nivelActivo = nullptr;
    if (texturaPanel) { SDL_DestroyTexture(texturaPanel); texturaPanel = nullptr; }
    anchoTextura = altoTextura = 0;
    modoJuego = false;
    nodoSeleccionado = nullptr;
    rutaArchivoSeleccionado.clear();
    nombreAccionPendiente.clear();
    accionPendiente = AccionPendiente::Ninguna;
    errorNombreDuplicado = false;
    popupEliminarArchivo = false;
    archivoAEliminar.clear();
    memset(bufferNivel, 0, sizeof(bufferNivel));
    memset(bufferNombreNodo, 0, sizeof(bufferNombreNodo));

    rutaProyecto = ruta;
    nombreProyecto = std::filesystem::path(ruta).filename().string();
    proyectoAbierto = true;

    for (const auto& entry : std::filesystem::directory_iterator(rutaProyecto)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".txt") continue;
        std::string nombreNivel = entry.path().stem().string();
        Escena* escena = new Escena();
        escena->rutaBase = rutaProyecto;
        if (escena->Cargar(entry.path().string())) {
            if (escena->nombre.empty()) escena->nombre = nombreNivel;
            escena->raiz->nombre = escena->nombre;
            niveles[nombreNivel] = escena;
            if (!nivelActivo) nivelActivo = escena;
        }
        else delete escena;
    }
    return true;
}

void Motor::CrearEscena(const std::string& nombre) {
    if (!proyectoAbierto || nombre.empty() || niveles.count(nombre)) return;
    Escena* nueva = new Escena();
    nueva->nombre = nombre;
    nueva->raiz->nombre = nombre;
    nueva->rutaBase = rutaProyecto;
    niveles[nombre] = nueva;
    nivelActivo = nueva;
    GuardarEscenaActual();
    nivelActivo->modificado = false;
}

void Motor::CambiarEscena(const std::string& nombre) {
    auto it = niveles.find(nombre);
    if (it != niveles.end()) nivelActivo = it->second;
}

void Motor::GuardarEscenaActual() {
    if (proyectoAbierto && nivelActivo) {
        nivelActivo->Guardar((std::filesystem::path(rutaProyecto) / (nivelActivo->nombre + ".txt")).string());
        nivelActivo->modificado = false;
    }
}

void Motor::EliminarEscena(const std::string& nombre) {
    if (!proyectoAbierto) return;
    auto it = niveles.find(nombre);
    if (it == niveles.end()) return;
    if (nivelActivo == it->second) nivelActivo = nullptr;
    delete it->second;
    niveles.erase(it);
    std::filesystem::remove((std::filesystem::path(rutaProyecto) / (nombre + ".txt")).string());
    if (!nivelActivo && !niveles.empty()) nivelActivo = niveles.begin()->second;
}

bool Motor::CopiarAssetAlProyecto(const std::string& origen, std::string& destinoRelativo) const {
    if (origen.empty() || rutaProyecto.empty()) return false;
    std::filesystem::path src(origen);
    if (!std::filesystem::exists(src)) return false;
    std::string ext = src.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    if (ext != ".png" && ext != ".jpg" && ext != ".jpeg" && ext != ".bmp" && ext != ".tga") return false;

    std::filesystem::path dst = std::filesystem::path(rutaProyecto) / "assets" / src.filename();
    int cont = 1;
    while (std::filesystem::exists(dst))
        dst = std::filesystem::path(rutaProyecto) / "assets" / (src.stem().string() + "_" + std::to_string(cont++) + src.extension().string());
    std::filesystem::copy_file(src, dst, std::filesystem::copy_options::overwrite_existing);
    destinoRelativo = (std::filesystem::path("assets") / dst.filename()).generic_string();
    return true;
}

static void DibujarArbolNodos(Nodo* nodo, Nodo*& seleccionado, Escena* escena) {
    ImGui::PushID(nodo);
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen;
    if (seleccionado == nodo) flags |= ImGuiTreeNodeFlags_Selected;
    bool abierto = ImGui::TreeNodeEx(nodo->nombre.c_str(), flags);
    if (ImGui::IsItemClicked()) seleccionado = nodo;
    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("Eliminar nodo") && nodo->padre) {
            auto& hijos = nodo->padre->hijos;
            hijos.erase(std::remove(hijos.begin(), hijos.end(), nodo), hijos.end());
            Escena::LiberarNodo(nodo);
            seleccionado = escena->raiz;
            escena->modificado = true;
        }
        ImGui::EndPopup();
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("+")) escena->CrearNodo(nodo, "Nodo");
    if (abierto) {
        for (auto* hijo : nodo->hijos) DibujarArbolNodos(hijo, seleccionado, escena);
        ImGui::TreePop();
    }
    ImGui::PopID();
}

static void DibujarEscenaEnTextura(SDL_Renderer* r, Escena* escena, SDL_Texture*& tex, int& anchoTex, int& altoTex, ImVec2 size, bool editor) {
    if (!escena || size.x <= 1 || size.y <= 1) return;
    int nuevoAncho = int(size.x), nuevoAlto = int(size.y);
    if (!tex || anchoTex != nuevoAncho || altoTex != nuevoAlto) {
        if (tex) SDL_DestroyTexture(tex);
        tex = SDL_CreateTexture(r, SDL_PIXELFORMAT_BGRA8888, SDL_TEXTUREACCESS_TARGET, nuevoAncho, nuevoAlto);
        anchoTex = nuevoAncho; altoTex = nuevoAlto;
    }
    SDL_SetRenderTarget(r, tex);
    SDL_SetRenderDrawColor(r, 48, 48, 52, 255);
    SDL_RenderClear(r);
    escena->Render(r, editor);
    SDL_SetRenderTarget(r, nullptr);
}

static void DibujarExplorador(const std::filesystem::path& dir, std::string& seleccionada,
    const std::map<std::string, Escena*>& niveles,
    const std::function<void(const std::string&)>& abrirNivel,
    const std::function<void(const std::string&)>& eliminar,
    bool& popupEliminar, std::string& archivoEliminar) {
    ImGui::PushID(dir.string().c_str());
    bool abierto = ImGui::TreeNodeEx(dir.filename().string().c_str(), ImGuiTreeNodeFlags_DefaultOpen);
    if (abierto) {
        for (const auto& entry : std::filesystem::directory_iterator(dir)) {
            if (entry.is_directory()) {
                DibujarExplorador(entry.path(), seleccionada, niveles, abrirNivel, eliminar, popupEliminar, archivoEliminar);
            }
            else if (entry.is_regular_file()) {
                std::string ext = entry.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                bool esNivel = (ext == ".txt");
                bool esAsset = (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".tga");
                if (!esNivel && !esAsset) continue;

                ImGui::PushID(entry.path().string().c_str());

                if (esNivel) {
                    ImGui::Text("%s", entry.path().filename().string().c_str());
                    ImGui::SameLine();
                    float anchoDisponible = ImGui::GetContentRegionAvail().x;
                    float anchoBoton = 60.0f;
                    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + anchoDisponible - anchoBoton * 2 - 8.0f);
                    if (ImGui::Button("Abrir")) {
                        abrirNivel(entry.path().stem().string());
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Eliminar")) {
                        archivoEliminar = entry.path().string();
                        popupEliminar = true;
                    }
                }
                else {
                    if (ImGui::Selectable(entry.path().filename().string().c_str(), seleccionada == entry.path().string()))
                        seleccionada = entry.path().string();
                    if (ImGui::BeginPopupContextItem()) {
                        if (ImGui::MenuItem("Eliminar")) {
                            archivoEliminar = entry.path().string();
                            popupEliminar = true;
                        }
                        ImGui::EndPopup();
                    }
                }
                ImGui::PopID();
            }
        }
        ImGui::TreePop();
    }
    ImGui::PopID();
}

void Motor::Run() {
    static char rutaProyectoBuffer[256] = "";
    Uint64 lastTime = SDL_GetTicks();

    auto ejecutarAccion = [&]() {
        if (accionPendiente == AccionPendiente::CrearNivel) CrearEscena(nombreAccionPendiente);
        else if (accionPendiente == AccionPendiente::CambiarNivel) CambiarEscena(nombreAccionPendiente);
        nodoSeleccionado = nivelActivo ? nivelActivo->raiz : nullptr;
        accionPendiente = AccionPendiente::Ninguna;
        nombreAccionPendiente.clear();
        };

    auto pedirCambio = [&](const std::string& nombre, AccionPendiente accion) {
        if (nivelActivo && nivelActivo->modificado) {
            nombreAccionPendiente = nombre;
            accionPendiente = accion;
            ImGui::OpenPopup("Guardar cambios?");
        }
        else {
            nombreAccionPendiente = nombre;
            accionPendiente = accion;
            ejecutarAccion();
        }
        };

    corriendo = true;
    while (corriendo) {
        Uint64 now = SDL_GetTicks();
        float dt = (now - lastTime) / 1000.0f;
        if (dt > 0.05f) dt = 0.05f;
        lastTime = now;

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            ImGui_ImplSDL3_ProcessEvent(&e);
            if (e.type == SDL_EVENT_QUIT) corriendo = false;
        }
        const bool* teclas = SDL_GetKeyboardState(nullptr);
        if (modoJuego && nivelActivo) {
            nivelActivo->Update(dt, teclas);
            if (teclas[SDL_SCANCODE_ESCAPE]) modoJuego = false;
        }

        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        if (!proyectoAbierto) {
            ImGui::Begin("Motor");
            ImGui::Text("No hay proyecto abierto");
            ImGui::InputText("Ruta proyecto", rutaProyectoBuffer, sizeof(rutaProyectoBuffer));
            if (ImGui::Button("Crear proyecto")) {
                if (CrearProyecto(rutaProyectoBuffer)) rutaProyectoBuffer[0] = '\0';
            }
            ImGui::SameLine();
            if (ImGui::Button("Abrir proyecto")) {
                if (AbrirProyecto(rutaProyectoBuffer)) rutaProyectoBuffer[0] = '\0';
            }
            ImGui::End();
        }
        else {
            ImGui::Begin("Motor");
            ImGui::Text("Proyecto: %s", nombreProyecto.c_str());
            if (!modoJuego) {
                if (ImGui::Button("Play")) {
                    GuardarEscenaActual();
                    modoJuego = true;
                }
                ImGui::SameLine();
                if (ImGui::Button("Nuevo nivel")) {
                    bufferNivel[0] = '\0';
                    errorNombreDuplicado = false;
                    ImGui::OpenPopup("Crear nivel");
                }
                ImGui::SameLine();
                if (ImGui::Button("Guardar nivel")) GuardarEscenaActual();
            }
            else {
                if (ImGui::Button("Stop")) {
                    modoJuego = false;
                    // Recargar la escena desde el archivo guardado (estado del editor)
                    std::string rutaArchivo = (std::filesystem::path(rutaProyecto) / (nivelActivo->nombre + ".txt")).string();
                    if (nivelActivo->Cargar(rutaArchivo)) {
                        nodoSeleccionado = nivelActivo->raiz;
                        nivelActivo->modificado = false;
                    }
                }
            }

            if (ImGui::BeginPopupModal("Crear nivel", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::InputText("Nombre", bufferNivel, sizeof(bufferNivel));
                if (errorNombreDuplicado) ImGui::TextColored(ImVec4(1, 0, 0, 1), "Ya existe");
                if (ImGui::Button("Crear")) {
                    std::string nom = bufferNivel;
                    if (nom.empty()) errorNombreDuplicado = false;
                    else if (niveles.count(nom)) errorNombreDuplicado = true;
                    else { errorNombreDuplicado = false; pedirCambio(nom, AccionPendiente::CrearNivel); ImGui::CloseCurrentPopup(); }
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancelar")) ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
            }

            if (ImGui::BeginPopupModal("Guardar cambios?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::Text("El nivel tiene cambios sin guardar.");
                if (ImGui::Button("Guardar")) { GuardarEscenaActual(); ejecutarAccion(); ImGui::CloseCurrentPopup(); }
                ImGui::SameLine();
                if (ImGui::Button("No guardar")) { ejecutarAccion(); ImGui::CloseCurrentPopup(); }
                ImGui::SameLine();
                if (ImGui::Button("Cancelar")) { accionPendiente = AccionPendiente::Ninguna; nombreAccionPendiente.clear(); ImGui::CloseCurrentPopup(); }
                ImGui::EndPopup();
            }

            if (popupEliminarArchivo) {
                ImGui::OpenPopup("Eliminar archivo");
            }
            if (ImGui::BeginPopupModal("Eliminar archivo", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::Text("¿Eliminar permanentemente %s?", std::filesystem::path(archivoAEliminar).filename().string().c_str());
                if (ImGui::Button("Sí")) {
                    std::error_code ec;
                    std::filesystem::remove(archivoAEliminar, ec);
                    std::string nombreNivel = std::filesystem::path(archivoAEliminar).stem().string();
                    if (niveles.count(nombreNivel)) {
                        if (nivelActivo == niveles[nombreNivel]) nivelActivo = nullptr;
                        delete niveles[nombreNivel];
                        niveles.erase(nombreNivel);
                        if (!nivelActivo && !niveles.empty()) nivelActivo = niveles.begin()->second;
                    }
                    else {
                        std::string assetRelativo = "assets/" + std::filesystem::path(archivoAEliminar).filename().string();
                        for (auto& par : niveles) {
                            Escena* esc = par.second;
                            std::function<void(Nodo*)> limpiar = [&](Nodo* n) {
                                if (!n) return;
                                if (n->asset == assetRelativo) {
                                    n->asset.clear();
                                    if (n->textura) SDL_DestroyTexture(n->textura);
                                    n->textura = nullptr;
                                    esc->modificado = true;
                                }
                                for (auto* h : n->hijos) limpiar(h);
                                };
                            limpiar(esc->raiz);
                        }
                    }
                    popupEliminarArchivo = false;
                    archivoAEliminar.clear();
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("No")) {
                    popupEliminarArchivo = false;
                    archivoAEliminar.clear();
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }

            if (!modoJuego) {
                ImGui::Columns(3, nullptr, true);

                ImGui::BeginChild("IzqArriba", ImVec2(0, 260), true);
                ImGui::Text("Nivel");
                if (nivelActivo && nivelActivo->raiz) {
                    if (ImGui::Selectable(nivelActivo->raiz->nombre.c_str(), nodoSeleccionado == nivelActivo->raiz))
                        nodoSeleccionado = nivelActivo->raiz;
                    if (ImGui::Button("Crear hijo del nivel")) nivelActivo->CrearNodo(nivelActivo->raiz, "Nodo");
                    ImGui::Separator();
                    if (ImGui::TreeNode("Arbol de nodos")) {
                        for (auto* h : nivelActivo->raiz->hijos) DibujarArbolNodos(h, nodoSeleccionado, nivelActivo);
                        ImGui::TreePop();
                    }
                }
                ImGui::EndChild();

                ImGui::BeginChild("IzqAbajo", ImVec2(0, 0), true);
                ImGui::Text("Explorador");
                DibujarExplorador(rutaProyecto, rutaArchivoSeleccionado, niveles,
                    [&](const std::string& n) { pedirCambio(n, AccionPendiente::CambiarNivel); },
                    [&](const std::string& a) { archivoAEliminar = a; popupEliminarArchivo = true; },
                    popupEliminarArchivo, archivoAEliminar);
                ImGui::EndChild();

                ImGui::NextColumn();

                ImGui::BeginChild("Centro", ImVec2(0, 0), true);
                if (nivelActivo) {
                    ImVec2 sz = ImGui::GetContentRegionAvail();
                    DibujarEscenaEnTextura(renderer, nivelActivo, texturaPanel, anchoTextura, altoTextura, sz, true);
                    if (texturaPanel) ImGui::Image((ImTextureID)texturaPanel, sz);
                }
                ImGui::EndChild();

                ImGui::NextColumn();

                ImGui::BeginChild("Derecha", ImVec2(0, 0), true);
                if (nivelActivo && nodoSeleccionado && nodoSeleccionado != nivelActivo->raiz) {
                    snprintf(bufferNombreNodo, sizeof(bufferNombreNodo), "%s", nodoSeleccionado->nombre.c_str());
                    if (ImGui::InputText("Nombre", bufferNombreNodo, sizeof(bufferNombreNodo))) {
                        nodoSeleccionado->nombre = bufferNombreNodo;
                        nivelActivo->modificado = true;
                    }
                    ImGui::DragFloat2("Posicion", &nodoSeleccionado->posicion.x, 1.0f);
                    ImGui::DragFloat2("Escala", &nodoSeleccionado->escala.x, 0.01f);
                    ImGui::DragFloat("Rotacion", &nodoSeleccionado->rotacion, 1.0f);
                    ImGui::Checkbox("Movimiento", &nodoSeleccionado->movement);
                    ImGui::Checkbox("Visible", &nodoSeleccionado->visible);
                    ImGui::DragFloat("Speed", &nodoSeleccionado->speed, 10.0f);
                    ImGui::Checkbox("Collision", &nodoSeleccionado->collision.enabled);
                    if (nodoSeleccionado->collision.enabled) {
                        ImGui::Indent();
                        ImGui::DragFloat2("Offset", &nodoSeleccionado->collision.offset.x, 1.0f);
                        ImGui::DragFloat2("Size", &nodoSeleccionado->collision.size.x, 1.0f, 1.0f, 1000.0f);
                        ImGui::Unindent();
                    }
                    if (ImGui::Button("Eliminar nodo") && nodoSeleccionado->padre) {
                        auto& hijos = nodoSeleccionado->padre->hijos;
                        hijos.erase(std::remove(hijos.begin(), hijos.end(), nodoSeleccionado), hijos.end());
                        Escena::LiberarNodo(nodoSeleccionado);
                        nodoSeleccionado = nivelActivo->raiz;
                        nivelActivo->modificado = true;
                    }

                    if (ImGui::BeginCombo("Asset", nodoSeleccionado->asset.empty() ? "Ninguno" : nodoSeleccionado->asset.c_str())) {
                        if (ImGui::Selectable("Ninguno")) {
                            if (nodoSeleccionado->textura) SDL_DestroyTexture(nodoSeleccionado->textura);
                            nodoSeleccionado->textura = nullptr;
                            nodoSeleccionado->asset = "";
                            nivelActivo->modificado = true;
                        }
                        std::filesystem::path assets = std::filesystem::path(rutaProyecto) / "assets";
                        if (std::filesystem::exists(assets)) {
                            for (const auto& entry : std::filesystem::directory_iterator(assets)) {
                                if (entry.is_regular_file()) {
                                    std::string ext = entry.path().extension().string();
                                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                                    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".tga") {
                                        std::string nombre = entry.path().filename().string();
                                        if (ImGui::Selectable(nombre.c_str(), nodoSeleccionado->asset == "assets/" + nombre)) {
                                            if (nodoSeleccionado->textura) SDL_DestroyTexture(nodoSeleccionado->textura);
                                            nodoSeleccionado->textura = nullptr;
                                            nodoSeleccionado->asset = "assets/" + nombre;
                                            nivelActivo->modificado = true;
                                        }
                                    }
                                }
                            }
                        }
                        ImGui::EndCombo();
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("X")) {
                        if (nodoSeleccionado->textura) SDL_DestroyTexture(nodoSeleccionado->textura);
                        nodoSeleccionado->textura = nullptr;
                        nodoSeleccionado->asset = "";
                        nivelActivo->modificado = true;
                    }
                }
                ImGui::EndChild();
                ImGui::Columns(1);
            }
            else {
                ImGui::BeginChild("GameView", ImVec2(0, 0), true);
                if (nivelActivo) {
                    ImVec2 sz = ImGui::GetContentRegionAvail();
                    DibujarEscenaEnTextura(renderer, nivelActivo, texturaPanel, anchoTextura, altoTextura, sz, false);
                    if (texturaPanel) ImGui::Image((ImTextureID)texturaPanel, sz);
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

    if (texturaPanel) { SDL_DestroyTexture(texturaPanel); texturaPanel = nullptr; }
}