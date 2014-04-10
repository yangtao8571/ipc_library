ipc_library
===========

- Overview:

  This is the inter process communication library(just a native dll), based on the shared memory techneque on windows.
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

- Usage:
  
  The example is java process write some data to c# process.
  1. c# example:

  class Program
    {
        [DllImport("coredll.dll", SetLastError = true)]
        public static extern IntPtr LocalAlloc(uint uFlags, uint uBytes);

        [DllImport("ipc_adapter.dll", EntryPoint = "ipcServerNew", CallingConvention = CallingConvention.StdCall, CharSet = CharSet.Ansi)]
        extern public static Boolean ipcServerNew(string addr);

        [DllImport("ipc_adapter.dll", EntryPoint = "ipcServerRead", CallingConvention = CallingConvention.StdCall, CharSet = CharSet.Ansi)]
        extern public static UInt32 ipcServerRead(IntPtr pBuff, UInt32 buffSize, UInt32 timeout);

        [DllImport("ipc_adapter.dll", EntryPoint = "ipcClientNew", CallingConvention = CallingConvention.StdCall, CharSet = CharSet.Ansi)]
        extern public static Boolean ipcClientNew(string addr);

        [DllImport("ipc_adapter.dll", EntryPoint = "ipcClientWrite", CallingConvention = CallingConvention.StdCall, CharSet = CharSet.Ansi)]
        extern public static UInt32 ipcClientWrite(IntPtr pBuff, UInt32 amount, UInt32 dwTimeout);
        
        static void Main(string[] args)
        {
          ipcServerNew("a_test_server");
          Byte[] buf = new Byte[4000];
          IntPtr p = Marshal.AllocHGlobal(4000);
          for (;;)
            {
                Console.WriteLine("blocking read...");

                uint ramount = ipcServerRead(p, 4000, 10000);
                if (ramount == 0xffffffff) break;
                Console.WriteLine("read " + ramount + " bytes.");

                Marshal.Copy(p, buf, 0, (int)ramount);

                Console.WriteLine(
                    Encoding.ASCII.GetString(buf, 0, (int)ramount));
            }
        }
}

  2. java example, using JNA to access native DLL, need jna.jar:


// Ipc_shared_memory_dll.java

import com.sun.jna.Library;
import com.sun.jna.Native;
import com.sun.jna.Pointer;
import com.sun.jna.Memory;

public interface Ipc_shared_memory_dll extends Library{
	Ipc_shared_memory_dll INSTANCE = (Ipc_shared_memory_dll) Native.loadLibrary("ipc_adapter", Ipc_shared_memory_dll.class);
	
	int ipcServerNew(String addr);
	int ipcServerRead(Pointer pBuff, int bufSize, int timeout);
	
	int ipcClientNew(String addr);
	int ipcClientWrite(Pointer pBuff, int amount, int dwTimeout);
}

// main method

public static main(String[] args){
  Ipc_shared_memory_dll ipcDll = Ipc_shared_memory_dll.INSTANCE;
  ipcDll.ipcClientNew("a_test_server");
  Memory buf = new Memory(4096);
	String cmd = "HEARTBEAT";
	int len = buf.write(0, cmd.getBytes(), 0, cmd.length());
	ipcDll.ipcClientWrite(buf, len, 0xffffffff);
}
