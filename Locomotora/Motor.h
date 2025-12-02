#pragma once

#include <SDL3/SDL.h>
#include <vector>
#include <string>
#include <map>
#include <unordered_set>
#include <fstream>
#include "Vector.h"

namespace Locomotora
{
	class Motor {
	private:
		float main_scale = 0.0;
		SDL_Window* window = NULL;
		SDL_WindowFlags window_flags = NULL;
		SDL_Renderer* renderer = NULL;


		bool running = false;
		std::unordered_set<std::string> escenasGuardadas;

		class Proyecto {
			public:
			std::string nombre;
			std::string ruta;
			Proyecto(const std::string& n, const std::string& r) : nombre(n), ruta(r) {};
		};

		class Escena {
		public:
			std::string nombre;
			std::vector<std::string> objects;
			std::vector<Escena*> hijos;
			Escena* padre = nullptr;
			Punt posicion;
			float rotacion;
			Vector escala;
			Escena() : posicion{ 0,0 }, rotacion{ 0.0 }, escala{ 1,1 } {};
			void netejar() { objects.clear(); hijos.clear(); };
		};
		std::map<std::string, Escena*> escenas;
		Escena* activa = nullptr;
	public:
		static Motor& Instance();
		int Init();
		void Run();
		void Exit();
		void CrearEscena(const std::string& nom);
		void CambiarEscena(const std::string& nom);
		void NetejarEscenaActiva();
		void DesarEscena(const std::string& fitxer);
		void CarregarEscena(const std::string& fitxer);
		/*void CrearProyecto(const std::string& nom, const std::string& rute);
		void CarregarProyecto(const std::string& rute);*/
	};
}
