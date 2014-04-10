#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers
#include <stdio.h>
#include <stdlib.h>
#include <tchar.h>
#include <windows.h>
#include <memory.h>
#include "helper.h"
#include "ipc.h"
#include <iostream>
using namespace std;

//#define MYDEBUG
#include "mydebug.h"

static void printError()
{

		TCHAR szBuf[128];
		LPVOID lpMsgBuf;
		DWORD dw = GetLastError();
		FormatMessage (
			FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
			NULL,
			dw,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPTSTR) &lpMsgBuf,
			0, 
			NULL );

		wsprintf(szBuf, _T(" error code=%d: %s\n"), dw, lpMsgBuf);
		cout << szBuf << endl;
		LocalFree(lpMsgBuf);
}
// ===================================================================
//								 server 
// ===================================================================

osIPC::Server::Server(const char *addr)
{
	// Set default params
	m_hMapFile = 0;
	m_hSignal = 0;
	m_hAvail = 0;
	m_pBuf = NULL;
	m_sAddr = NULL;
	m_stat.nTryGetBlock = m_stat.nGetBlock = m_stat.nRetBlock = m_stat.nReleaseSemaphore = m_stat.nWait = m_stat.nWokeUpSuccess = 0;

	// create the server
	create(addr);
};

osIPC::Server::~Server(void)
{
	// Free memory
	if (m_sAddr) {
		free(m_sAddr);
		m_sAddr = NULL;
	}

	// Close the server
	close();
};

//TCHAR szName[]=TEXT("aaaaaMyFileMappingObject");
void osIPC::Server::create(const char *addr)
{
	// Determine the name of the memory
	DWORD ProcessID = GetCurrentProcessId();
	DWORD ThreadID = GetCurrentThreadId();
	DWORD ServerID = osIPC::GetID();

	m_sAddr = (char*)malloc(IPC_MAX_ADDR);
	if (!m_sAddr) return;
	ZeroMemory(m_sAddr, IPC_MAX_ADDR);
	if (addr)	strcpy_s(m_sAddr, IPC_MAX_ADDR, addr);
	else		sprintf_s(m_sAddr, IPC_MAX_ADDR, "IPC_%04u_%04u_%04u", ProcessID, ThreadID, ServerID);
	

	char *m_sEvtAvail = (char*)malloc(IPC_MAX_ADDR);
	if (!m_sEvtAvail) { printf("server_create: failed: %01d\n", __LINE__); return; }
	ZeroMemory(m_sEvtAvail, IPC_MAX_ADDR);
	sprintf_s(m_sEvtAvail, IPC_MAX_ADDR, "%s_evt_avail", m_sAddr);

	char *m_sEvtFilled = (char*)malloc(IPC_MAX_ADDR);
	if (!m_sEvtFilled) { free(m_sEvtAvail); printf("server_create: failed: %01d\n", __LINE__); return; }
	ZeroMemory(m_sEvtFilled, IPC_MAX_ADDR);
	sprintf_s(m_sEvtFilled, IPC_MAX_ADDR, "%s_evt_filled", m_sAddr);

	char *m_sMemName = (char*)malloc(IPC_MAX_ADDR);
	if (!m_sMemName)
	{ 
		free(m_sEvtAvail); 
		free(m_sEvtFilled); 
		printf("server_create: failed: %01d\n", __LINE__); 
		return; 
	}
	ZeroMemory(m_sMemName, IPC_MAX_ADDR);
	sprintf_s(m_sMemName, IPC_MAX_ADDR, "%s_mem", m_sAddr);

	wchar_t *memName = charToWchar(m_sMemName);
	if (!memName)
	{
		free(m_sEvtAvail); 
		free(m_sEvtFilled); 
		free(m_sMemName);
		printf("server_create: failed: %01d\n", __LINE__); 
		return; 
	}

	// Create the events
	wchar_t *sEvtFilled = charToWchar(m_sEvtFilled);
	if (!sEvtFilled)
	{
		free(m_sEvtAvail); 
		free(m_sEvtFilled); 
		free(m_sMemName);
		printf("client_create: failed: %01d\n", __LINE__); 
		return; 
	}
	m_hSignal = CreateSemaphore(NULL, 0, IPC_BLOCK_COUNT - 1, sEvtFilled);
	if (m_hSignal == NULL || m_hSignal == INVALID_HANDLE_VALUE) { free(m_sEvtAvail); free(m_sEvtFilled); free(m_sMemName); return; }


	wchar_t *sEvtAvail = charToWchar(m_sEvtAvail);
	if (!sEvtAvail)
	{
		free(m_sEvtAvail); 
		free(m_sEvtFilled); 
		free(m_sMemName);
		printf("client_create: failed: %01d\n", __LINE__); 
		return; 
	}
	m_hAvail = CreateSemaphore(NULL, IPC_BLOCK_COUNT - 1, IPC_BLOCK_COUNT - 1, sEvtAvail);
	if (m_hAvail == NULL || m_hSignal == INVALID_HANDLE_VALUE) { free(m_sEvtAvail); free(m_sEvtFilled); free(m_sMemName); return; }


	// Create the file mapping
	m_hMapFile = CreateFileMapping(	INVALID_HANDLE_VALUE,
									NULL,
									PAGE_READWRITE,
									0,
									sizeof(MemBuff),
									memName);
	if (m_hMapFile == NULL || m_hMapFile == INVALID_HANDLE_VALUE)  { free(m_sEvtAvail); free(m_sEvtFilled); free(m_sMemName); printf("server_create: failed: %01d\n", __LINE__); return; }
 
	// Map to the file
	m_pBuf = (MemBuff*)MapViewOfFile(	m_hMapFile,				// handle to map object
										FILE_MAP_ALL_ACCESS,	// read/write permission
										0,
										0,
										sizeof(MemBuff)); 
	if (m_pBuf == NULL) 
	{ 
		free(m_sEvtAvail); 
		free(m_sEvtFilled); 
		free(m_sMemName); 
		printf("server_create: failed: %01d\n", __LINE__); 
		return; 
	}

	// Clear the buffer
	ZeroMemory(m_pBuf, sizeof(MemBuff));
	
	// Create a circular linked list
	int N = 1;
	m_pBuf->m_Blocks[0].Next = 1;
	m_pBuf->m_Blocks[0].Prev = (IPC_BLOCK_COUNT-1);
	for (;N < IPC_BLOCK_COUNT-1; N++)
	{
		// Add this block into the available list
		m_pBuf->m_Blocks[N].Next = (N+1);
		m_pBuf->m_Blocks[N].Prev = (N-1);
	}
	m_pBuf->m_Blocks[N].Next = 0;
	m_pBuf->m_Blocks[N].Prev = (IPC_BLOCK_COUNT-2);

	// Initialize the pointers
	m_pBuf->m_ReadEnd = 0;
	m_pBuf->m_ReadStart = 0;
	m_pBuf->m_WriteEnd = 0;
	m_pBuf->m_WriteStart = 0;
		
	// Release memory
	free(m_sEvtAvail);
	free(m_sEvtFilled);
	free(m_sMemName);
	free(memName);
};

void osIPC::Server::close(void)
{
	// Close the event
	if (m_hSignal) {
		HANDLE handle = m_hSignal;
		m_hSignal = NULL;
		CloseHandle(handle);
	}

	// Close the event
	if (m_hAvail) {
		HANDLE handle = m_hAvail;
		m_hAvail = NULL;
		CloseHandle(handle);
	}

	// Unmap the memory
	if (m_pBuf) {
		MemBuff *pBuff = m_pBuf;
		m_pBuf = NULL;
		UnmapViewOfFile(pBuff);
	}

	// Close the file handle
	if (m_hMapFile) {
		HANDLE handle = m_hMapFile;
		m_hMapFile = NULL;
		CloseHandle(handle);
	}
};

osIPC::Block* osIPC::Server::getBlock(DWORD dwTimeout)
{
	// No room is available, wait for room to become available
	mydebug("waiting m_hSignal...\n");
	m_stat.nWait++;
	switch (WaitForSingleObject(m_hSignal, dwTimeout))
	{
	case WAIT_OBJECT_0:
		mydebug("wake up via m_hSignal\n");
		m_stat.nWokeUpSuccess++;
		for (;;)
		{
			m_stat.nTryGetBlock++;
			// Check if there is room to expand the read start cursor
			LONG blockIndex = m_pBuf->m_ReadStart;
			Block *pBlock = m_pBuf->m_Blocks + blockIndex;
			
			// Make sure the operation is atomic
			if (InterlockedCompareExchange(&m_pBuf->m_ReadStart, pBlock->Next, blockIndex) == blockIndex)
				return pBlock;

		}

	case WAIT_TIMEOUT:
		return NULL;

	case WAIT_FAILED:
		return (osIPC::Block *)2; // explicit errer.
	}
	
};

void osIPC::Server::retBlock(osIPC::Block* pBlock)
{
	// Set the done flag for this block
	pBlock->doneRead = 1;

	// Move the read pointer as far forward as we can
	for (;;)
	{
		// Try and get the right to move the poitner
		DWORD blockIndex = m_pBuf->m_ReadEnd;
		pBlock = m_pBuf->m_Blocks + blockIndex;
		if (InterlockedCompareExchange(&pBlock->doneRead, 0, 1) != 1)
		{
			// If we get here then another thread has already moved the pointer
			// for us or we have reached as far as we can possible move the pointer
			return;
		}

		// Move the pointer one forward (interlock protected)
		InterlockedCompareExchange(&m_pBuf->m_ReadEnd, pBlock->Next, blockIndex);
		
		// Signal availability of more data but only if a thread is waiting
		if (!ReleaseSemaphore(m_hAvail, 1, NULL))
        {
			printf("ReleaseSemaphore error: %d\n", GetLastError());
			printError();
		}
		m_stat.nReleaseSemaphore++;
	}
};

DWORD osIPC::Server::read(void *pBuff, DWORD buffSize, DWORD dwTimeout)
{
	// Grab a block
	Block *pBlock = getBlock(dwTimeout);
	if (!pBlock) return 0xffffffff; // time out
	mydebug("getted block\n");
	m_stat.nGetBlock++;

	// Copy the data
	DWORD dwAmount = min(pBlock->Amount, buffSize);
	memcpy(pBuff, pBlock->Data, dwAmount);

	// Return the block
	retBlock(pBlock);
	m_stat.nRetBlock++;
	mydebug("returned block\n");

	mydebug("read done. returning %lu\n", dwAmount);

	// Success
	return dwAmount;
};

void osIPC::Server::recoveryFromClientDeath(void)
{
	LONG iWriteEnd = m_pBuf->m_WriteEnd;
	LONG iWriteStart = m_pBuf->m_WriteStart;

	LONG i, j;
	Block *pWriteStart = m_pBuf->m_Blocks + iWriteStart;
	Block *pZero = NULL;
	Block *p = m_pBuf->m_Blocks + iWriteEnd;
	Block *pFindZero = p;
	for (; p != pWriteStart; )
	{
		if (p->writing == 0)
		{
			for (; pFindZero != p; )
			{
				if (pFindZero->writing == 1)
				{
					pZero = pFindZero;
					break;
				}

				j = pFindZero->Next;
				pFindZero = m_pBuf->m_Blocks + j;
			}

			if (pZero)
			{
				memcpy(pZero->Data, p->Data, sizeof(p->Data));
				pZero->writing = 0;
				pZero->Amount = p->Amount;
				p->writing = 1;
				p->Amount = 0;
				pZero = NULL;
			}
		}
		
		i = p->Next;
		p = m_pBuf->m_Blocks + i;
	}

	// let write start and write end pointer point to the correct place.
	LONG finalWrite;
	i = iWriteEnd;
	for (p = m_pBuf->m_Blocks + iWriteEnd; p->writing != 1; )
	{
		i = p->Next;
		p = m_pBuf->m_Blocks + i;
	}
	finalWrite = i;

	// doneWrite and writing set 0.
	for (p = m_pBuf->m_Blocks + iWriteEnd; p != pWriteStart; )
	{
		p->doneWrite = 0;
		p->writing = 0;

		i = p->Next;
		p = m_pBuf->m_Blocks + i;
	}

	m_pBuf->m_WriteStart = m_pBuf->m_WriteEnd = finalWrite;
}

BOOL osIPC::Server::isInitState(void)
{
	mydebug("m_ReadStart  is : %lu\n", m_pBuf->m_ReadStart);
	mydebug("m_ReadEnd    is : %lu\n", m_pBuf->m_ReadEnd);
	mydebug("m_WriteStart is : %lu\n", m_pBuf->m_WriteStart);
	mydebug("m_WriteEnd   is : %lu\n", m_pBuf->m_WriteEnd);
	return m_pBuf->m_ReadEnd == m_pBuf->m_ReadStart
		&& m_pBuf->m_WriteEnd == m_pBuf->m_WriteStart
		&& m_pBuf->m_ReadEnd == m_pBuf->m_WriteEnd;
}

void osIPC::Server::printStatus(void)
{
	printf("m_ReadStart  is : %lu\n", m_pBuf->m_ReadStart);
	printf("m_ReadEnd    is : %lu\n", m_pBuf->m_ReadEnd);
	printf("m_WriteStart is : %lu\n", m_pBuf->m_WriteStart);
	printf("m_WriteEnd   is : %lu\n", m_pBuf->m_WriteEnd);
	printf("nTryGetBlock      is : %lu\n", m_stat.nTryGetBlock);
	printf("nGetBlock         is : %lu\n", m_stat.nGetBlock);
	printf("nRetBlock         is : %lu\n", m_stat.nRetBlock);
	printf("nWait             is : %lu\n", m_stat.nWait);
	printf("nWokeUpSuccess    is : %lu\n", m_stat.nWokeUpSuccess);
	printf("nReleaseSemaphore is : %lu\n", m_stat.nReleaseSemaphore);
}


// ===================================================================
//								 client 
// ===================================================================


osIPC::Client::Client(void)
{
	// Set default params
	m_hMapFile = 0;
	m_hSignal = 0;
	m_hAvail = 0;
	m_pBuf = NULL;
	m_stat.nTryGetBlock = m_stat.nGetBlock = m_stat.nPostBlock = m_stat.nReleaseSemaphore = m_stat.nWait = m_stat.nWokeUpSuccess = 0;
};

osIPC::Client::Client(const char *connectAddr)
{
	// Set default params
	m_hMapFile = 0;
	m_hSignal = 0;
	m_hAvail = 0;
	m_pBuf = NULL;
	m_stat.nTryGetBlock = m_stat.nGetBlock = m_stat.nPostBlock = m_stat.nReleaseSemaphore = m_stat.nWait = m_stat.nWokeUpSuccess = 0;

	// Determine the name of the memory
	m_sAddr = (char*)malloc(IPC_MAX_ADDR);
	if (!m_sAddr) return;
	ZeroMemory(m_sAddr, IPC_MAX_ADDR);
	strcpy_s(m_sAddr, IPC_MAX_ADDR, connectAddr);
	
	char *m_sEvtAvail = (char*)malloc(IPC_MAX_ADDR);
	if (!m_sEvtAvail) return;
	ZeroMemory(m_sEvtAvail, IPC_MAX_ADDR);
	sprintf_s(m_sEvtAvail, IPC_MAX_ADDR, "%s_evt_avail", m_sAddr);
	
	char *m_sEvtFilled = (char*)malloc(IPC_MAX_ADDR);
	if (!m_sEvtFilled) { free(m_sEvtAvail); return; }
	ZeroMemory(m_sEvtFilled, IPC_MAX_ADDR);
	sprintf_s(m_sEvtFilled, IPC_MAX_ADDR, "%s_evt_filled", m_sAddr);
	
	char *m_sMemName = (char*)malloc(IPC_MAX_ADDR);
	if (!m_sMemName) 
	{ 
		free(m_sEvtAvail); 
		free(m_sEvtFilled); 
		return; 
	}
	ZeroMemory(m_sMemName, IPC_MAX_ADDR);
	sprintf_s(m_sMemName, IPC_MAX_ADDR, "%s_mem", m_sAddr);

	wchar_t *memName = charToWchar(m_sMemName);
	if (!memName)
	{
		free(m_sEvtAvail); 
		free(m_sEvtFilled); 
		free(m_sMemName);
		printf("client_create: failed: %01d\n", __LINE__); 
		return; 
	}

	// Create the events
	wchar_t *sEvtFilled = charToWchar(m_sEvtFilled);
	if (!sEvtFilled)
	{
		free(m_sEvtAvail); 
		free(m_sEvtFilled); 
		free(m_sMemName);
		printf("client_create: failed: %01d\n", __LINE__); 
		return; 
	}
	m_hSignal = CreateSemaphore(NULL, 0, IPC_BLOCK_COUNT - 1, sEvtFilled);
	if (m_hSignal == NULL || m_hSignal == INVALID_HANDLE_VALUE) { free(m_sEvtAvail); free(m_sEvtFilled); free(m_sMemName); return; }


	wchar_t *sEvtAvail = charToWchar(m_sEvtAvail);
	if (!sEvtAvail)
	{
		free(m_sEvtAvail); 
		free(m_sEvtFilled); 
		free(m_sMemName);
		printf("client_create: failed: %01d\n", __LINE__); 
		return; 
	}
	m_hAvail = CreateSemaphore(NULL, IPC_BLOCK_COUNT - 1, IPC_BLOCK_COUNT - 1, sEvtAvail);
	if (m_hAvail == NULL || m_hSignal == INVALID_HANDLE_VALUE) { free(m_sEvtAvail); free(m_sEvtFilled); free(m_sMemName); return; }

	// Open the shared file
	m_hMapFile = OpenFileMapping(	FILE_MAP_ALL_ACCESS,	// read/write access
									FALSE,					// do not inherit the name
									memName);	// name of mapping object
	if (m_hMapFile == NULL || m_hMapFile == INVALID_HANDLE_VALUE)  
	{ 
		free(m_sEvtAvail); 
		free(m_sEvtFilled); 
		free(m_sMemName); 
		printf("client open file mapping: failed: %01d\n", __LINE__); 


		return; 
	}


	// Map to the file
	m_pBuf = (MemBuff*)MapViewOfFile(	m_hMapFile,				// handle to map object
										FILE_MAP_ALL_ACCESS,	// read/write permission
										0,
										0,
										sizeof(MemBuff)); 
	if (m_pBuf == NULL) 
	{ 
		free(m_sEvtAvail); 
		free(m_sEvtFilled); 
		free(m_sMemName); 
		printf("client map view: failed: %01d\n", __LINE__); 
		return; 
	}

	// Release memory
	free(m_sEvtAvail);
	free(m_sEvtFilled);
	free(m_sMemName);
	free(memName);
};

osIPC::Client::~Client(void)
{
	// Close the event
	CloseHandle(m_hSignal);

	// Close the event
	CloseHandle(m_hAvail);

	// Unmap the memory
	UnmapViewOfFile(m_pBuf);

	// Close the file handle
	CloseHandle(m_hMapFile);
};

osIPC::Block* osIPC::Client::getBlock(DWORD dwTimeout)
{
	// No room is available, wait for room to become available
	mydebug("waiting m_hAvail...\n");
	m_stat.nWait++;
	switch (WaitForSingleObject(m_hAvail, dwTimeout))
	{
	case WAIT_OBJECT_0:
		mydebug("wake up via m_hAvail\n");
		m_stat.nWokeUpSuccess++;
		for (;;)
		{
			m_stat.nTryGetBlock++;
			LONG blockIndex = m_pBuf->m_WriteStart;
			Block *pBlock = m_pBuf->m_Blocks + blockIndex;
			
			// Make sure the operation is atomic
			if (InterlockedCompareExchange(&m_pBuf->m_WriteStart, pBlock->Next, blockIndex) == blockIndex)
			{
				pBlock->writing = 1;
				return pBlock;
			}
		}

	case WAIT_TIMEOUT:
		return NULL;

	case WAIT_FAILED:
		return (osIPC::Block *)1; // explicit errer.
	}
};

void osIPC::Client::postBlock(Block *pBlock)
{
	// Set the done flag for this block
	pBlock->doneWrite = 1;
	pBlock->writing = 0;

	// Move the write pointer as far forward as we can
	for (;;)
	{
		// Try and get the right to move the poitner
		DWORD blockIndex = m_pBuf->m_WriteEnd;
		pBlock = m_pBuf->m_Blocks + blockIndex;
		if (InterlockedCompareExchange(&pBlock->doneWrite, 0, 1) != 1)
		{
			// If we get here then another thread has already moved the pointer
			// for us or we have reached as far as we can possible move the pointer
			return;
		}

		// Move the pointer one forward (interlock protected)
		InterlockedCompareExchange(&m_pBuf->m_WriteEnd, pBlock->Next, blockIndex);
		
		// Signal availability of more data but only if threads are waiting
		mydebug("client post after write. m_hSignal. could read now.\n");
		if (!ReleaseSemaphore(m_hSignal, 1, NULL))
        {
			printf("ReleaseSemaphore error: %d\n", GetLastError());
			printError();
		}
		m_stat.nPostBlock++;
	}
};

DWORD osIPC::Client::write(void *pBuff, DWORD amount, DWORD dwTimeout)
{
	mydebug("writing...\n");

	// Grab a block
	Block *pBlock = getBlock(dwTimeout);
	if (!pBlock) return 0xffffffff; // time out
	mydebug("getted block\n");
	m_stat.nGetBlock++;

	// Copy the data
	DWORD dwAmount = min(amount, IPC_BLOCK_SIZE);
	memcpy(pBlock->Data, pBuff, dwAmount);
	pBlock->Amount = dwAmount;

	// Post the block
	postBlock(pBlock);
	mydebug("posted block\n");
	
	mydebug("write done. returning %lu\n", dwAmount);

	return dwAmount;
};

bool osIPC::Client::waitAvailable(DWORD dwTimeout)
{
	// Wait on the available event
	if (WaitForSingleObject(m_hAvail, dwTimeout) != WAIT_OBJECT_0)
	{
		mydebug("wake up via m_hAvail\n");
		return false;
	}

	// Success
	return true;
};

void osIPC::Client::printStatus(void)
{
	printf("m_ReadStart  is : %lu\n", m_pBuf->m_ReadStart);
	printf("m_ReadEnd    is : %lu\n", m_pBuf->m_ReadEnd);
	printf("m_WriteStart is : %lu\n", m_pBuf->m_WriteStart);
	printf("m_WriteEnd   is : %lu\n", m_pBuf->m_WriteEnd);
	printf("nTryGetBlock      is : %lu\n", m_stat.nTryGetBlock);
	printf("nGetBlock         is : %lu\n", m_stat.nGetBlock);
	printf("nPostBlock        is : %lu\n", m_stat.nPostBlock);
	printf("nWait             is : %lu\n", m_stat.nWait);
	printf("nWokeUpSuccess    is : %lu\n", m_stat.nWokeUpSuccess);
	printf("nReleaseSemaphore is : %lu\n", m_stat.nReleaseSemaphore);
}