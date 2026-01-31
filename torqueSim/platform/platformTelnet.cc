//-----------------------------------------------------------------------------
// Copyright (c) 2025-2026 korkscript contributors.
// See AUTHORS file and git repository for contributor information.
//
// SPDX-License-Identifier: MIT
//-----------------------------------------------------------------------------

#include <unordered_map>
#include <vector>
#include <cstdint>
#include <cstring>
#include <cstdio>

#include "platform/platform.h"
#include "platform/platformProcess.h"
#include "platform/platformNetwork.h"
#include "embed/api.h"
#include "platform/platformTelnet.h"


TelnetPlatformNetworkAdapter::TelnetPlatformNetworkAdapter(const ExecCallbacks& exec)
: mExec(exec)
{
   // Build the interface vtable.
   mIface.StartListenFn       = &KorkApi::APIThunk<TelnetPlatformNetworkAdapter, &TelnetPlatformNetworkAdapter::startListen>::call;
   mIface.StopListenFn        = &KorkApi::APIThunk<TelnetPlatformNetworkAdapter, &TelnetPlatformNetworkAdapter::stopListen>::call;
   
   mIface.CheckSocketActiveFn = &KorkApi::APIThunk<TelnetPlatformNetworkAdapter, &TelnetPlatformNetworkAdapter::checkSocketActive>::call;
   mIface.CheckAcceptFn       = &KorkApi::APIThunk<TelnetPlatformNetworkAdapter, &TelnetPlatformNetworkAdapter::checkAccept>::call;
   mIface.CheckListenFn       = &KorkApi::APIThunk<TelnetPlatformNetworkAdapter, &TelnetPlatformNetworkAdapter::checkListen>::call;
   mIface.StopSocketFn        = &KorkApi::APIThunk<TelnetPlatformNetworkAdapter, &TelnetPlatformNetworkAdapter::stopSocket>::call;
   
   mIface.SendDataFn          = &KorkApi::APIThunk<TelnetPlatformNetworkAdapter, &TelnetPlatformNetworkAdapter::sendData>::call;
   mIface.RecvDataFn          = &KorkApi::APIThunk<TelnetPlatformNetworkAdapter, &TelnetPlatformNetworkAdapter::recvData>::call;
   
   mIface.GetSocketAddressFn  = &KorkApi::APIThunk<TelnetPlatformNetworkAdapter, &TelnetPlatformNetworkAdapter::getSocketAddress>::call;
   
   mIface.QueueEvaluateFn     = &KorkApi::APIThunk<TelnetPlatformNetworkAdapter, &TelnetPlatformNetworkAdapter::queueEvaluate>::call;
   mIface.YieldExecFn         = &KorkApi::APIThunk<TelnetPlatformNetworkAdapter, &TelnetPlatformNetworkAdapter::yieldExec>::call;
}

TelnetPlatformNetworkAdapter::~TelnetPlatformNetworkAdapter()
{
   for (auto& kv : mListeners)
   {
      if (kv.second.getHandle() != NetSocket::INVALID.getHandle())
      {
         Net::closeSocket(kv.second);
      }
   }
   
   for (auto& kv : mClients)
   {
      if (kv.second.sock.getHandle() != NetSocket::INVALID.getHandle())
      {
         Net::closeSocket(kv.second.sock);
      }
   }
   
   mListeners.clear();
   mClients.clear();
}

bool TelnetPlatformNetworkAdapter::isValidSocket(NetSocket s) const
{
   return s.getHandle() != NetSocket::INVALID.getHandle() && s.getHandle() >= 0;
}

TelnetPlatformNetworkAdapter::Client* TelnetPlatformNetworkAdapter::findClient(U32 id)
{
   auto it = mClients.find(id);
   return (it == mClients.end()) ? nullptr : &it->second;
}

bool TelnetPlatformNetworkAdapter::startListen(KorkApi::TelnetSocket kind, int port)
{
   stopListen(kind);
   
   NetSocket ls = Net::openListenPort((U16)port, NetAddress::IPAddress);
   if (!isValidSocket(ls))
   {
      return false;
   }
   
   Net::setBlocking(ls, false);
   
   mListeners[kind] = ls;
   return true;
}

bool TelnetPlatformNetworkAdapter::stopListen(KorkApi::TelnetSocket kind)
{
   auto it = mListeners.find(kind);
   if (it == mListeners.end())
   {
      return true;
   }
   
   NetSocket ls = it->second;
   if (isValidSocket(ls))
   {
      Net::closeSocket(ls);
   }
   
   mListeners.erase(it);
   return true;
}

bool TelnetPlatformNetworkAdapter::checkListen(KorkApi::TelnetSocket kind) const
{
   auto it = mListeners.find(kind);
   if (it == mListeners.end())
   {
      return false;
   }
   return it->second.getHandle() != NetSocket::INVALID.getHandle();
}

U32 TelnetPlatformNetworkAdapter::checkAccept(KorkApi::TelnetSocket kind)
{
   auto it = mListeners.find(kind);
   if (it == mListeners.end())
   {
      return 0;
   }
   
   NetSocket listenSock = it->second;
   if (!isValidSocket(listenSock))
   {
      return 0;
   }
   
   NetAddress fromAddr = {};
   NetSocket newSock = Net::accept(listenSock, &fromAddr);
   if (!isValidSocket(newSock))
   {
      return 0;
   }
   
   // Make new client non-blocking as well.
   Net::setBlocking(newSock, false);
   
   Client c;
   c.sock   = newSock;
   c.addr   = fromAddr;
   c.active = true;
   mClients.emplace(newSock.getHandle(), c);
   
   return newSock.getHandle();
}

bool TelnetPlatformNetworkAdapter::checkSocketActive(U32 socketId)
{
   Client* c = findClient(socketId);
   if (!c) return false;
   if (!c->active) return false;
   if (!isValidSocket(c->sock)) return false;
   return true;
}

bool TelnetPlatformNetworkAdapter::stopSocket(U32 socketId)
{
   auto it = mClients.find(socketId);
   if (it == mClients.end())
   {
      return true;
   }
   
   Client& c = it->second;
   if (isValidSocket(c.sock))
   {
      Net::closeSocket(c.sock);
   }
   
   mClients.erase(it);
   return true;
}

void TelnetPlatformNetworkAdapter::sendData(U32 socketId, U32 bytes, const void* data)
{
   Client* c = findClient(socketId);
   if (!c || !c->active || !isValidSocket(c->sock) || !data || bytes == 0)
   {
      return;
   }
   
   S32 written = 0;
   Net::Error e = Net::send(c->sock, (const U8*)data, (S32)bytes, &written);
   
   if (e != Net::NoError && e != Net::WouldBlock)
   {
      c->active = false;
   }
}

bool TelnetPlatformNetworkAdapter::recvData(U32 socketId, void* data, U32 bufferBytes, U32* outBytes)
{
   if (outBytes) *outBytes = 0;
   
   Client* c = findClient(socketId);
   if (!c || !c->active || !isValidSocket(c->sock) || !data || bufferBytes == 0)
   {
      return false;
   }
   
   S32 got = 0;
   Net::Error e = Net::recv(c->sock, (U8*)data, (S32)bufferBytes, &got);
   
   if (e == Net::NoError)
   {
      if (got <= 0)
      {
         c->active = false;
         return false;
      }
      
      if (outBytes) *outBytes = (U32)got;
      return true;
   }
   
   if (e == Net::WouldBlock)
   {
      return true;
   }
   
   // Any other error => treat as dead.
   c->active = false;
   return false;
}

void TelnetPlatformNetworkAdapter::getSocketAddress(U32 socketId, char* buffer256)
{
   if (!buffer256)
      return;
   
   buffer256[0] = '\0';
   
   Client* c = findClient(socketId);
   if (!c || !isValidSocket(c->sock))
   {
      std::snprintf(buffer256, 256, "invalid");
      return;
   }
   
   char tmp[256] = {};
   Net::addressToString(&c->addr, tmp);
   std::snprintf(buffer256, 256, "%s", tmp);
}

void TelnetPlatformNetworkAdapter::queueEvaluate(const char* evalStr)
{
   if (mExec.queueEval)
   {
      mExec.queueEval(mExec.user, evalStr ? evalStr : "");
   }
}

void TelnetPlatformNetworkAdapter::yieldExec()
{
   Platform::sleep(10);
}
