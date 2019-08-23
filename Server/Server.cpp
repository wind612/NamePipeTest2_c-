// Server.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"
#include <iostream>
#include <thread>
#include "NamePipe.h"
#include <string>
#include <windows.h>

#include <conio.h> // for _getch();

using namespace std;

void hello()
{
	std::cout << "Hello World!" << std::endl;
}

int main()
{
	CNamePipe pipe;
	hello();

	//std::thread t(hello);
	std::thread t(pipe_thread, &pipe);	

	int count = 0;

	_getch();
	while (++count < 100 + 1)
	{
		pipe.SendData();
		Sleep(50);
	}
	printf("recv_err = %d\n", pipe.recv_err);

	while (true)
	{
		string str;
		getline(cin, str);
		cout << "count=" << count++ << ", str=" << str << endl;
		if (str == "exit")
		{
			break;
		}
		pipe.SendData();
	}

	t.join();
}

