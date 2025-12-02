#include "Motor.h"
#include "Vector.h"

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"
#include <stdio.h>
#include <unordered_map>
#include <sstream>



#define SDL_RenderFillRectF SDL_RenderFillRect

#ifdef __EMSCRIPTEN__
#include "../libs/emscripten/emscripten_mainloop_stub.h"
#endif

#include <string>
#include <iostream>


using namespace Locomotora;

void Motor::CrearEscena(const std::string& nom) {
    if (escenas.find(nom) == escenas.end()) {
        escenas[nom] = new Escena();
        escenas[nom]->nombre = nom;
    }
}

void Motor::CambiarEscena(const std::string& nom) {
    auto it = escenas.find(nom);
    if (it != escenas.end()) activa = it->second;
}

void Motor::NetejarEscenaActiva() {
    if (activa) activa->netejar();
}
std::unordered_set<std::string> escenasGuardadas;

void Motor::DesarEscena(const std::string& fitxer) {
    if (!activa) return;
    std::ofstream f(fitxer);
    if (!f.is_open()) return;
    std::vector<std::pair<Escena*, int>> stack;
    stack.push_back(std::make_pair(activa, 0));
    while (!stack.empty()) {
        std::pair<Escena*, int> p = stack.back();
        stack.pop_back();
        Escena* e = p.first;
        int nivel = p.second;
        for (int i = 0; i < nivel; ++i) f << "\t";
        f << e->nombre << " " << e->posicion.x << " " << e->posicion.y << " "
            << e->rotacion << " " << e->rotacion << " "
            << e->escala.x << " " << e->escala.y << "\n";
        for (auto it = e->hijos.rbegin(); it != e->hijos.rend(); ++it) {
            stack.push_back(std::make_pair(*it, nivel + 1));
        }
    }
    escenasGuardadas.insert(activa->nombre);
}

void Motor::CarregarEscena(const std::string& fitxer) {
    std::ifstream f(fitxer);
    if (!f.is_open()) return;
    std::string line;
    std::vector<Escena*> stack;
    while (std::getline(f, line)) {
        int nivel = 0;
        while (!line.empty() && line[0] == '\t') { nivel++; line.erase(0, 1); }
        std::istringstream iss(line);
        std::string nom;
        float px, py, rx, ry, sx, sy;
        iss >> nom >> px >> py >> rx >> ry >> sx >> sy;
        Escena* e = new Escena();
        e->nombre = nom;
        e->posicion = Punt{ px, py };
        e->rotacion = float{ rx };
        e->escala = Vector{ sx, sy };
        if (nivel == 0) {
            escenas[e->nombre] = e;
            escenasGuardadas.insert(e->nombre);
            stack.clear();
            stack.push_back(e);
        }
        else {
            while (stack.size() > nivel) stack.pop_back();
            e->padre = stack.back();
            stack.back()->hijos.push_back(e);
            stack.push_back(e);
        }
    }
}




int Motor::Init() {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
    {
        printf("Error: SDL_Init(): %s\n", SDL_GetError());
        return -1;
    }

    main_scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
    window_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY;

    window = SDL_CreateWindow("Dear ImGui SDL3+OpenGL3 example", (int)(1280 * main_scale), (int)(800 * main_scale), window_flags);
    if (window == nullptr)
    {
        printf("Error: SDL_CreateWindow(): %s\n", SDL_GetError());
        return -1;
    }

    renderer = SDL_CreateRenderer(window, nullptr);
    SDL_SetRenderVSync(renderer, 1);
    if (renderer == nullptr)
    {
        SDL_Log("Error: SDL_CreateRenderer(): %s\n", SDL_GetError());
        return 1;
    }

    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_ShowWindow(window);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

    ImGui::StyleColorsDark();

    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(main_scale);
    style.FontScaleDpi = main_scale;

    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);

    return 0;
}

void Motor::Run() {

    bool vector_window = false;
    bool menu = true;
	bool main = true;


    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;


    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    running = true;
    while (running) {


        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT)
                running = false;
            if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(window))
                running = false;

            // [ Inputs ]

        }


        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        if (menu)
            ImGui::ShowDemoWindow(&menu);
            {
                ImGui::Begin("MENU");
                ImGui::Checkbox("Vector y Punto test", &vector_window);
                ImGui::End();

            }


        if (vector_window)
        {
            static Punt punto1{0, 0};
            static Punt punto2{ 0, 0 };
            static Vector v;
            static bool show_ambdos = false;
            static bool show_punt = false;
            static bool show_vector = false;

            ImGui::Begin("Punto y vectores lool", &vector_window);

            ImGui::DragFloat("Punt 1 X", &punto1.x);
            ImGui::DragFloat("Punt 1 Y", &punto1.y);
            ImGui::DragFloat("Punt 2 X", &punto2.x);
            ImGui::DragFloat("Punt 2 Y", &punto2.y);


            Vector desplacament = punto1.desplacament(punto2);
            Punt suma = punto1 + v;
            Punt resta = punto1 - v;
            float magni = desplacament.magnitud();
            float magni_qua = desplacament.magnitud_quadrada();
            Vector perp = desplacament.perpendicular();
            Vector norm = desplacament.normalitzat();


            if (ImGui::Button("Mostrar suma/resta")) show_ambdos = !show_ambdos;
            if (show_ambdos)
            {
                ImGui::SeparatorText("Operaciones Punt + Vector");
                ImGui::Text("Suma -> (%.2f, %.2f)", suma.x, suma.y);
                ImGui::Text("Resta -> (%.2f, %.2f)", resta.x, resta.y);
            }

            if (ImGui::Button("Mostrar Punto")) show_punt = !show_punt;
            if (show_punt)
            {
                ImGui::SeparatorText("Desplazamiento");
                ImGui::Text("Desplazamiento: (%.2f, %.2f)", desplacament.x, desplacament.y);
                ImGui::Text("Magnitud: %.2f", magni);
                ImGui::Text("Magnitud cuadrada: %.2f", magni_qua);
            }

            if (ImGui::Button("Mostrar Vector")) show_vector = !show_vector;
            if (show_vector)
            {
                ImGui::SeparatorText("Vector info");
                ImGui::Text("Perpendicular: (%.2f, %.2f)", perp.x, perp.y);
                ImGui::Text("Normalizado: (%.2f, %.2f)", norm.x, norm.y);
            }

            ImGui::End();
            
        }

        
        if (main)
        {
            ImGui::Begin("Locomotora");

            static bool showCrearPopup = false;
            static char nuevaEscenaNombre[128] = "";

            static bool showProjectPopup = false;
            static char nuevoProyectoNombre[128] = "";
            static char nuevoProyectoRuta[128] = "";
            static std::unordered_map<Escena*, std::vector<char>> nuevosNodos;
            static Escena* seleccionado = nullptr;

            static bool iniciado = false;
            if (!iniciado) {
                std::ifstream index("escenas.txt");
                if (!index.is_open()) { std::ofstream("escenas.txt").close(); }
                else {
                    std::string nombre;
                    while (std::getline(index, nombre)) {
                        if (nombre.empty()) continue;
                        Escena* e = new Escena();
                        e->nombre = nombre;
                        std::ifstream f(nombre + ".txt");
                        if (f.is_open()) {
                            std::string linea;
                            std::vector<Escena*> stack;
                            while (std::getline(f, linea)) {
                                if (linea.empty()) continue;
                                int nivel = 0; while (nivel < (int)linea.size() && linea[nivel] == '\t') nivel++;
                                std::istringstream iss(linea.substr(nivel));
                                std::string nodeName;
                                float px, py, rx, ry, sx, sy;
                                iss >> nodeName >> px >> py >> rx >> ry >> sx >> sy;
                                Escena* nodo = new Escena();
                                nodo->nombre = nodeName;
                                nodo->posicion = Punt{ px, py };
                                nodo->rotacion = float{ rx };
                                nodo->escala = Vector{ sx, sy };
                                if (nivel == 0) { e->hijos.push_back(nodo); nodo->padre = e; }
                                else {
                                    if ((int)stack.size() >= nivel) {
                                        stack[nivel - 1]->hijos.push_back(nodo);
                                        nodo->padre = stack[nivel - 1];
                                    }
                                }
                                if (nivel < (int)stack.size()) stack.resize(nivel);
                                stack.push_back(nodo);
                            }
                        }
                        escenas[nombre] = e;
                        escenasGuardadas.insert(nombre);
                    }
                }
                iniciado = true;
            }

            if (ImGui::Button("Crear Proyecto")) { showProjectPopup = true; nuevaEscenaNombre[0] = '\0'; }
            ImGui::SameLine();
            if (ImGui::Button("Crear Escena")) { showCrearPopup = true; nuevaEscenaNombre[0] = '\0'; }
            ImGui::SameLine();
            if (ImGui::Button("Limpiar Escena Activa")) NetejarEscenaActiva();
            ImGui::SameLine();
            if (ImGui::Button("Guardar Escena") && activa) DesarEscena(activa->nombre + ".txt");

            if (showCrearPopup) { ImGui::OpenPopup("Crear Nueva Escena"); showCrearPopup = false; }
            if (ImGui::BeginPopupModal("Crear Nueva Escena", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                if (ImGui::InputText("Nombre", nuevaEscenaNombre, IM_ARRAYSIZE(nuevaEscenaNombre), ImGuiInputTextFlags_EnterReturnsTrue)
                    || ImGui::Button("Crear")) {
                    if (strlen(nuevaEscenaNombre) > 0) {
                        CrearEscena(nuevaEscenaNombre);
                        activa = escenas[nuevaEscenaNombre];
                        seleccionado = activa;
                        ImGui::CloseCurrentPopup();
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancelar")) ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
            }

            if (showProjectPopup) { ImGui::OpenPopup("Crear nuevo proyecto"); showProjectPopup = false; }
            if (ImGui::BeginPopupModal("Crear nuevo proyecto", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                if (ImGui::InputText("Nombre", nuevoProyectoNombre, IM_ARRAYSIZE(nuevoProyectoNombre), ImGuiInputTextFlags_EnterReturnsTrue)
                    || ImGui::Button("Crear")) {
                    if (strlen(nuevoProyectoNombre) > 0) {
                        //CrearProyecto(nuevoProyectoNombre);
                        activa = escenas[nuevoProyectoNombre];
                        seleccionado = activa;
                        ImGui::CloseCurrentPopup();
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancelar")) ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
            }

            ImGui::Separator();
            ImGui::Columns(3, nullptr, true);

            ImGui::BeginChild("SceneTree", ImVec2(0, 300), true);
            ImGui::Text("Escena activa y nodos:");
            if (activa) {
                struct Printer {
                    Escena*& sel;
                    std::unordered_map<Escena*, std::vector<char>>& tmpInputs;
                    Printer(Escena*& s, std::unordered_map<Escena*, std::vector<char>>& i) : sel(s), tmpInputs(i) {}
                    void run(Escena* e) {
                        ImGui::PushID((void*)e);
                        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen;
                        if (sel == e) flags |= ImGuiTreeNodeFlags_Selected;
                        bool open = ImGui::TreeNodeEx(e->nombre.c_str(), flags);
                        if (ImGui::IsItemClicked()) sel = e;
                        ImGui::SameLine();
                        if (ImGui::Button("+##crear")) {
                            if (!tmpInputs.count(e))
                                tmpInputs[e] = std::vector<char>(128, 0);
                        }
                        if (tmpInputs.count(e)) {
                            auto& buf = tmpInputs[e];
                            ImGui::InputText("##input", buf.data(), buf.size(), ImGuiInputTextFlags_EnterReturnsTrue);
                            if (ImGui::IsItemDeactivatedAfterEdit()) {
                                std::string s = buf.data();
                                if (!s.empty()) {
                                    Escena* hijo = new Escena();
                                    hijo->nombre = s;
                                    hijo->padre = e;
                                    e->hijos.push_back(hijo);
                                }
                                tmpInputs.erase(e);
                            }
                        }
                        if (open) {
                            for (auto* h : e->hijos) run(h);
                            ImGui::TreePop();
                        }
                        ImGui::PopID();
                    }
                };
                Printer printer(seleccionado, nuevosNodos);
                printer.run(activa);
            }
            ImGui::EndChild();
            ImGui::NextColumn();

            ImGui::BeginChild("Centro", ImVec2(0, 300), true);
            ImGui::Text("Vista de Escena:");
            ImVec2 panelSize = ImGui::GetContentRegionAvail();

            static SDL_Texture* panelTexture = nullptr;
            static int panelTextureW = 0;
            static int panelTextureH = 0;
            static SDL_Texture* rectTexture = nullptr;

            if (renderer) {
                if (!panelTexture || panelSize.x != panelTextureW || panelSize.y != panelTextureH) {
                    if (panelTexture) SDL_DestroyTexture(panelTexture);
                    panelTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, (int)panelSize.x, (int)panelSize.y);
                    panelTextureW = (int)panelSize.x;
                    panelTextureH = (int)panelSize.y;
                }

                if (!rectTexture) {
                    rectTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, 100, 100);
                    SDL_SetRenderTarget(renderer, rectTexture);
                    SDL_SetRenderDrawColor(renderer, 200, 100, 100, 255);
                    SDL_RenderClear(renderer);
                    SDL_SetRenderTarget(renderer, nullptr);
                }

                SDL_SetRenderTarget(renderer, panelTexture);
                SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
                SDL_RenderClear(renderer);

                if (activa) {
                    SDL_FRect dest = { activa->posicion.x, activa->posicion.y, 100 * activa->escala.x, 100 * activa->escala.y };
                    SDL_FPoint center = { dest.w * 0.5f, dest.h * 0.5f };
                    SDL_RenderTextureRotated(renderer, rectTexture, nullptr, &dest, activa->rotacion, &center, SDL_FLIP_NONE);
                }

                SDL_SetRenderTarget(renderer, nullptr);
                ImGui::Image((ImTextureID)panelTexture, panelSize);
            }

            ImGui::EndChild();




            ImGui::NextColumn();

            ImGui::BeginChild("Inspector", ImVec2(0, 300), true);
            ImGui::Text("Inspector del nodo:");
            if (seleccionado) {
                ImGui::Text("%s", seleccionado->nombre.c_str());
                ImGui::DragFloat("Pos X", &seleccionado->posicion.x, 1.0f);
                ImGui::DragFloat("Pos Y", &seleccionado->posicion.y, 1.0f);
                ImGui::DragFloat("Rotación", &seleccionado->rotacion, 1.0f);
                ImGui::DragFloat("Escala X", &seleccionado->escala.x, 0.1f);
                ImGui::DragFloat("Escala Y", &seleccionado->escala.y, 0.1f);
            }
            ImGui::EndChild();




            ImGui::Columns(1);
            ImGui::Separator();

            ImGui::BeginChild("CarpetaProyecto", ImVec2(0, 300), true);
            ImGui::Text("Escenas guardadas:");
            for (auto& nombre : escenasGuardadas) {
                if (ImGui::Selectable(nombre.c_str(), activa && activa->nombre == nombre)) {
                    CarregarEscena(nombre + ".txt");
                    CambiarEscena(nombre);
                    seleccionado = activa;
                }
            }
            ImGui::EndChild();
            ImGui::End();
        }







        // [ Rendering ]
        ImGui::Render();
        SDL_SetRenderScale(renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
        SDL_SetRenderDrawColorFloat(renderer, clear_color.x, clear_color.y, clear_color.z, clear_color.w);
        SDL_RenderClear(renderer);

        // [ Objects Rendering ]


        // [ ImGui Rendering ]
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
        SDL_RenderPresent(renderer);

        // [ Delay ]
        // [If using SDL_MAIN_USE_CALLBACKS: all code below would likely be your SDL_AppIterate() function]
        if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED)
        {
            SDL_Delay(10);
            continue;
        }
    }
}

void Motor::Exit() {

    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

Motor& Motor::Instance()
{
    static Motor instance;
    return instance;
}