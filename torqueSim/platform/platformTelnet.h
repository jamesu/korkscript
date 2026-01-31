//-----------------------------------------------------------------------------
// Copyright (c) 2025-2026 korkscript contributors.
// See AUTHORS file and git repository for contributor information.
//
// SPDX-License-Identifier: MIT
//-----------------------------------------------------------------------------

/// Adapter that provides TelnetInterface callbacks implemented using PlatformNetwork Net API.
///
class TelnetPlatformNetworkAdapter
{
public:
   using QueueEvalFn = void(*)(void* user, const char* evalStr);
   
   struct ExecCallbacks
   {
      void*      user;
      QueueEvalFn queueEval;
   };
   
   struct Client
   {
      NetSocket  sock;
      NetAddress addr;
      bool       active;
   };
   
private:
   
   KorkApi::TelnetInterface mIface;
   ExecCallbacks   mExec;
   
   // Listener per "kind".
   std::unordered_map<KorkApi::TelnetSocket, NetSocket> mListeners;
   
   // Client sockets indexed by telnet-facing ID.
   std::unordered_map<U32, Client> mClients;
   
public:
   
   TelnetPlatformNetworkAdapter(const ExecCallbacks& exec = {});
   ~TelnetPlatformNetworkAdapter();
   
   const KorkApi::TelnetInterface& getInterface() const { return mIface; }
   
private:
   
   bool isValidSocket(NetSocket s) const;
   
   Client* findClient(U32 id);
   
   // Telnet Interface
   
   bool startListen(KorkApi::TelnetSocket kind, int port);
   
   bool stopListen(KorkApi::TelnetSocket kind);
   
   bool checkListen(KorkApi::TelnetSocket kind) const;
   
   U32 checkAccept(KorkApi::TelnetSocket kind);

   bool checkSocketActive(U32 socketId);
   
   bool stopSocket(U32 socketId);
   
   void sendData(U32 socketId, U32 bytes, const void* data);
   
   bool recvData(U32 socketId, void* data, U32 bufferBytes, U32* outBytes);
   
   void getSocketAddress(U32 socketId, char* buffer256);
   
   void queueEvaluate(const char* evalStr);
   
   void yieldExec();
};
