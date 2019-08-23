#include "pch.h"
#include "NamePipe.h"

#include <windows.h> 
#include <stdio.h>
#include <tchar.h>
#include <strsafe.h>

#include <sstream>
#include <string>


//////////////////////////////////////////////////////////////////
//https://docs.microsoft.com/zh-cn/windows/win32/ipc/named-pipe-server-using-overlapped-i-o
//Named Pipe Server Using Overlapped I/O
// 1个NamePipe对象，每个NamePipe绑定3个事件，分别是：连接，接收数据，发送数据。
//

void pipe_thread(void* data)
{
	CNamePipe* pPipe = (CNamePipe*)data;
	pPipe->run();
}

CNamePipe::CNamePipe()
{
	ZeroMemory(Pipe, sizeof(Pipe));
	ZeroMemory(hEvents, sizeof(hEvents));
}


CNamePipe::~CNamePipe()
{
}

// 初始化
void CNamePipe::init()
{
	DWORD i, j;
	LPCTSTR lpszPipename = TEXT("\\\\.\\pipe\\mynamedpipe");

	// The initial loop creates several instances of a named pipe 
	// along with an event object for each instance.  An 
	// overlapped ConnectNamedPipe operation is started for 
	// each instance. 

	for (i = 0; i < INSTANCES; i++)
	{
		for (j = 0; j < EVENTS; j++)
		{
			// Create an event object for this instance. 

			hEvents[j] = CreateEvent(
				NULL,    // default security attribute 
				TRUE,    // manual-reset event 
				TRUE,    // initial state = signaled 
				NULL);   // unnamed event object 

			if (hEvents[j] == NULL)
			{
				printf("CreateEvent failed with %d.\n", GetLastError());
				return;
			}

			ResetEvent(hEvents[j]);
			Pipe[i].oOverlap[j].hEvent = hEvents[j];
		}

		Pipe[i].hPipeInst = CreateNamedPipe(
			lpszPipename,            // pipe name 
			PIPE_ACCESS_DUPLEX |     // read/write access 
			FILE_FLAG_OVERLAPPED,    // overlapped mode 
			PIPE_TYPE_MESSAGE |      // message-type pipe 
			PIPE_READMODE_MESSAGE |  // message-read mode 
			PIPE_WAIT,               // blocking mode 
			INSTANCES,               // number of instances 
			BUFSIZE * sizeof(TCHAR),   // output buffer size 
			BUFSIZE * sizeof(TCHAR),   // input buffer size 
			PIPE_TIMEOUT,            // client time-out 
			NULL);                   // default security attributes 

		if (Pipe[i].hPipeInst == INVALID_HANDLE_VALUE)
		{
			printf("CreateNamedPipe failed with %d.\n", GetLastError());
			return;
		}

		// Call the subroutine to connect to the new client

		Pipe[i].fPendingIO = ConnectToNewClient(
			Pipe[i].hPipeInst,
			&Pipe[i].oOverlap[CONNECT_EVENT]);

		Pipe[i].dwState = Pipe[i].fPendingIO ?
			CONNECTING_STATE : // still connecting 
			READING_STATE;     // ready to read 
	}
}

// 接收
BOOL CNamePipe::DoRead(DWORD i)
{
	BOOL fSuccess;
	DWORD dwErr;

	printf("DoRead()\n");
	StringCchCopy(Pipe[i].chRequest, BUFSIZE, TEXT(""));
	Pipe[i].cbRead = 0;
	fSuccess = ReadFile(
		Pipe[i].hPipeInst,
		Pipe[i].chRequest,
		BUFSIZE * sizeof(TCHAR),
		&Pipe[i].cbRead,
		&Pipe[i].oOverlap[READING_EVENT]);

	// The read operation completed successfully. 

	if (fSuccess && Pipe[i].cbRead != 0)
	{
		OnRecvData(i, Pipe[i].chRequest);
		_tprintf(TEXT("################\n"));
		//_tprintf(TEXT("[%d] %s\n"), Pipe[i].hPipeInst, Pipe[i].chRequest);

		Pipe[i].fPendingIO = FALSE;
		Pipe[i].dwState = WRITING_STATE;

		return TRUE;
	}

	// The read operation is still pending. 

	dwErr = GetLastError();
	if (!fSuccess && (dwErr == ERROR_IO_PENDING))
	{
		printf("Pending\n");
		Pipe[i].fPendingIO = TRUE;

		return FALSE;;
	}

	// An error occurred; disconnect from the client. 

	printf("FAIL\n");
	DisconnectAndReconnect(i);
	return FALSE;
}

// 发送
BOOL CNamePipe::DoWrite(DWORD i)
{
	BOOL fSuccess;
	DWORD dwErr;
	DWORD cbRet;

	printf("DoWrite()\n");

	fSuccess = WriteFile(
		Pipe[i].hPipeInst,
		Pipe[i].chReply,
		Pipe[i].cbToWrite,
		&cbRet,
		&Pipe[i].oOverlap[WRITING_EVENT]);

	// The write operation completed successfully. 
	printf("cbRet=%d\n", cbRet);

	if (fSuccess && cbRet == Pipe[i].cbToWrite)
	{
		printf("OK\n");
		Pipe[i].fPendingIO = FALSE;
		Pipe[i].dwState = READING_STATE;
		return TRUE;
	}

	// The write operation is still pending. 

	dwErr = GetLastError();
	if (!fSuccess && (dwErr == ERROR_IO_PENDING))
	{
		printf("Pending\n");
		Pipe[i].fPendingIO = TRUE;
		return TRUE;
	}

	// An error occurred; disconnect from the client. 

	printf("FAIL\n");
	DisconnectAndReconnect(i);
	return FALSE;
}

// 连接完成
void CNamePipe::OnConnect(DWORD index)
{
	printf("OnConnect()\n");
	ResetEvent(Pipe[index].oOverlap[CONNECT_EVENT].hEvent);
	while (DoRead(index)) {};
}

// 读完成
void CNamePipe::OnRead(DWORD index, DWORD cbRet)
{
	printf("OnRead(), cbRet=%d\n", cbRet);
	Pipe[index].cbRead = cbRet;
	OnRecvData(index, Pipe[index].chRequest);
	_tprintf(TEXT(".............\n"));

	//ResetEvent(Pipe[index].oOverlap[READING_EVENT].hEvent);
	DoRead(index);
}

// 写完成
void CNamePipe::OnWrite(DWORD index, DWORD& cbRet)
{
	printf("OnWrite(), cbRet=%d\n", cbRet);
	ResetEvent(Pipe[index].oOverlap[WRITING_EVENT].hEvent);
	//DoWrite(index);
}

// 主循环
void CNamePipe::run()
{
	DWORD i, j, dwWait, cbRet;
	BOOL fSuccess;
	i = 0;

	init();

	while (1)
	{
		// Wait for the event object to be signaled, indicating 
		// completion of an overlapped read, write, or 
		// connect operation. 

		dwWait = WaitForMultipleObjects(
			EVENTS,       // number of event objects 
			hEvents,      // array of event objects 
			FALSE,        // does not wait for all 
			INFINITE);    // waits indefinitely 

	  // dwWait shows which pipe completed the operation. 

		j = dwWait - WAIT_OBJECT_0;  // determines which pipe 
		if (j < 0 || j >(EVENTS - 1))
		{
			printf("Index out of range.\n");
			return;
		}
		printf("WaitForMultipleObjects return, j=%d\n", j);

		fSuccess = GetOverlappedResult(
			Pipe[i].hPipeInst, // handle to pipe 
			&Pipe[i].oOverlap[j], // OVERLAPPED structure 
			&cbRet,            // bytes transferred 
			FALSE);            // do not wait 

		switch (j)
		{
		case CONNECT_EVENT:
			printf("CONNECT_EVENT\n");
			if (!fSuccess)
			{
				printf("Error %d.\n", GetLastError());
				return;
			}
			Pipe[i].dwState = READING_STATE;
			OnConnect(i);
			break;
		case READING_EVENT:
			printf("READING_EVENT cbRet=%d\n", cbRet);
			if (!fSuccess || cbRet == 0)
			{
				DisconnectAndReconnect(i);
				continue;
			}

			Pipe[i].cbRead = cbRet;
			Pipe[i].dwState = READING_STATE;
			OnRead(i, cbRet);
			break;
		case WRITING_EVENT:
			printf("WRITING_EVENT, cbRet=%d\n", cbRet);
			if (!fSuccess || cbRet != Pipe[i].cbToWrite)
			{
				DisconnectAndReconnect(i);
				continue;
			}
			printf("WRITING_STATE => READING_STATE\n");
			Pipe[i].dwState = READING_STATE;
			OnWrite(i, cbRet);
			break;
		default:
			printf("Invalid pipe state.\n");
			return;
		}
	}
}

// DisconnectAndReconnect(DWORD) 
// This function is called when an error occurs or when the client 
// closes its handle to the pipe. Disconnect from this client, then 
// call ConnectNamedPipe to wait for another client to connect. 

void CNamePipe::DisconnectAndReconnect(DWORD i)
{
	// Disconnect the pipe instance. 
	printf("DisconnectAndReconnect, i=%d\n", i);

	if (!DisconnectNamedPipe(Pipe[i].hPipeInst))
	{
		printf("DisconnectNamedPipe failed with %d.\n", GetLastError());
	}

	// Call a subroutine to connect to the new client. 
	ResetEvent(Pipe[i].oOverlap[READING_EVENT].hEvent);
	Pipe[i].cbToWrite = 0;
	Pipe[i].fPendingIO = ConnectToNewClient(
		Pipe[i].hPipeInst,
		&Pipe[i].oOverlap[CONNECT_EVENT]);

	Pipe[i].dwState = Pipe[i].fPendingIO ?
		CONNECTING_STATE : // still connecting 
		READING_STATE;     // ready to read 
}

// ConnectToNewClient(HANDLE, LPOVERLAPPED) 
// This function is called to start an overlapped connect operation. 
// It returns TRUE if an operation is pending or FALSE if the 
// connection has been completed. 

BOOL CNamePipe::ConnectToNewClient(HANDLE hPipe, LPOVERLAPPED lpo)
{
	BOOL fConnected, fPendingIO = FALSE;

	printf("ConnectToNewClient\n");
	// Start an overlapped connection for this pipe instance. 
	fConnected = ConnectNamedPipe(hPipe, lpo);

	// Overlapped ConnectNamedPipe should return zero. 
	if (fConnected)
	{
		printf("ConnectNamedPipe failed with %d.\n", GetLastError());
		return 0;
	}

	switch (GetLastError())
	{
		// The overlapped connection in progress. 
	case ERROR_IO_PENDING:
		fPendingIO = TRUE;
		break;

		// Client is already connected, so signal an event. 

	case ERROR_PIPE_CONNECTED:
		if (SetEvent(lpo->hEvent))
			break;

		// If an error occurs during the connect operation... 
	default:
	{
		printf("ConnectNamedPipe failed with %d.\n", GetLastError());
		return 0;
	}
	}

	return fPendingIO;
}

void CNamePipe::OnRecvData(DWORD index, TCHAR* data)
{
	_tprintf(TEXT("[%d] %s\n"), index, data);

	std::wstringstream ss;
	static int recv_count = 0;
	ss << L"Client message: " << recv_count++;
	std::wstring str1 = data;
	std::wstring str2 = ss.str();
	if (str1 != str2)
	{
		recv_err++;
	}
}

void CNamePipe::SendData()
{
	static int count = 0;
	DWORD i;
	BOOL fSuccess = FALSE;

	TCHAR msg[50] = TEXT("test Server message:");
	_stprintf_s(msg, 50, TEXT("test Server message: %d"), count++);

	i = 0;
	if (Pipe[i].dwState == CONNECTING_STATE)
	{
		printf("ERROR: Name pipe not connect!\n");
		return;
	}

	StringCchCopy(Pipe[i].chReply, BUFSIZE, msg);
	Pipe[i].cbToWrite = (lstrlen(Pipe[i].chReply) + 1) * sizeof(TCHAR);
	Pipe[i].dwState = WRITING_STATE;

	DoWrite(i);
}
