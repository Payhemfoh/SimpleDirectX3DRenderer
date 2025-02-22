#include "Application.h"
#include <iostream>
#include <exception>

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	Application app(hInstance, nCmdShow);

	try {
		app.Run();
	}
	catch (std::exception err)
	{
		std::cerr << err.what() << std::endl;
	}

	return S_OK;
}