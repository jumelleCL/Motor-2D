#include "Motor.h"
#include "Vector.h"

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"
#include <stdio.h>

#ifdef __EMSCRIPTEN__
#include "../libs/emscripten/emscripten_mainloop_stub.h"
#endif

using namespace Locomotora;

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

        if( main)
        {
            ImGui::Begin("Main Window", &main);
            ImGui::Text("Welcome to Locomotora Engine!");
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