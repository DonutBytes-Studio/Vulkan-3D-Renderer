#include <SDL3/SDL_main.h>
#include "application.hpp"

int main()
{
	Application testApp;
	if (testApp.initialize())
	{
		testApp.Run();
	}
	testApp.shutdown();

	return 0;
}