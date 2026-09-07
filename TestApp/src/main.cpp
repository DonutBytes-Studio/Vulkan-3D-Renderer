#include "application.hpp"

int main()
{
	Application testApp;
	if (testApp.initialize())
	{
		testApp.load_data();
		testApp.run();
	}
	testApp.shutdown();

	return 0;
}