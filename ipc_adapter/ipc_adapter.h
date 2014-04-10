#ifndef IPC_ADAPTER_H
#define IPC_ADAPTER_H

extern "C" {


CLASS_DECLSPEC BOOL				ipcServerNew(const char *addr);
CLASS_DECLSPEC BOOL				ipcServerDestroy(void);
CLASS_DECLSPEC DWORD			ipcServerRead(void *pBuff, DWORD buffSize, DWORD timeout);


CLASS_DECLSPEC BOOL				ipcClientNew(const char *addr);
CLASS_DECLSPEC BOOL				ipcClientDestroy(DWORD clientId);
CLASS_DECLSPEC DWORD			ipcClientWrite(void *pBuff, DWORD amount, DWORD dwTimeout);//0xFFFFFFFF is infinite timeout


}

#endif