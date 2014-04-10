#ifndef IPC_H
#define IPC_H

#include <windows.h>

#ifdef IPC_SHARED_MEMORY_DLL_EXPORTS
   #define CLASS_DECLSPEC    __declspec(dllexport)
#else
   #define CLASS_DECLSPEC    __declspec(dllimport)
#endif

// Definitions
#define IPC_BLOCK_COUNT			512
#define IPC_BLOCK_SIZE			4096

#define IPC_MAX_ADDR			256



// ---------------------------------------
// -- Inter-Process Communication class --
// ---------------------------------------------------------------
// Provides intercommunication between processes and there threads
// ---------------------------------------------------------------
class CLASS_DECLSPEC osIPC
{
public:
	// Block that represents a piece of data to transmit between the
	// client and server
	struct Block
	{
		// Variables
		LONG					Next;						// Next block in the circular linked list
		LONG					Prev;						// Previous block in the circular linked list

		volatile LONG			doneRead;					// Flag used to signal that this block has been read
		volatile LONG			doneWrite;					// Flag used to signal that this block has been written
		LONG					writing;
		
		DWORD					Amount;						// Amount of data help in this block
		DWORD					_Padding;					// Padded used to ensure 64bit boundary

		BYTE					Data[IPC_BLOCK_SIZE];		// Data contained in this block
	};

	// statistics information
	struct Stat {
		volatile unsigned long	nTryGetBlock;
		volatile unsigned long	nGetBlock;
		union {
		volatile unsigned long	nRetBlock;
		volatile unsigned long	nPostBlock;
		};
		volatile unsigned long	nWait;
		volatile unsigned long	nWokeUpSuccess;
		volatile unsigned long	nReleaseSemaphore;
	};

private:
	// Shared memory buffer that contains everything required to transmit
	// data between the client and server
	struct MemBuff
	{
		// Block data, this is placed first to remove the offset (optimisation)
		Block					m_Blocks[IPC_BLOCK_COUNT];	// Array of buffers that are used in the communication

		// Cursors
		volatile LONG			m_ReadEnd;					// End of the read cursor
		volatile LONG			m_ReadStart;				// Start of read cursor

		volatile LONG			m_WriteEnd;					// Pointer to the first write cursor, i.e. where we are currently writting to
		volatile LONG			m_WriteStart;				// Pointer in the list where we are currently writting
	};

public:
	// ID Generator
	static DWORD GetID(void)
	{
		// Generate an ID and return id
		static volatile LONG id = 1;
		return (DWORD)InterlockedIncrement((LONG*)&id);
	};

public:
	// Server class
	class Server
	{
	public:
		// Construct / Destruct
		CLASS_DECLSPEC Server(const char *addr = NULL);
		CLASS_DECLSPEC ~Server();

	private:
		Stat					m_stat;
		// Internal variables
		char					*m_sAddr;		// Address of this server
		HANDLE					m_hMapFile;		// Handle to the mapped memory file
		HANDLE					m_hSignal;		// Event used to signal when data exists
		HANDLE					m_hAvail;		// Event used to signal when some blocks become available
		MemBuff					*m_pBuf;		// Buffer that points to the shared memory

	public:
		// Exposed functions
		CLASS_DECLSPEC DWORD	read(void *pBuff, DWORD buffSize, DWORD timeout = INFINITE);
		char*					getAddress(void) { return m_sAddr; };
		CLASS_DECLSPEC void		recoveryFromClientDeath(void);
		CLASS_DECLSPEC BOOL		isInitState(void);
		CLASS_DECLSPEC void		printStatus(void);


		// Block functions
		Block*					getBlock(DWORD dwTimeout = INFINITE);
		void					retBlock(Block* pBlock);

		// Create and destroy functions
		void					create(const char *addr);
		void					close(void);
	};

	// Client class
	class Client
	{
	public:
		// Construct / Destruct
		CLASS_DECLSPEC Client(void);
		CLASS_DECLSPEC Client(const char *connectAddr);
		CLASS_DECLSPEC ~Client();

	private:
		Stat					m_stat;
		// Internal variables
		char					*m_sAddr;		// Address of this server
		HANDLE					m_hMapFile;		// Handle to the mapped memory file
		HANDLE					m_hSignal;		// Event used to signal when data exists
		HANDLE					m_hAvail;		// Event used to signal when some blocks become available
		MemBuff					*m_pBuf;		// Buffer that points to the shared memory

	public:
		// Exposed functions
		CLASS_DECLSPEC DWORD	write(void *pBuff, DWORD amount, DWORD dwTimeout = INFINITE);	// Writes to the buffer
		bool					waitAvailable(DWORD dwTimeout = INFINITE);						// Waits until some blocks become available

		Block*					getBlock(DWORD dwTimeout = INFINITE);							// Gets a block
		void					postBlock(Block *pBlock);										// Posts a block to be processed				
		CLASS_DECLSPEC void		printStatus(void);
		// Functions
		CLASS_DECLSPEC BOOL		IsOk(void) { if (m_pBuf) return true; else return false; };
		
	};
};

#endif