// test.cpp : 定义控制台应用程序的入口点。

/*
 * 测试死锁问题，需要Client::getBlock方法加上一句Sleep(20)，使写慢于读。同时需要事件为自动重置的。
 *
 */

#include "stdafx.h"

#define MYDEBUG
#include "mydebug.h"
#include "ipc.h"

volatile int sync = -1;

// Client thread
DWORD WINAPI w1(LPVOID context)
{
	while (sync < 0);
	mydebug("sync is : 0\n");

	// Create the IPC client
	osIPC::Client client("my_shared_mem_addr");

	// Declare vars
	BYTE testData[4000];
	// Continuously write data
	// Write some data
	for (int i = 0; i < 511; i++)
	{
		client.write(testData, 1);
	}

	mydebug("sync is : 1\n");
	sync = 1;

	mydebug("w1 done.\n");

	//client.write(testData, 1);




	// Success
	return 0;
};
// Client thread
DWORD WINAPI w2(LPVOID context)
{
	// Create the IPC client
	osIPC::Client client("my_shared_mem_addr");

	// Declare vars
	BYTE testData[4000];


	while (sync < 2);
	mydebug("sync is : 2\n");
	mydebug("sleeping 3 sec...\n");
	Sleep(3000);
	mydebug("sleeped 3 sec...\n");

	client.write(testData, 1);

	mydebug("sync is : 3\n");
	sync = 3;

	client.write(testData, 1);

	mydebug("w2 released.\n");


	while (sync < 4);
	mydebug("sync is : 4\n");
	mydebug("sleeping 3 sec...\n");
	Sleep(3000);
	mydebug("sleeped 3 sec...\n");

	for (int i = 0; i < 250; i++)
	{
		client.write(testData, 1);
	}

	mydebug("w2 done.\n");

	// Success
	return 0;
};
// Client thread
DWORD WINAPI w3(LPVOID context)
{

	// Create the IPC client
	osIPC::Client client("my_shared_mem_addr");

	// Declare vars
	BYTE testData[4000];

	while (sync < 3);
	mydebug("sync is : 3\n");
	mydebug("sleeping 3 sec...\n");
	Sleep(3000);
	mydebug("sleeped 3 sec...\n");

	client.write(testData, 1);

	mydebug("w3 released.\n");


	while (sync < 4);
	mydebug("sync is : 4\n");
	mydebug("sleeping 3 sec...\n");
	Sleep(3000);
	mydebug("sleeped 3 sec...\n");

	for (int i = 0; i < 250; i++)
	{
		client.write(testData, 1);
	}


	for (;;)
	{
		Sleep(2000);
		mydebug("w3 write...\n");
		client.write(testData, 1);
	}

	mydebug("w3 done.\n");

	// Success
	return 0;
};




int _tmain(int argc, _TCHAR* argv[])
{
	BYTE data[4000];
	osIPC::Server server("my_shared_mem_addr");


	HANDLE hThread = CreateThread(NULL, 0, w1, NULL, 0, NULL);
	if (!hThread) return false;

	hThread = CreateThread(NULL, 0, w2, NULL, 0, NULL);
	if (!hThread) return false;

	hThread = CreateThread(NULL, 0, w3, NULL, 0, NULL);
	if (!hThread) return false;


	
	mydebug("sync is : 0\n");
	sync = 0;

	while (sync < 1);
	mydebug("sync is : 1\n");
	mydebug("sleeping 3 sec...\n");
	Sleep(3000);
	mydebug("sleeped 3 sec...\n");

	DWORD actRead = server.read(data, 1);

	mydebug("sync is : 2\n");
	mydebug("sleeping 10 sec...\n");
	sync = 2;
	Sleep(10000);
	mydebug("sleeped 10 sec...\n");


	while (server.isInitState() == FALSE)
	{
		actRead = server.read(data, 1);
	}
	
	mydebug("sync is : 4\n");
	sync = 4;

	for (;;)
	{
		actRead = server.read(data, 1);
		mydebug("read %lu\n", actRead);
	}

	
	return 0;
}

