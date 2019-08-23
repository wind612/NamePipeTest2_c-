// Client.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"
#include <windows.h> 
#include <stdio.h>
#include <conio.h>
#include <tchar.h>
#include <string>
#include <sstream>

using namespace std;

#define BUFSIZE 512

int _tmain(int argc, TCHAR *argv[])
{
	int recv_err = 0;
	int send_err = 0;
	HANDLE hPipe;
	LPCTSTR lpvMessage = TEXT("Default message from client.");
	TCHAR  chBuf[BUFSIZE];
	BOOL   fSuccess = FALSE;
	DWORD  cbRead, cbToWrite, cbWritten, dwMode;
	LPCTSTR lpszPipename = TEXT("\\\\.\\pipe\\mynamedpipe");

	if (argc > 1)
		lpvMessage = argv[1];

	// Try to open a named pipe; wait for it, if necessary. 

	while (1)
	{
		hPipe = CreateFile(
			lpszPipename,   // pipe name 
			GENERIC_READ |  // read and write access 
			GENERIC_WRITE,
			0,              // no sharing 
			NULL,           // default security attributes
			OPEN_EXISTING,  // opens existing pipe 
			0,              // default attributes 
			NULL);          // no template file 

	  // Break if the pipe handle is valid. 

		if (hPipe != INVALID_HANDLE_VALUE)
			break;

		// Exit if an error other than ERROR_PIPE_BUSY occurs. 

		if (GetLastError() != ERROR_PIPE_BUSY)
		{
			_tprintf(TEXT("Could not open pipe. GLE=%d\n"), GetLastError());
			return -1;
		}

		// All pipe instances are busy, so wait for 20 seconds. 

		if (!WaitNamedPipe(lpszPipename, 20000))
		{
			printf("Could not open pipe: 20 second wait timed out.");
			return -1;
		}
	}

	// The pipe connected; change to message-read mode. 

	dwMode = PIPE_READMODE_MESSAGE;
	fSuccess = SetNamedPipeHandleState(
		hPipe,    // pipe handle 
		&dwMode,  // new pipe mode 
		NULL,     // don't set maximum bytes 
		NULL);    // don't set maximum time 
	if (!fSuccess)
	{
		_tprintf(TEXT("SetNamedPipeHandleState failed. GLE=%d\n"), GetLastError());
		return -1;
	}

SEND_NEXT:
	// Send a message to the pipe server. 
	static int count = 0;
	TCHAR msg[50] = TEXT("Server message:");
	_stprintf_s(msg, 50, TEXT("Client message: %d"), count++);
	cbToWrite = (lstrlen(msg) + 1) * sizeof(TCHAR);
	_tprintf(TEXT("Sending %d byte message: \"%s\"\n"), cbToWrite, msg);

	fSuccess = WriteFile(
		hPipe,                  // pipe handle 
		msg,					// message 
		cbToWrite,              // message length 
		&cbWritten,             // bytes written 
		NULL);                  // not overlapped 

	if (!fSuccess)
	{
		_tprintf(TEXT("WriteFile to pipe failed. GLE=%d\n"), GetLastError());
		return -1;
	}

	printf("\nMessage sent to server, receiving reply as follows:\n");

RECV_NEXT:
	do
	{
		// Read from the pipe. 

		fSuccess = ReadFile(
			hPipe,    // pipe handle 
			chBuf,    // buffer to receive reply 
			BUFSIZE * sizeof(TCHAR),  // size of buffer 
			&cbRead,  // number of bytes read 
			NULL);    // not overlapped 

		if (!fSuccess && GetLastError() != ERROR_MORE_DATA)
			break;

		_tprintf(TEXT("\"%s\"\n"), chBuf);
		wstringstream ss;
		static int recv_count = 0;
		ss << L"test Server message: " << recv_count++;
		wstring str1 = chBuf;
		wstring str2 = ss.str();
		if (str1 != str2)
		{
			recv_err++;
		}
	} while (!fSuccess);  // repeat loop if ERROR_MORE_DATA 

	if (!fSuccess)
	{
		_tprintf(TEXT("ReadFile from pipe failed. GLE=%d\n"), GetLastError());
		return -1;
	}



	printf("\n<End of message, press ENTER to terminate connection and exit>\n");
	//_getch();

	static int recvCount = 0;
	static int totalCount = 0;
	totalCount++;
	if (++recvCount < 5)
	{
		goto RECV_NEXT;
	}
	printf("recvCount = %d\n", recvCount);

	static int sendCount = 0;
	recvCount = 0;
	if (++sendCount < 20)
	{
		goto SEND_NEXT;
	}
	printf("sendCount = %d\n", sendCount);

	CloseHandle(hPipe);

	printf("totalCount = %d\n", totalCount);
	printf("recv_err = %d\n", recv_err);
	_getch();

	return 0;
}

