#pragma once

#include <windows.h> 

class CNamePipe
{
private:
#define CONNECTING_STATE 0 
#define READING_STATE 1 
#define WRITING_STATE 2 
#define INSTANCES 1
#define EVENTS 3
#define PIPE_TIMEOUT 5000
#define BUFSIZE 4096

	typedef struct
	{
		OVERLAPPED oOverlap[EVENTS];
		HANDLE hPipeInst;
		TCHAR chRequest[BUFSIZE];
		DWORD cbRead;
		TCHAR chReply[BUFSIZE];
		DWORD cbToWrite;
		DWORD dwState;
		BOOL fPendingIO;
	} PIPEINST, *LPPIPEINST;

	enum _EventIndex
	{
		CONNECT_EVENT,
		READING_EVENT,
		WRITING_EVENT
	};

	PIPEINST Pipe[INSTANCES];
	HANDLE hEvents[EVENTS];

private:
	void init();
	BOOL DoRead(DWORD i);
	BOOL DoWrite(DWORD i);
	void OnConnect(DWORD index);
	void OnRead(DWORD index, DWORD cbRet);
	void OnWrite(DWORD index, DWORD& cbRet);
	void DisconnectAndReconnect(DWORD);
	BOOL ConnectToNewClient(HANDLE, LPOVERLAPPED);

public:
	int recv_err = 0;
public:
	CNamePipe();
	~CNamePipe();

	void run();
	void OnRecvData(DWORD index, TCHAR* data);
	void SendData();
};

void pipe_thread(void* data);
