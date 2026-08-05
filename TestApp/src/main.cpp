#include "application.hpp"

int main()
{
	Application testApp;
	if (testApp.initialize())
	{
		testApp.run();
	}
	testApp.shutdown();

	return 0;
}