#pragma once
#include <SDL3/SDL.h>
#include <string>
#include <vector>
#include "Vector.h"

namespace Locomotora
{
    struct CollisionData
    {
        bool enabled = false;
        Punt offset{ 0, 0 };
        Vector size{ 100, 100 };
        float rotation = 0.0f;
    };

    struct Nodo
    {
        std::string nombre;
        Punt posicion{ 0, 0 };
        Vector escala{ 1, 1 };
        Vector size{ 100, 100 };
        bool visible = true;
        float rotacion = 0.0f;
        bool movement = false;
        float speed = 180.0f;
        std::string asset;
        SDL_Texture* textura = nullptr;
        Nodo* padre = nullptr;
        std::vector<Nodo*> hijos;

        CollisionData collision;
    };

    class Escena
    {
    public:
        std::string nombre;
        std::string rutaBase;
        Nodo* raiz = nullptr;
        bool modificado = false;

        Escena();
        ~Escena();

        Nodo* CrearNodo(Nodo* padre, const std::string& nombre);
        void netejar();
        void Update(float deltaTime, const bool* teclas);
        void Render(SDL_Renderer* renderer, bool modoEditor);
        void Guardar(const std::string& ruta) const;
        bool Cargar(const std::string& ruta);

        static void LiberarNodo(Nodo* nodo);
    };
}