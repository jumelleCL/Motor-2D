#pragma once
#include <SDL3/SDL.h>
#include <map>
#include <string>
#include "Escena.h"

namespace Locomotora
{
    class Motor
    {
    private:
        SDL_Window* ventana = nullptr;
        SDL_Renderer* renderer = nullptr;
        bool corriendo = false;
        bool modoJuego = false;
        bool proyectoAbierto = false;

        std::string rutaProyecto;
        std::string nombreProyecto;
        float escalaPantalla = 1.0f;

        std::map<std::string, Escena*> niveles;
        Escena* nivelActivo = nullptr;

        Nodo* nodoSeleccionado = nullptr;
        SDL_Texture* texturaPanel = nullptr;
        int anchoTextura = 0, altoTextura = 0;
        std::string rutaArchivoSeleccionado;
        std::string nombreAccionPendiente;
        enum class AccionPendiente { Ninguna, CrearNivel, CambiarNivel };
        AccionPendiente accionPendiente = AccionPendiente::Ninguna;
        bool errorNombreDuplicado = false;
        bool popupEliminarNivel = false;
        std::string nivelAEliminar;
        bool popupEliminarArchivo = false;
        std::string archivoAEliminar;
        char bufferNivel[128] = "";
        char bufferNombreNodo[128] = "";

        bool CopiarAssetAlProyecto(const std::string& origen, std::string& destinoRelativo) const;
        void EliminarEscena(const std::string& nombre);

    public:
        static Motor& Instance();

        int Init();
        void Run();
        void Exit();

        bool CrearProyecto(const std::string& ruta);
        bool AbrirProyecto(const std::string& ruta);
        void CrearEscena(const std::string& nombre);
        void CambiarEscena(const std::string& nombre);
        void GuardarEscenaActual();
    };
}