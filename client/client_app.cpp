// client.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "client.h"

//---------------------------------------------------------------
int main()
{
    std::cout << "I'm a client!\n";
	auto client = std::make_unique<CClient>(); //creating the object in heap for not using much stack
	if (client->open_pipe())
		client->communicate();

	return 0;
}
//---------------------------------------------------------------
//---------------------------------------------------------------
