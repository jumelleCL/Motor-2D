#pragma once

#include <SDL3/SDL.h>

namespace Locomotora
{
	class Motor {
		private:
			float main_scale = 0.0;
			SDL_Window* window = NULL;
			SDL_WindowFlags window_flags = NULL;
			SDL_Renderer* renderer = NULL;


			bool running = false;

		public:
			//Singleton
			static Motor& Instance();
			//Funcions
			int Init();
			void Run();
			void Exit();
	};
}