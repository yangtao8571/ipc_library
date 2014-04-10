ipc_library
===========

- Overview:

  This is the inter process communication library, based on the shared memory techneque on windows.
  In any IPC implementation, it's always best to derive a server/client approach to communication. 
  Also, for simplicity, it's wise to have the communication one way, normally to the server. 
  It's very easy to modify an IPC to perform two way communication. Simply create two IPC servers on both processes. 
  Making your IPC communication one way allows you to concentrate on performance issues.
  
- Features:

  1. Support multi-clients to one server, write operation of clients is thread-safe.
  2. c#, java could use this library with a same adapter dll.
  3. Support 64 bit windows. Only windows.
  4. Server read data in the same order with clients write.

- Performance:
  
  1. Bandwidth test shows transfer speed is about 500MB/sec on my 2 core 64bit 2.5G Hz 4G memory windows, with no optimization compiler option.
