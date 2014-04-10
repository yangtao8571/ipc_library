#include <windows.h>
#include <stdio.h>
#include "ipc.h"
#include "ipc_adapter.h"

static osIPC::Server server;
static osIPC::Client client;

BOOL ipcServerNew(const char *addr)
{
	server = *new osIPC::Server(addr);
	return TRUE;
}

BOOL ipcServerDestroy(void)
{
	server.~Server();
	return TRUE;
}

DWORD ipcServerRead(void *pBuff, DWORD buffSize, DWORD timeout)
{
	return server.read(pBuff, buffSize, timeout);
}


BOOL ipcClientNew(const char *addr)
{
	client = *new osIPC::Client(addr);
	return TRUE;
}

DWORD ipcClientWrite(void *pBuff, DWORD amount)
{
	return client.write(pBuff, amount);
}

DWORD ipcClientWrite(void *pBuff, DWORD amount, DWORD dwTimeout)
{
	return client.write(pBuff, amount, dwTimeout);
}

BOOL ipcClientDestroy(void)
{
	client.~Client();
	return TRUE;
}