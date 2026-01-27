//-----------------------------------------------------------------------------
// Copyright (c) 2012 GarageGames, LLC
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//-----------------------------------------------------------------------------

#include "core/torqueConfig.h"
#include "platform/platformNetwork.h"
#include "platform/platformString.h"
#include "platform/threads/mutex.h"
//#include "platform/event.h"
#include "core/hashFunction.h"
#include "core/fileStream.h"
#include "platform/platformNetAsync.h"
//#include "platform/gameInterface.h"
#include <string.h>
#include <stdio.h>
#include <algorithm>
#include "console/console.h"

// jamesu - debug DNS
//#define TORQUE_DEBUG_LOOKUPS

namespace PlatformNetState
{

   /// Extracts core address parts from an address string. Returns false if it's malformed.
   bool extractAddressParts(const char *addressString, char outAddress[256], int &outPort, NetAddress::Type &outType)
   {
      outPort = 0;
      outType = NetAddress::Invalid;
      
      if (!dStrnicmp(addressString, "ipx:", 4))
         // ipx support deprecated
         return false;
      
      if (!dStrnicmp(addressString, "ip:", 3))
      {
         addressString += 3;  // eat off the ip:
         outType = NetAddress::IPAddress;
      }
      else if (!dStrnicmp(addressString, "ip6:", 4))
      {
         addressString += 4;  // eat off the ip6:
         outType = NetAddress::IPV6Address;
      }
      
      if (strlen(addressString) > 255)
         return false;
      
      char *portString = NULL;
      
      if (addressString[0] == '[')
      {
         // Must be ipv6 notation
         dStrcpy(outAddress, addressString+1);
         addressString = outAddress;
         
         portString = dStrchr(outAddress, ']');
         if (portString)
         {
            // Sort out the :port after the ]
            *portString++ = '\0';
            if (*portString != ':')
            {
               portString = NULL;
            }
            else
            {
               *portString++ = '\0';
            }
         }
         
         if (outType == NetAddress::Invalid)
         {
            outType = NetAddress::IPV6Address;
         }
      }
      else
      {
         dStrcpy(outAddress, addressString);
         addressString = outAddress;
         
         // Check to see if we have multiple ":" which would indicate this is an ipv6 address
         char* scan = outAddress;
         int colonCount = 0;
         while (*scan != '\0' && colonCount < 2)
         {
            if (*scan++ == ':')
               colonCount++;
         }
         if (colonCount <= 1)
         {
            // either ipv4 or host
            portString = dStrchr(outAddress, ':');
            
            if (portString)
            {
               *portString++ = '\0';
            }
         }
         else if (outType == NetAddress::Invalid)
         {
            // Must be ipv6
            outType = NetAddress::IPV6Address;
         }
      }
      
      if (portString)
      {
         outPort = dAtoi(portString);
      }
      
      return true;
   }
   
}


#if defined (TORQUE_NO_SOCKETS)

typedef S32 SOCKET;

#define STUB_NETWORK

#elif defined (TORQUE_OS_WIN32)
#define TORQUE_USE_WINSOCK
#include <errno.h>
#include <ws2tcpip.h>

#ifndef EINPROGRESS
#define EINPROGRESS             WSAEINPROGRESS
#endif // EINPROGRESS

#define ioctl ioctlsocket

typedef S32 socklen_t;

#elif defined ( TORQUE_OS_MAC )  || defined(TORQUE_OS_OSX) || defined(TORQUE_OS_IOS) || defined(TORQUE_OS_ANDROID)

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <net/if.h>

typedef sockaddr_in SOCKADDR_IN;
typedef sockaddr * PSOCKADDR;
typedef sockaddr SOCKADDR;
typedef in_addr IN_ADDR;
typedef int SOCKET;

#define INVALID_SOCKET -1
#define SOCKET_ERROR   -1

#define closesocket close

#elif defined( TORQUE_OS_LINUX )

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <net/if.h>

typedef sockaddr_in SOCKADDR_IN;
typedef sockaddr_in6 SOCKADDR_IN6;
typedef sockaddr * PSOCKADDR;
typedef sockaddr SOCKADDR;
typedef in_addr IN_ADDR;
typedef in6_addr IN6_ADDR;
typedef int SOCKET;

#define INVALID_SOCKET -1
#define SOCKET_ERROR   -1

#define closesocket close


S32 Poll(SOCKET fd, S32 eventMask, S32 timeoutMs)
{
   pollfd pfd;
   S32 retVal;
   pfd.fd = fd;
   pfd.events = eventMask;
   
   retVal = poll(&pfd, 1, timeoutMs);
   return retVal;
   if (retVal <= 0)
      return retVal;
   else
      return pfd.revents;
}

#endif

#if defined(TORQUE_USE_WINSOCK)
static const char* strerror_wsa( S32 code )
{
   switch( code )
   {
#define E( name ) case name: return #name;
         E( WSANOTINITIALISED );
         E( WSAENETDOWN );
         E( WSAEADDRINUSE );
         E( WSAEINPROGRESS );
         E( WSAEALREADY );
         E( WSAEADDRNOTAVAIL );
         E( WSAEAFNOSUPPORT );
         E( WSAEFAULT );
         E( WSAEINVAL );
         E( WSAEISCONN );
         E( WSAENETUNREACH );
         E( WSAEHOSTUNREACH );
         E( WSAENOBUFS );
         E( WSAENOTSOCK );
         E( WSAETIMEDOUT );
         E( WSAEWOULDBLOCK );
         E( WSAEACCES );
#undef E
      default:
         return "Unknown";
   }
}
#endif




NetSocket NetSocket::INVALID = NetSocket::fromHandle(-1);

template<class T> class ReservedSocketList
{
public:
   struct EntryType
   {
      T value;
      bool used;
      
      EntryType() : value(-1), used(false) { ; }
      
      bool operator==(const EntryType &e1)
      {
         return value == e1.value && used == e1.used;
      }
      
      bool operator!=(const EntryType &e1)
      {
         return !(value == e1.value && used == e1.used);
      }
   };
   
   std::vector<EntryType> mSocketList;
   Mutex *mMutex;
   
   ReservedSocketList()
   {
      mMutex = new Mutex;
   }
   
   ~ReservedSocketList()
   {
      delete mMutex;
   }
   
   inline void modify() { Mutex::lockMutex(mMutex); }
   inline void endModify() { Mutex::unlockMutex(mMutex); }
   
   NetSocket reserve(SOCKET reserveId = -1, bool doLock = true);
   void remove(NetSocket socketToRemove, bool doLock = true);
   
   T activate(NetSocket socketToActivate, int family, bool useUDP, bool clearOnFail = false);
   T resolve(NetSocket socketToResolve);
};

const SOCKET InvalidSocketHandle = -1;


#ifndef STUB_NETWORK
static void IPSocketToNetAddress(const struct sockaddr_in *sockAddr, NetAddress *address);
static void IPSocket6ToNetAddress(const struct sockaddr_in6 *sockAddr, NetAddress *address);
#endif

namespace PlatformNetState
{
#ifndef STUB_NETWORK
   static S32 initCount = 0;
   
   static const S32 defaultPort = 28000;
   static S32 netPort = 0;
   
   static NetSocket udpSocket = NetSocket::INVALID;
   static NetSocket udp6Socket = NetSocket::INVALID;
   static NetSocket multicast6Socket = NetSocket::INVALID;
   
   static ipv6_mreq multicast6Group;

   static ReservedSocketList<SOCKET> smReservedSocketList;
   

   Net::Error getLastError()
   {
#if defined(TORQUE_USE_WINSOCK)
      S32 err = WSAGetLastError();
      switch (err)
      {
         case 0:
            return Net::NoError;
         case WSAEWOULDBLOCK:
            return Net::WouldBlock;
         default:
            return Net::UnknownError;
      }
#else
      int theError = errno;
      if (errno == EAGAIN)
         return Net::WouldBlock;
      if (errno == 0)
         return Net::NoError;
      if (errno == EINPROGRESS)
         return Net::WouldBlock;
      
      return Net::UnknownError;
#endif
   }
   
   S32 getDefaultGameProtocol()
   {
      // we turn off VDP in non-release builds because VDP does not support broadcast packets
      // which are required for LAN queries (PC->Xbox connectivity).  The wire protocol still
      // uses the VDP packet structure, though.
      S32 protocol = IPPROTO_UDP;
      bool useVDP = false;
#ifdef TORQUE_DISABLE_PC_CONNECTIVITY
      // Xbox uses a VDP (voice/data protocol) socket for networking
      protocol = IPPROTO_VDP;
      useVDP = true;
#endif
      
      return protocol;
   }
   
   struct addrinfo* pickAddressByProtocol(struct addrinfo* addr, int protocol)
   {
      for (; addr != NULL; addr = addr->ai_next)
      {
         if (addr->ai_family == protocol)
            return addr;
      }
      
      return NULL;
   }

   Net::Error getSocketAddress(SOCKET socketFd, int requiredFamily, NetAddress *outAddress)
   {
      Net::Error error = Net::UnknownError;
      
      if (requiredFamily == AF_INET)
      {
         sockaddr_in ipAddr;
         socklen_t len = sizeof(ipAddr);
         if (getsockname(socketFd, (struct sockaddr*)&ipAddr, &len) >= 0)
         {
            IPSocketToNetAddress(&ipAddr, outAddress);
            error = Net::NoError;
         }
         else
         {
            error = getLastError();
         }
      }
      else if (requiredFamily == AF_INET6)
      {
         sockaddr_in6 ipAddr;
         socklen_t len = sizeof(ipAddr);
         if (getsockname(socketFd, (struct sockaddr*)&ipAddr, &len) >= 0)
         {
            IPSocket6ToNetAddress(&ipAddr, outAddress);
            error = Net::NoError;
         }
         else
         {
            error = getLastError();
         }
      }
      
      return error;
   }

#endif // STUB_NETWORK
};


#ifndef STUB_NETWORK

template<class T> NetSocket ReservedSocketList<T>::reserve(SOCKET reserveId, bool doLock)
{
   MutexHandle handle;
   if (doLock)
   {
      handle.lock(mMutex, true);
   }
   
   auto itr = std::find(mSocketList.begin(), mSocketList.end(), EntryType());
   if (itr == mSocketList.end())
   {
      EntryType entry;
      entry.value = reserveId;
      entry.used = true;
      mSocketList.push_back(entry);
      return NetSocket::fromHandle(mSocketList.size() - 1);
   }
   else
   {
      EntryType &entry = *itr;
      entry.used = true;
      entry.value = reserveId;
   }
   
   return NetSocket::fromHandle(itr - mSocketList.begin());
}

template<class T> void ReservedSocketList<T>::remove(NetSocket socketToRemove, bool doLock)
{
   MutexHandle handle;
   if (doLock)
   {
      handle.lock(mMutex, true);
   }
   
   if ((U32)socketToRemove.getHandle() >= (U32)mSocketList.size())
      return;
   
   mSocketList[socketToRemove.getHandle()] = EntryType();
}

template<class T> T ReservedSocketList<T>::activate(NetSocket socketToActivate, int family, bool useUDP, bool clearOnFail)
{
   MutexHandle h;
   h.lock(mMutex, true);
   
   int typeID = useUDP ? SOCK_DGRAM : SOCK_STREAM;
   int protocol = useUDP ? PlatformNetState::getDefaultGameProtocol() : 0;
   
   if ((U32)socketToActivate.getHandle() >= (U32)mSocketList.size())
      return -1;
   
   EntryType &entry = mSocketList[socketToActivate.getHandle()];
   if (!entry.used)
      return -1;
   
   T socketFd = entry.value;
   if (socketFd == -1)
   {
      socketFd = ::socket(family, typeID, protocol);
      
      if (socketFd == InvalidSocketHandle)
      {
         if (clearOnFail)
         {
            remove(socketToActivate, false);
         }
         return InvalidSocketHandle;
      }
      else
      {
         entry.used = true;
         entry.value = socketFd;
         return socketFd;
      }
   }
   
   return socketFd;
}

template<class T> T ReservedSocketList<T>::resolve(NetSocket socketToResolve)
{
   MutexHandle h;
   h.lock(mMutex, true);
   
   if ((U32)socketToResolve.getHandle() >= (U32)mSocketList.size())
      return -1;
   
   EntryType &entry = mSocketList[socketToResolve.getHandle()];
   return entry.used ? entry.value : -1;
}


// Multicast stuff
bool Net::smMulticastEnabled = true;
//
// Protocol Stuff
bool Net::smIpv4Enabled = true;
bool Net::smIpv6Enabled = false;
//

// the Socket structure helps us keep track of the
// above states
struct PolledSocket
{
   // local enum for socket states for polled sockets
   enum SocketState
   {
      InvalidState,
      Connected,
      ConnectionPending,
      Listening,
      NameLookupRequired
   };
   
   PolledSocket()
   {
      fd = -1;
      handleFd = NetSocket::INVALID;
      state = InvalidState;
      remoteAddr[0] = 0;
      remotePort = -1;
   }
   
   SOCKET fd;
   NetSocket handleFd;
   S32 state;
   char remoteAddr[256];
   S32 remotePort;
};

// list of polled sockets
static std::vector<PolledSocket*> gPolledSockets;

static PolledSocket* addPolledSocket(NetSocket handleFd, SOCKET fd, S32 state,
                                     char* remoteAddr = NULL, S32 port = -1)
{
   PolledSocket* sock = new PolledSocket();
   sock->fd = fd;
   sock->handleFd = handleFd;
   sock->state = state;
   if (remoteAddr)
      dStrcpy(sock->remoteAddr, remoteAddr);
   if (port != -1)
      sock->remotePort = port;
   gPolledSockets.push_back(sock);
   return sock;
}

bool netSocketWaitForWritable(NetSocket handleFd, S32 timeoutMs)
{
   fd_set writefds;
   timeval timeout;
   SOCKET socketFd = PlatformNetState::smReservedSocketList.resolve(handleFd);
   
   FD_ZERO( &writefds );
   FD_SET( socketFd, &writefds );
   
   timeout.tv_sec = timeoutMs / 1000;
   timeout.tv_usec = ( timeoutMs % 1000 ) * 1000;
   
   if( select(socketFd + 1, NULL, &writefds, NULL, &timeout) > 0 )
      return true;
   
   return false;
}

bool Net::init()
{
   if(!PlatformNetState::initCount)
   {
#if defined(TORQUE_USE_WINSOCK)
      WSADATA stWSAData;
      AssertISV( !WSAStartup( 0x0101, &stWSAData ), "Net::init - failed to init WinSock!" );
      //logprintf("Winsock initialization %s", success ? "succeeded." : "failed!");
#endif
      NetAsync::startAsync();
   }
   PlatformNetState::initCount++;
   
   return(true);
}

void Net::shutdown()
{
   
   while (gPolledSockets.size() > 0)
   {
      if (gPolledSockets[0] == NULL)
         gPolledSockets.erase(gPolledSockets.begin());
      else
         closeConnectTo(gPolledSockets[0]->handleFd);
   }
   
   closePort();
   NetAsync::stopAsync();
   PlatformNetState::initCount--;
   
   
#if defined(TORQUE_USE_WINSOCK)
   if(!PlatformNetState::initCount)
   {
      WSACleanup();
   }
#endif
}

// ipv4 version of name routines

static void NetAddressToIPSocket(const NetAddress *address, struct sockaddr_in *sockAddr)
{
   memset(sockAddr, 0, sizeof(struct sockaddr_in));
   sockAddr->sin_family = AF_INET;
   sockAddr->sin_port = htons(address->port);
#if defined(TORQUE_OS_BSD) || defined(TORQUE_OS_MAC) || defined(TORQUE_OS_OSX) || defined(TORQUE_OS_IOS)
   sockAddr->sin_len = sizeof(struct sockaddr_in);
#endif
   if (address->type == NetAddress::IPBroadcastAddress)
   {
      sockAddr->sin_addr.s_addr = htonl(INADDR_BROADCAST);
   }
   else
   {
      memcpy(&sockAddr->sin_addr, &address->address.ipv4.netNum[0], 4);
   }
}

static void IPSocketToNetAddress(const struct sockaddr_in *sockAddr, NetAddress *address)
{
   address->type = NetAddress::IPAddress;
   address->port = ntohs(sockAddr->sin_port);
   memcpy(&address->address.ipv4.netNum[0], &sockAddr->sin_addr, 4);
}

// ipv6 version of name routines

static void NetAddressToIPSocket6(const NetAddress *address, struct sockaddr_in6 *sockAddr)
{
   memset(sockAddr, 0, sizeof(struct sockaddr_in6));
#ifdef SIN6_LEN
   sockAddr->sin6_len = sizeof(struct sockaddr_in6);
#endif
   sockAddr->sin6_family = AF_INET6;
   sockAddr->sin6_port = ntohs(address->port);
   
   if (address->type == NetAddress::IPV6MulticastAddress)
   {
      sockAddr->sin6_addr = PlatformNetState::multicast6Group.ipv6mr_multiaddr;
      sockAddr->sin6_scope_id = PlatformNetState::multicast6Group.ipv6mr_interface;
   }
   else
   {
      sockAddr->sin6_flowinfo = address->address.ipv6.netFlow;
      sockAddr->sin6_scope_id = address->address.ipv6.netScope;
      memcpy(&sockAddr->sin6_addr, address->address.ipv6.netNum, sizeof(address->address.ipv6.netNum));
   }
}

static void IPSocket6ToNetAddress(const struct sockaddr_in6 *sockAddr, NetAddress *address)
{
   address->type = NetAddress::IPV6Address;
   address->port = ntohs(sockAddr->sin6_port);
   memcpy(address->address.ipv6.netNum, &sockAddr->sin6_addr, sizeof(address->address.ipv6.netNum));
   address->address.ipv6.netFlow = sockAddr->sin6_flowinfo;
   address->address.ipv6.netScope = sockAddr->sin6_scope_id;
}

//

NetSocket Net::openListenPort(U16 port, NetAddress::Type addressType)
{
#ifdef TORQUE_ALLOW_JOURNALING
   if(Game->isJournalReading())
   {
      U32 ret;
      Game->journalRead(&ret);
      return NetSocket::fromHandle(ret);
   }
#endif
   
   Net::Error error = NoError;
   NetAddress address;
   if (Net::getListenAddress(addressType, &address) != Net::NoError)
      error = Net::WrongProtocolType;
   
   NetSocket handleFd = NetSocket::INVALID;
   SOCKET sockId = InvalidSocketHandle;
   
   if (error == NoError)
   {
      handleFd = openSocket();
      sockId = PlatformNetState::smReservedSocketList.activate(handleFd, address.type == NetAddress::IPAddress ? AF_INET : AF_INET6, false, true);
   }
   
   if (error == NoError && (handleFd == NetSocket::INVALID || sockId == InvalidSocketHandle))
   {
      Con::errorf("Unable to open listen socket: %s", strerror(errno));
      error = NotASocket;
      handleFd = NetSocket::INVALID;
   }
   
   if (error == NoError)
   {
      address.port = port;
      error = bindAddress(address, handleFd, false);
      if (error != NoError)
      {
         Con::errorf("Unable to bind port %d: %s", port, strerror(errno));
         closeSocket(handleFd);
         handleFd = NetSocket::INVALID;
      }
   }
   
   if (error == NoError)
   {
      error = listen(handleFd, 4);
      if (error != NoError)
      {
         Con::errorf("Unable to listen on port %d: %s", port, strerror(errno));
         closeSocket(handleFd);
         handleFd = NetSocket::INVALID;
      }
   }
   
   if (error == NoError)
   {
      setBlocking(handleFd, false);
      addPolledSocket(handleFd, sockId, PolledSocket::Listening);
   }
   
#ifdef TORQUE_ALLOW_JOURNALING
   if(Game->isJournalWriting())
      Game->journalWrite(U32(handleFd.getHandle()));
#endif
   return handleFd;
}

NetSocket Net::openConnectTo(const char *addressString)
{
#ifdef TORQUE_ALLOW_JOURNALING
   if (Game->isJournalReading())
   {
      U32 ret;
      Game->journalRead(&ret);
      return NetSocket::fromHandle(ret);
   }
#endif
   NetAddress address;
   NetSocket handleFd = NetSocket::INVALID;
   Net::Error error = NoError;
   
   error = Net::stringToAddress(addressString, &address, false);
   
   if (error == NoError && address.type != NetAddress::IPAddress && address.type != NetAddress::IPV6Address)
   {
      error = Net::WrongProtocolType;
   }
   
   // Open socket
   if (error == NoError || error == NeedHostLookup)
   {
      handleFd = openSocket();
   }
   
   // Attempt to connect or queue a lookup
   if (error == NoError && address.type == NetAddress::IPAddress)
   {
      sockaddr_in ipAddr;
      NetAddressToIPSocket(&address, &ipAddr);
      SOCKET socketFd = PlatformNetState::smReservedSocketList.activate(handleFd, AF_INET, false, true);
      if (socketFd != InvalidSocketHandle)
      {
         setBlocking(handleFd, false);
         if (::connect(socketFd, (struct sockaddr *)&ipAddr, sizeof(ipAddr)) == -1)
         {
            Net::Error err = PlatformNetState::getLastError();
            if (err != Net::WouldBlock)
            {
               Con::errorf("Error connecting to %s: %u",
                           addressString, err);
               closeSocket(handleFd);
               handleFd = NetSocket::INVALID;
            }
         }
      }
      else
      {
         PlatformNetState::smReservedSocketList.remove(handleFd);
         handleFd = NetSocket::INVALID;
      }
      
      if (handleFd != NetSocket::INVALID)
      {
         // add this socket to our list of polled sockets
         addPolledSocket(handleFd, socketFd, PolledSocket::ConnectionPending);
      }
   }
   else if (error == NoError && address.type == NetAddress::IPV6Address)
   {
      sockaddr_in6 ipAddr6;
      NetAddressToIPSocket6(&address, &ipAddr6);
      SOCKET socketFd = PlatformNetState::smReservedSocketList.activate(handleFd, AF_INET6, false, true);
      if (socketFd != InvalidSocketHandle)
      {
         setBlocking(handleFd, false);
         if (::connect(socketFd, (struct sockaddr *)&ipAddr6, sizeof(ipAddr6)) == -1)
         {
            Net::Error err = PlatformNetState::getLastError();
            if (err != Net::WouldBlock)
            {
               Con::errorf("Error connecting to %s: %u",
                           addressString, err);
               closeSocket(handleFd);
               handleFd = NetSocket::INVALID;
            }
         }
      }
      else
      {
         PlatformNetState::smReservedSocketList.remove(handleFd);
         handleFd = NetSocket::INVALID;
      }
      
      if (handleFd != NetSocket::INVALID)
      {
         // add this socket to our list of polled sockets
         addPolledSocket(handleFd, socketFd, PolledSocket::ConnectionPending);
      }
   }
   else if (error == Net::NeedHostLookup)
   {
      // need to do an asynchronous name lookup.  first, add the socket
      // to the polled list
      char addr[256];
      int port = 0;
      NetAddress::Type actualType = NetAddress::Invalid;
      if (PlatformNetState::extractAddressParts(addressString, addr, port, actualType))
      {
         addPolledSocket(handleFd, InvalidSocketHandle, PolledSocket::NameLookupRequired, addr, port);
         // queue the lookup
         gNetAsync.queueLookup(addressString, handleFd);
      }
      else
      {
         closeSocket(handleFd);
         handleFd = NetSocket::INVALID;
      }
   }
   else
   {
      closeSocket(handleFd);
      handleFd = NetSocket::INVALID;
   }
#ifdef TORQUE_ALLOW_JOURNALING
   if (Game->isJournalWriting())
      Game->journalWrite(U32(handleFd.getHandle()));
#endif
   return handleFd;
}

void Net::closeConnectTo(NetSocket handleFd)
{
#ifdef TORQUE_ALLOW_JOURNALING
   if(Game->isJournalReading())
      return;
#endif
   // if this socket is in the list of polled sockets, remove it
   for (S32 i = 0; i < gPolledSockets.size(); ++i)
   {
      if (gPolledSockets[i] && gPolledSockets[i]->handleFd == handleFd)
      {
         delete gPolledSockets[i];
         gPolledSockets[i] = NULL;
         break;
      }
   }
   
   closeSocket(handleFd);
}

Net::Error Net::sendtoSocket(NetSocket handleFd, const U8 *buffer, S32  bufferSize, S32 *outBufferWritten)
{
#ifdef TORQUE_ALLOW_JOURNALING
   if(Game->isJournalReading())
   {
      U32 e;
      U32 outBytes;
      Game->journalRead(&e);
      Game->journalRead(&outBytes);
      if (outBufferWritten)
         *outBufferWritten = outBytes;
      
      return (Net::Error) e;
   }
#endif
   S32 outBytes = 0;
   Net::Error e = send(handleFd, buffer, bufferSize, &outBytes);
#ifdef TORQUE_ALLOW_JOURNALING
   if (Game->isJournalWriting())
   {
      Game->journalWrite(U32(e));
      Game->journalWrite(outBytes);
   }
#endif
   if (outBufferWritten)
      *outBufferWritten = outBytes;
   
   return e;
}

bool Net::openPort(S32 port, bool doBind)
{
   if (PlatformNetState::udpSocket != NetSocket::INVALID)
   {
      closeSocket(PlatformNetState::udpSocket);
      PlatformNetState::udpSocket = NetSocket::INVALID;
   }
   if (PlatformNetState::udp6Socket != NetSocket::INVALID)
   {
      closeSocket(PlatformNetState::udp6Socket);
      PlatformNetState::udp6Socket = NetSocket::INVALID;
   }
   
   // Update prefs
   Net::smMulticastEnabled = Con::getBoolVariable("pref::Net::Multicast6Enabled", true);
   Net::smIpv4Enabled = Con::getBoolVariable("pref::Net::IPV4Enabled", true);
   Net::smIpv6Enabled = Con::getBoolVariable("pref::Net::IPV6Enabled", false);
   
   // we turn off VDP in non-release builds because VDP does not support broadcast packets
   // which are required for LAN queries (PC->Xbox connectivity).  The wire protocol still
   // uses the VDP packet structure, though.
   S32 protocol = PlatformNetState::getDefaultGameProtocol();
   
   SOCKET socketFd = InvalidSocketHandle;
   NetAddress address;
   NetAddress listenAddress;
   char listenAddressStr[256];
   
   if (Net::smIpv4Enabled)
   {
      if (Net::getListenAddress(NetAddress::IPAddress, &address) == Net::NoError)
      {
         address.port = port;
         socketFd = ::socket(AF_INET, SOCK_DGRAM, protocol);
         
         if (socketFd != InvalidSocketHandle)
         {
            PlatformNetState::udpSocket = PlatformNetState::smReservedSocketList.reserve(socketFd);
            Net::Error error = NoError;
            if (doBind)
            {
               error = bindAddress(address, PlatformNetState::udpSocket, true);
            }
            
            if (error == NoError)
               error = setBufferSize(PlatformNetState::udpSocket, 32768 * 8);
            
#ifndef TORQUE_DISABLE_PC_CONNECTIVITY
            if (error == NoError)
               error = setBroadcast(PlatformNetState::udpSocket, true);
#endif
            
            if (error == NoError)
               error = setBlocking(PlatformNetState::udpSocket, false);
            
            if (error == NoError)
            {
               error = PlatformNetState::getSocketAddress(socketFd, AF_INET, &listenAddress);
               if (error == NoError)
               {
                  Net::addressToString(&listenAddress, listenAddressStr);
                  Con::printf("UDP initialized on ipv4 %s", listenAddressStr);
               }
            }
            
            if (error != NoError)
            {
               closeSocket(PlatformNetState::udpSocket);
               PlatformNetState::udpSocket = NetSocket::INVALID;
               Con::printf("Unable to initialize UDP on ipv4 - error %d", error);
            }
         }
      }
      else
      {
         Con::errorf("Unable to initialize UDP on ipv4 - invalid address.");
         PlatformNetState::udpSocket = NetSocket::INVALID;
      }
   }
   
   if (Net::smIpv6Enabled)
   {
      if (Net::getListenAddress(NetAddress::IPV6Address, &address) == Net::NoError)
      {
         address.port = port;
         socketFd = ::socket(AF_INET6, SOCK_DGRAM, protocol);
         
         if (socketFd != InvalidSocketHandle)
         {
            PlatformNetState::udp6Socket = PlatformNetState::smReservedSocketList.reserve(socketFd);
            
            Net::Error error = NoError;
            
            int v = 1;
            setsockopt(socketFd, IPPROTO_IPV6, IPV6_V6ONLY, (const char *)&v, sizeof(v));
            PlatformNetState::getLastError();
            
            if (doBind)
            {
               error = bindAddress(address, PlatformNetState::udp6Socket, true);
            }
            
            if (error == NoError)
               error = setBufferSize(PlatformNetState::udp6Socket, 32768 * 8);
            
            if (error == NoError)
               error = setBlocking(PlatformNetState::udp6Socket, false);
            
            if (error == NoError)
            {
               error = PlatformNetState::getSocketAddress(socketFd, AF_INET6, &listenAddress);
               if (error == NoError)
               {
                  Net::addressToString(&listenAddress, listenAddressStr);
                  Con::printf("UDP initialized on ipv6 %s", listenAddressStr);
               }
            }
            
            if (error != NoError)
            {
               closeSocket(PlatformNetState::udp6Socket);
               PlatformNetState::udp6Socket = NetSocket::INVALID;
               Con::printf("Unable to initialize UDP on ipv6 - error %d", error);
            }
            
            if (Net::smMulticastEnabled && doBind)
            {
               Net::enableMulticast();
            }
            else
            {
               Net::disableMulticast();
            }
         }
      }
   }
   
   PlatformNetState::netPort = port;
   
   return PlatformNetState::udpSocket != NetSocket::INVALID || PlatformNetState::udp6Socket != NetSocket::INVALID;
}

NetSocket Net::getPort()
{
   return PlatformNetState::udpSocket;
}

void Net::closePort()
{
   if (PlatformNetState::udpSocket != NetSocket::INVALID)
      closeSocket(PlatformNetState::udpSocket);
   if (PlatformNetState::udp6Socket != NetSocket::INVALID)
      closeSocket(PlatformNetState::udp6Socket);
}

Net::Error Net::sendto(const NetAddress *address, const U8 *buffer, S32  bufferSize)
{
#ifdef TORQUE_ALLOW_JOURNALING
   if(Game->isJournalReading())
      return NoError;
#endif
   SOCKET socketFd;
   
   if(address->type == NetAddress::IPAddress || address->type == NetAddress::IPBroadcastAddress)
   {
      socketFd = PlatformNetState::smReservedSocketList.resolve(PlatformNetState::udpSocket);
      if (socketFd != InvalidSocketHandle)
      {
         sockaddr_in ipAddr;
         NetAddressToIPSocket(address, &ipAddr);
         
         if (::sendto(socketFd, (const char*)buffer, bufferSize, 0,
                      (sockaddr *)&ipAddr, sizeof(sockaddr_in)) == SOCKET_ERROR)
            return PlatformNetState::getLastError();
         else
            return NoError;
      }
      else
      {
         return NotASocket;
      }
   }
   else if (address->type == NetAddress::IPV6Address || address->type == NetAddress::IPV6MulticastAddress)
   {
      socketFd = PlatformNetState::smReservedSocketList.resolve(address->type == NetAddress::IPV6MulticastAddress ? PlatformNetState::multicast6Socket : PlatformNetState::udp6Socket);
      
      if (socketFd != InvalidSocketHandle)
      {
         sockaddr_in6 ipAddr;
         NetAddressToIPSocket6(address, &ipAddr);
         if (::sendto(socketFd, (const char*)buffer, bufferSize, 0,
                      (struct sockaddr *) &ipAddr, sizeof(sockaddr_in6)) == SOCKET_ERROR)
            return PlatformNetState::getLastError();
         else
            return NoError;
      }
      else
      {
         return NotASocket;
      }
   }
   
   return WrongProtocolType;
}

void Net::process()
{
#if TOFIX
   // Process listening sockets
   processListenSocket(PlatformNetState::udpSocket);
   processListenSocket(PlatformNetState::udp6Socket);
   
   // process the polled sockets.  This blob of code performs functions
   // similar to WinsockProc in winNet.cc
   
   if (gPolledSockets.size() == 0)
      return;
   
   static ConnectedNotifyEvent notifyEvent;
   static ConnectedAcceptEvent acceptEvent;
   static ConnectedReceiveEvent cReceiveEvent;
   
   S32 optval;
   socklen_t optlen = sizeof(S32);
   S32 bytesRead;
   Net::Error err;
   bool removeSock = false;
   PolledSocket *currentSock = NULL;
   NetSocket incomingHandleFd = NetSocket::INVALID;
   NetAddress out_h_addr;
   S32 out_h_length = 0;
   NetSocket removeSockHandle;
   
   for (S32 i = 0; i < gPolledSockets.size();
        /* no increment, this is done at end of loop body */)
   {
      removeSock = false;
      currentSock = gPolledSockets[i];
      
      // Cleanup if we've removed it
      if (currentSock == NULL)
      {
         gPolledSockets.erase(i);
         continue;
      }
      
      switch (currentSock->state)
      {
         case PolledSocket::InvalidState:
            Con::errorf("Error, InvalidState socket in polled sockets  list");
            break;
         case PolledSocket::ConnectionPending:
            // see if it is now connected
            if (getsockopt(currentSock->fd, SOL_SOCKET, SO_ERROR,
                           (char*)&optval, &optlen) == -1)
            {
               Con::errorf("Error getting socket options: %s",  strerror(errno));
               
               removeSock = true;
               removeSockHandle = currentSock->handleFd;
               
               notifyEvent.state = Net::ConnectFailed;
               notifyEvent.tag = currentSock->handleFd.getHandle();
               Game->postEvent(notifyEvent);
            }
            else
            {
               if (optval == EINPROGRESS)
                  // still connecting...
                  break;
               
               if (optval == 0)
               {
                  // poll for writable status to be sure we're connected.
                  bool ready = netSocketWaitForWritable(currentSock->handleFd,0);
                  if(!ready)
                     break;
                  
                  currentSock->state = PolledSocket::Connected;
                  notifyEvent.state = Net::Connected;
                  notifyEvent.tag = currentSock->handleFd.getHandle();
                  Game->postEvent(notifyEvent);
               }
               else
               {
                  // some kind of error
                  Con::errorf("Error connecting: %s", strerror(errno));
                  
                  removeSock = true;
                  removeSockHandle = currentSock->handleFd;
                  
                  notifyEvent.state = Net::ConnectFailed;
                  notifyEvent.tag = currentSock->handleFd.getHandle();
                  Game->postEvent(notifyEvent);
               }
            }
            break;
         case PolledSocket::Connected:
            
            // try to get some data
            bytesRead = 0;
            err = Net::recv(currentSock->handleFd, (U8*)cReceiveEvent.data, MaxPacketDataSize, &bytesRead);
            if(err == Net::NoError)
            {
               if (bytesRead > 0)
               {
                  // got some data, post it
                  cReceiveEvent.size = ConnectedReceiveEventHeaderSize +
                  bytesRead;
                  cReceiveEvent.tag = currentSock->handleFd.getHandle();
                  Game->postEvent(cReceiveEvent);
               }
               else
               {
                  // ack! this shouldn't happen
                  if (bytesRead < 0)
                  {
                     Con::errorf("Unexpected error on socket: %s", strerror(errno));
                  }
                  
                  removeSock = true;
                  removeSockHandle = currentSock->handleFd;
                  
                  // zero bytes read means EOF
                  notifyEvent.tag = currentSock->handleFd.getHandle();
                  notifyEvent.state = Net::Disconnected;
                  notifyEvent.tag = currentSock->handleFd.getHandle();
                  Game->postEvent(notifyEvent);
               }
            }
            else if (err != Net::NoError && err != Net::WouldBlock)
            {
               Con::errorf("Error reading from socket: %s",  strerror(errno));
               
               removeSock = true;
               removeSockHandle = currentSock->handleFd;
               
               notifyEvent.state = Net::Disconnected;
               notifyEvent.tag = currentSock->handleFd.getHandle();
               Game->postEvent(notifyEvent);
            }
            break;
         case PolledSocket::NameLookupRequired:
            U32 newState;
            
            // is the lookup complete?
            if (!gNetAsync.checkLookup(
                                       currentSock->handleFd, &out_h_addr, &out_h_length,
                                       sizeof(out_h_addr)))
               break;
            
            if (out_h_length == -1)
            {
               Con::errorf("DNS lookup failed: %s", currentSock->remoteAddr);
               notifyEvent.state = Net::DNSFailed;
               newState = Net::DNSFailed;
               removeSock = true;
               removeSockHandle = currentSock->handleFd;
            }
            else
            {
               // try to connect
               out_h_addr.port = currentSock->remotePort;
               const sockaddr *ai_addr = NULL;
               int ai_addrlen = 0;
               sockaddr_in socketAddress;
               sockaddr_in6 socketAddress6;
               
               if (out_h_addr.type == NetAddress::IPAddress)
               {
                  ai_addr = (const sockaddr*)&socketAddress;
                  ai_addrlen = sizeof(socketAddress);
                  NetAddressToIPSocket(&out_h_addr, &socketAddress);
                  
                  currentSock->fd = PlatformNetState::smReservedSocketList.activate(currentSock->handleFd, AF_INET, false);
                  setBlocking(currentSock->handleFd, false);
                  
#ifdef TORQUE_DEBUG_LOOKUPS
                  char addrString[256];
                  NetAddress addr;
                  IPSocketToNetAddress(&socketAddress, &addr);
                  Net::addressToString(&addr, addrString);
                  Con::printf("DNS: lookup resolved to %s", addrString);
#endif
               }
               else if (out_h_addr.type == NetAddress::IPV6Address)
               {
                  ai_addr = (const sockaddr*)&socketAddress6;
                  ai_addrlen = sizeof(socketAddress6);
                  NetAddressToIPSocket6(&out_h_addr, &socketAddress6);
                  
                  currentSock->fd = PlatformNetState::smReservedSocketList.activate(currentSock->handleFd, AF_INET6, false);
                  setBlocking(currentSock->handleFd, false);
                  
#ifdef TORQUE_DEBUG_LOOKUPS
                  char addrString[256];
                  NetAddress addr;
                  IPSocket6ToNetAddress(&socketAddress6, &addr);
                  Net::addressToString(&addr, addrString);
                  Con::printf("DNS: lookup resolved to %s", addrString);
#endif
               }
               else
               {
                  Con::errorf("Error connecting to %s: Invalid Protocol",
                              currentSock->remoteAddr);
                  notifyEvent.state = Net::ConnectFailed;
                  newState = Net::ConnectFailed;
                  removeSock = true;
                  removeSockHandle = currentSock->handleFd;
               }
               
               if (ai_addr)
               {
                  if (::connect(currentSock->fd, ai_addr,
                                ai_addrlen) == -1)
                  {
                     err = PlatformNetState::getLastError();
                     if (err != Net::WouldBlock)
                     {
                        Con::errorf("Error connecting to %s: %u",
                                    currentSock->remoteAddr, err);
                        notifyEvent.state = Net::ConnectFailed;
                        newState = Net::ConnectFailed;
                        removeSock = true;
                        removeSockHandle = currentSock->handleFd;
                     }
                     else
                     {
                        notifyEvent.state = Net::DNSResolved;
                        newState = Net::DNSResolved;
                        currentSock->state = PolledSocket::ConnectionPending;
                     }
                  }
                  else
                  {
                     notifyEvent.state = Net::Connected;
                     newState = Net::Connected;
                     currentSock->state = Connected;
                  }
               }
            }
            notifyEvent.tag = currentSock->handleFd.getHandle();
            Game->postEvent(notifyEvent);
            break;
         case PolledSocket::Listening:
            
            incomingHandleFd = Net::accept(currentSock->handleFd, &acceptEvent.address);
            if(incomingHandleFd != NetSocket::INVALID)
            {
               setBlocking(incomingHandleFd, false);
               addPolledSocket(incomingHandleFd, PlatformNetState::smReservedSocketList.resolve(incomingHandleFd), Connected);
               acceptEvent.portTag = currentSock->handleFd.getHandle();
               acceptEvent.connectionTag = incomingHandleFd.getHandle();
               Game->postEvent(acceptEvent);
            }
            break;
      }
      
      // only increment index if we're not removing the connection,  since
      // the removal will shift the indices down by one
      if (removeSock)
         closeConnectTo(removeSockHandle);
      else
         i++;
   }
   #endif
}

void Net::processListenSocket(NetSocket socketHandle)
{
   #if TOFIX
   if (socketHandle == NetSocket::INVALID)
      return;
   PacketReceiveEvent receiveEvent;
   
   sockaddr_storage sa;
   sa.ss_family = AF_UNSPEC;
   
   SOCKET socketFd = PlatformNetState::smReservedSocketList.resolve(socketHandle);
   
   for (;;)
   {
      socklen_t addrLen = sizeof(sa);
      S32 bytesRead = -1;
      
      if (socketHandle != NetSocket::INVALID)
         bytesRead = ::recvfrom(socketFd, (char *)receiveEvent.data, MaxPacketDataSize, 0, (struct sockaddr*)&sa, &addrLen);
      
      if (bytesRead == -1)
         break;
      
      if (sa.ss_family == AF_INET)
         IPSocketToNetAddress((sockaddr_in *)&sa, &receiveEvent.sourceAddress);
      else if (sa.ss_family == AF_INET6)
         IPSocket6ToNetAddress((sockaddr_in6 *)&sa, &receiveEvent.sourceAddress);
      else
         continue;
      
      if (bytesRead <= 0)
         continue;
      
      if (receiveEvent.sourceAddress.type == NetAddress::IPAddress &&
          receiveEvent.sourceAddress.address.ipv4.netNum[0] == 127 &&
          receiveEvent.sourceAddress.address.ipv4.netNum[1] == 0 &&
          receiveEvent.sourceAddress.address.ipv4.netNum[2] == 0 &&
          receiveEvent.sourceAddress.address.ipv4.netNum[3] == 1 &&
          receiveEvent.sourceAddress.port == PlatformNetState::netPort)
         continue;
      
      receiveEvent.size = PacketReceiveEventHeaderSize + bytesRead;
      Game->postEvent(receiveEvent);
   }
   #endif
}

NetSocket Net::openSocket()
{
   return PlatformNetState::smReservedSocketList.reserve();
}

Net::Error Net::closeSocket(NetSocket handleFd)
{
   if(handleFd != NetSocket::INVALID)
   {
      SOCKET socketFd = PlatformNetState::smReservedSocketList.resolve(handleFd);
      PlatformNetState::smReservedSocketList.remove(handleFd);
      
      if(!::closesocket(socketFd))
         return NoError;
      else
         return PlatformNetState::getLastError();
   }
   else
      return NotASocket;
}

Net::Error Net::connect(NetSocket handleFd, const NetAddress *address)
{
   if(!(address->type == NetAddress::IPAddress || address->type == NetAddress::IPV6Address))
      return WrongProtocolType;
   
   SOCKET socketFd = PlatformNetState::smReservedSocketList.resolve(handleFd);
   
   if (address->type == NetAddress::IPAddress)
   {
      sockaddr_in socketAddress;
      NetAddressToIPSocket(address, &socketAddress);
      
      if (socketFd == InvalidSocketHandle)
      {
         socketFd = PlatformNetState::smReservedSocketList.activate(handleFd, AF_INET, false);
      }
      
      if (!::connect(socketFd, (struct sockaddr *) &socketAddress, sizeof(socketAddress)))
         return NoError;
   }
   else if (address->type == NetAddress::IPV6Address)
   {
      sockaddr_in6 socketAddress;
      NetAddressToIPSocket6(address, &socketAddress);
      
      if (socketFd == InvalidSocketHandle)
      {
         socketFd = PlatformNetState::smReservedSocketList.activate(handleFd, AF_INET6, false);
      }
      
      if (!::connect(socketFd, (struct sockaddr *) &socketAddress, sizeof(socketAddress)))
         return NoError;
   }
   
   return PlatformNetState::getLastError();
}

Net::Error Net::listen(NetSocket handleFd, S32 backlog)
{
   SOCKET socketFd = PlatformNetState::smReservedSocketList.resolve(handleFd);
   if (socketFd == InvalidSocketHandle)
      return NotASocket;
   
   if(!::listen(socketFd, backlog))
      return NoError;
   return PlatformNetState::getLastError();
}

NetSocket Net::accept(NetSocket handleFd, NetAddress *remoteAddress)
{
   sockaddr_storage addr;
   socklen_t addrLen = sizeof(addr);
   
   SOCKET socketFd = PlatformNetState::smReservedSocketList.resolve(handleFd);
   if (socketFd == InvalidSocketHandle)
      return NetSocket::INVALID;
   
   SOCKET acceptedSocketFd = ::accept(socketFd, (sockaddr *)&addr, &addrLen);
   if (acceptedSocketFd != InvalidSocketHandle)
   {
      if (addr.ss_family == AF_INET)
      {
         // ipv4
         IPSocketToNetAddress(((struct sockaddr_in*)&addr), remoteAddress);
      }
      else if (addr.ss_family == AF_INET6)
      {
         // ipv6
         IPSocket6ToNetAddress(((struct sockaddr_in6*)&addr), remoteAddress);
      }
      
      NetSocket newHandleFd = PlatformNetState::smReservedSocketList.reserve(acceptedSocketFd);
      return newHandleFd;
   }
   
   return NetSocket::INVALID;
}

Net::Error Net::bindAddress(const NetAddress &address, NetSocket handleFd, bool useUDP)
{
   int error = 0;
   sockaddr_storage socketAddress;
   
   memset(&socketAddress, '\0', sizeof(socketAddress));
   
   SOCKET socketFd = PlatformNetState::smReservedSocketList.resolve(handleFd);
   if (socketFd == InvalidSocketHandle)
   {
      if (handleFd.getHandle() == -1)
         return NotASocket;
   }
   
   if (address.type == NetAddress::IPAddress)
   {
      socketFd = PlatformNetState::smReservedSocketList.activate(handleFd, AF_INET, useUDP);
      NetAddressToIPSocket(&address, (struct sockaddr_in*)&socketAddress);
      error = ::bind(socketFd, (struct sockaddr*)&socketAddress, sizeof(sockaddr_in));
   }
   else if (address.type == NetAddress::IPV6Address)
   {
      socketFd = PlatformNetState::smReservedSocketList.activate(handleFd, AF_INET6, useUDP);
      NetAddressToIPSocket6(&address, (struct sockaddr_in6*)&socketAddress);
      error = ::bind(socketFd, (struct sockaddr*)&socketAddress, sizeof(sockaddr_in6));
   }
   
   if (!error)
      return NoError;
   return PlatformNetState::getLastError();
}

Net::Error Net::setBufferSize(NetSocket handleFd, S32 bufferSize)
{
   S32 error;
   SOCKET socketFd = PlatformNetState::smReservedSocketList.resolve(handleFd);
   if (socketFd == InvalidSocketHandle)
      return NotASocket;
   
   error = ::setsockopt(socketFd, SOL_SOCKET, SO_RCVBUF, (char *)  &bufferSize, sizeof(bufferSize));
   if(!error)
      error = ::setsockopt(socketFd, SOL_SOCKET, SO_SNDBUF, (char *)  &bufferSize, sizeof(bufferSize));
   if(!error)
      return NoError;
   return PlatformNetState::getLastError();
}

Net::Error Net::setBroadcast(NetSocket handleFd, bool broadcast)
{
   S32 bc = broadcast;
   SOCKET socketFd = PlatformNetState::smReservedSocketList.resolve(handleFd);
   if (socketFd == InvalidSocketHandle)
      return NotASocket;
   S32 error = ::setsockopt(socketFd, SOL_SOCKET, SO_BROADCAST, (char*)&bc,  sizeof(bc));
   if(!error)
      return NoError;
   return PlatformNetState::getLastError();
}

Net::Error Net::setBlocking(NetSocket handleFd, bool blockingIO)
{
   SOCKET socketFd = PlatformNetState::smReservedSocketList.resolve(handleFd);
   if (socketFd == InvalidSocketHandle)
      return NotASocket;
   
   unsigned long notblock = !blockingIO;
   S32 error = ioctl(socketFd, FIONBIO, &notblock);
   if(!error)
      return NoError;
   return PlatformNetState::getLastError();
}

Net::Error Net::getListenAddress(const NetAddress::Type type, NetAddress *address, bool forceDefaults)
{
   if (type == NetAddress::IPAddress)
   {
      const char* serverIP = forceDefaults ? NULL : Con::getVariable("pref::Net::BindAddress");
      if (!serverIP || serverIP[0] == '\0')
      {
         address->type = type;
         address->port = 0;
         *((U32*)address->address.ipv4.netNum) = INADDR_ANY;
         return Net::NoError;
      }
      else
      {
         return Net::stringToAddress(serverIP, address, false);
      }
   }
   else if (type == NetAddress::IPBroadcastAddress)
   {
      address->type = type;
      address->port = 0;
      *((U32*)address->address.ipv4.netNum) = INADDR_BROADCAST;
      return Net::NoError;
   }
   else if (type == NetAddress::IPV6Address)
   {
      const char* serverIP6 = forceDefaults ? NULL : Con::getVariable("pref::Net::BindAddress6");
      if (!serverIP6 || serverIP6[0] == '\0')
      {
         sockaddr_in6 addr;
         memset(&addr, '\0', sizeof(addr));
         
         addr.sin6_port = 0;
         addr.sin6_addr = in6addr_any;
         
         IPSocket6ToNetAddress(&addr, address);
         return Net::NoError;
      }
      else
      {
         return Net::stringToAddress(serverIP6, address, false);
      }
   }
   else if (type == NetAddress::IPV6MulticastAddress)
   {
      const char* multicastAddressValue = forceDefaults ? NULL : Con::getVariable("pref::Net::Multicast6Address");
      if (!multicastAddressValue || multicastAddressValue[0] == '\0')
      {
         multicastAddressValue = TORQUE_NET_DEFAULT_MULTICAST_ADDRESS;
      }
      
      return Net::stringToAddress(multicastAddressValue, address, false);
   }
   else
   {
      return Net::WrongProtocolType;
   }
}

void Net::getIdealListenAddress(NetAddress *address)
{
   memset(address, '\0', sizeof(NetAddress));
   
   if (Net::smIpv6Enabled)
   {
      if (Net::getListenAddress(NetAddress::IPV6Address, address) == NeedHostLookup)
      {
         Net::getListenAddress(NetAddress::IPV6Address, address, true);
      }
   }
   else
   {
      if (Net::getListenAddress(NetAddress::IPAddress, address) == NeedHostLookup)
      {
         Net::getListenAddress(NetAddress::IPAddress, address, true);
      }
   }
}

Net::Error Net::send(NetSocket handleFd, const U8 *buffer, S32 bufferSize, S32 *outBytesWritten)
{
   SOCKET socketFd = PlatformNetState::smReservedSocketList.resolve(handleFd);
   if (socketFd == InvalidSocketHandle)
      return NotASocket;
   
#if defined( TORQUE_OS_LINUX )
   // Poll for write status.  this blocks.  should really
   // do this in a separate thread or set it up so that the data can
   // get queued and sent later
   // JMQTODO
   Poll(socketFd, POLLOUT, 10000);
#endif
   
   errno = 0;
   S32 bytesWritten = ::send(socketFd, (const char*)buffer, bufferSize, 0);
   
   if (outBytesWritten)
   {
      *outBytesWritten = outBytesWritten < (void *)0 ? 0 : bytesWritten;
   }
   
   return PlatformNetState::getLastError();
}

Net::Error Net::recv(NetSocket handleFd, U8 *buffer, S32 bufferSize, S32  *bytesRead)
{
   SOCKET socketFd = PlatformNetState::smReservedSocketList.resolve(handleFd);
   if (socketFd == InvalidSocketHandle)
      return NotASocket;
   
   *bytesRead = ::recv(socketFd, (char*)buffer, bufferSize, 0);
   if(*bytesRead == -1)
      return PlatformNetState::getLastError();
   return NoError;
}

bool Net::compareAddresses(const NetAddress *a1, const NetAddress *a2)
{
   return a1->isSameAddressAndPort(*a2);
}

static inline int NetAddressTypeToIpType(NetAddress::Type natype)
{
   switch (natype)
   {
   case NetAddress::IPAddress:
   case NetAddress::IPBroadcastAddress:
      return AF_INET;
   case NetAddress::IPV6Address:
   case NetAddress::IPV6MulticastAddress:
      return AF_INET6;
   default:
      return AF_UNSPEC;
   }
}

Net::Error Net::stringToAddress(const char *addressString, NetAddress  *address, bool hostLookup, NetAddress::Type requiredType)
{
   char addr[256];
   int port = 0;
   NetAddress::Type actualType = NetAddress::Invalid;
   if (!PlatformNetState::extractAddressParts(addressString, addr, port, actualType))
   {
      return WrongProtocolType;
   }
   
   // Make sure family matches (in cast we have IP: stuff in address)
   if (requiredType != NetAddress::Invalid && actualType != NetAddress::Invalid && (actualType != requiredType))
   {
      return WrongProtocolType;
   }
   
   if (actualType == NetAddress::Invalid)
   {
      actualType = requiredType;
   }
   
   addressString = addr;
   memset(address, '\0', sizeof(NetAddress));
   
   if (!dStricmp(addressString, "broadcast"))
   {
      address->type = NetAddress::IPBroadcastAddress;
      if (!(actualType == NetAddress::Invalid || 
            actualType == NetAddress::IPAddress))
         return WrongProtocolType;
      
      if (port != 0)
         address->port = port;
      else
         address->port = PlatformNetState::defaultPort;
   }
   else if (!dStricmp(addressString, "multicast"))
   {
      address->type = NetAddress::IPV6MulticastAddress;
      if (!(actualType == NetAddress::Invalid || 
            actualType == NetAddress::IPV6Address))
         return WrongProtocolType;
      
      if (port != 0)
         address->port = port;
      else
         address->port = PlatformNetState::defaultPort;
   }
   else
   {
      sockaddr_in ipAddr;
      sockaddr_in6 ipAddr6;
      
      memset(&ipAddr, 0, sizeof(ipAddr));
      memset(&ipAddr6, 0, sizeof(ipAddr6));
      
      bool hasInterface = dStrchr(addressString, '%') != NULL; // if we have an interface, best use getaddrinfo to parse
      
      // Check if we've got a simple ipv4 / ipv6
      
      if (inet_pton(AF_INET, addressString, &ipAddr.sin_addr) == 1)
      {
         if (!(actualType == NetAddress::Invalid || 
               actualType == NetAddress::IPAddress))
               return WrongProtocolType;
         IPSocketToNetAddress(((struct sockaddr_in*)&ipAddr), address);
         
         if (port != 0)
            address->port = port;
         else
            address->port = PlatformNetState::defaultPort;
         
         return NoError;
      }
      else if (!hasInterface && inet_pton(AF_INET6, addressString, &ipAddr6.sin6_addr) == 1)
      {
         if (!(actualType == NetAddress::Invalid || 
               actualType == NetAddress::IPV6Address))
            return WrongProtocolType;
         IPSocket6ToNetAddress(((struct sockaddr_in6*)&ipAddr6), address);
         
         if (port != 0)
            address->port = port;
         else
            address->port = PlatformNetState::defaultPort;
         
         return NoError;
      }
      else
      {
         if (!hostLookup && !hasInterface)
            return NeedHostLookup;
         
         struct addrinfo hint, *res = NULL;
         memset(&hint, 0, sizeof(hint));
         hint.ai_family = NetAddressTypeToIpType(actualType);
         hint.ai_flags = hostLookup ? 0 : AI_NUMERICHOST;
         
         if (getaddrinfo(addressString, NULL, &hint, &res) == 0)
         {
            if (hint.ai_family != AF_UNSPEC)
            {
               // Prefer desired protocol
               res = PlatformNetState::pickAddressByProtocol(res, hint.ai_family);
            }
            
            if (res && res->ai_family == AF_INET)
            {
               // ipv4
               IPSocketToNetAddress(((struct sockaddr_in*)res->ai_addr), address);
            }
            else if (res && res->ai_family == AF_INET6)
            {
               // ipv6
               IPSocket6ToNetAddress(((struct sockaddr_in6*)res->ai_addr), address);
            }
            else
            {
               // unknown
               return UnknownError;
            }
            
            if (port != 0)
               address->port = port;
            else
               address->port = PlatformNetState::defaultPort;
         }
      }
   }
   
   return NoError;
}

void Net::addressToString(const NetAddress *address, char  addressString[256])
{
   if(address->type == NetAddress::IPAddress || address->type == NetAddress::IPBroadcastAddress)
   {
      sockaddr_in ipAddr;
      NetAddressToIPSocket(address, &ipAddr);
      
      if (ipAddr.sin_addr.s_addr == htonl(INADDR_BROADCAST) || address->type == NetAddress::IPBroadcastAddress)
      {
         if (ipAddr.sin_port == 0)
            dSprintf(addressString, 256, "IP:Broadcast");
         else
            dSprintf(addressString, 256, "IP:Broadcast:%d", ntohs(ipAddr.sin_port));
      }
      else
      {
         char buffer[256];
         buffer[0] = '\0';
         sockaddr_in ipAddr;
         NetAddressToIPSocket(address, &ipAddr);
         inet_ntop(AF_INET, &(ipAddr.sin_addr), buffer, sizeof(buffer));
         if (ipAddr.sin_port == 0)
            dSprintf(addressString, 256, "IP:%s", buffer);
         else
            dSprintf(addressString, 256, "IP:%s:%i", buffer, ntohs(ipAddr.sin_port));
      }
   }
   else if (address->type == NetAddress::IPV6Address)
   {
      char buffer[256];
      buffer[0] = '\0';
      sockaddr_in6 ipAddr;
      NetAddressToIPSocket6(address, &ipAddr);
      inet_ntop(AF_INET6, &(ipAddr.sin6_addr), buffer, sizeof(buffer));
      if (ipAddr.sin6_port == 0)
         dSprintf(addressString, 256, "IP6:%s", buffer);
      else
         dSprintf(addressString, 256, "IP6:[%s]:%i", buffer, ntohs(ipAddr.sin6_port));
   }
   else if (address->type == NetAddress::IPV6MulticastAddress)
   {
      if (address->port == 0)
         dSprintf(addressString, 256, "IP6:Multicast");
      else
         dSprintf(addressString, 256, "IP6:Multicast:%d", address->port);
   }
   else
   {
      *addressString = 0;
      return;
   }
}

void Net::enableMulticast()
{
   SOCKET socketFd;
   
   if (Net::smIpv6Enabled)
   {
      socketFd = PlatformNetState::smReservedSocketList.resolve(PlatformNetState::udp6Socket);
      
      if (socketFd != InvalidSocketHandle)
      {
         PlatformNetState::multicast6Socket = PlatformNetState::udp6Socket;
         
         Net::Error error = NoError;
         
         if (error == NoError)
         {
            unsigned long multicastTTL = 1;
            
            if (setsockopt(socketFd, IPPROTO_IPV6, IPV6_MULTICAST_HOPS,
                           (char*)&multicastTTL, sizeof(multicastTTL)) < 0)
            {
               error = PlatformNetState::getLastError();
            }
         }
         
         // Find multicast to bind to...
         
         NetAddress multicastAddress;
         sockaddr_in6 multicastSocketAddress;
         
         const char *multicastAddressValue = Con::getVariable("pref::Net::Multicast6Address");
         if (!multicastAddressValue || multicastAddressValue[0] == '\0')
         {
            multicastAddressValue = TORQUE_NET_DEFAULT_MULTICAST_ADDRESS;
         }
         
         error = Net::stringToAddress(multicastAddressValue, &multicastAddress, false);
         
         if (error == NoError)
         {
            memset(&PlatformNetState::multicast6Group, '\0', sizeof(&PlatformNetState::multicast6Group));
            NetAddressToIPSocket6(&multicastAddress, &multicastSocketAddress);
            memcpy(&PlatformNetState::multicast6Group.ipv6mr_multiaddr, &multicastSocketAddress.sin6_addr, sizeof(PlatformNetState::multicast6Group.ipv6mr_multiaddr));
         }
         
         // Setup group
         
         if (error == NoError)
         {
            const char *multicastInterface = Con::getVariable("pref::Net::Multicast6Interface");
            
            if (multicastInterface && multicastInterface[0] != '\0')
            {
#ifdef TORQUE_USE_WINSOCK
               PlatformNetState::multicast6Group.ipv6mr_interface = dAtoi(multicastInterface);
#else
               PlatformNetState::multicast6Group.ipv6mr_interface = if_nametoindex(multicastInterface);
#endif
            }
            else
            {
               PlatformNetState::multicast6Group.ipv6mr_interface = 0; // 0 == accept from any interface
            }
            
            if (PlatformNetState::multicast6Group.ipv6mr_interface && error == NoError)
            {
               if (setsockopt(socketFd, IPPROTO_IPV6, IPV6_MULTICAST_IF, (char *)&PlatformNetState::multicast6Group.ipv6mr_interface, sizeof(PlatformNetState::multicast6Group.ipv6mr_interface)) < 0)
               {
                  error = PlatformNetState::getLastError();
               }
            }
            
            if (error == NoError && setsockopt(socketFd, IPPROTO_IPV6, IPV6_JOIN_GROUP, (char*)&PlatformNetState::multicast6Group, sizeof(PlatformNetState::multicast6Group)) < 0)
            {
               error = PlatformNetState::getLastError();
            }
         }
         
         
         if (error == NoError)
         {
            NetAddress listenAddress;
            char listenAddressStr[256];
            Net::addressToString(&multicastAddress, listenAddressStr);
            Con::printf("Multicast initialized on %s", listenAddressStr);
         }
         
         if (error != NoError)
         {
            PlatformNetState::multicast6Socket = NetSocket::INVALID;
            Con::printf("Unable to multicast UDP - error %d", error);
         }
      }
   }
}

void Net::disableMulticast()
{
   if (PlatformNetState::multicast6Socket != NetSocket::INVALID)
   {
      PlatformNetState::multicast6Socket = NetSocket::INVALID;
   }
}

bool Net::isMulticastEnabled()
{
   return PlatformNetState::multicast6Socket != NetSocket::INVALID;
}

U32 NetAddress::getHash() const
{
   U32 value = 0;
   switch (type)
   {
      case NetAddress::IPAddress:
         value = hash((U8*)&address.ipv4.netNum, sizeof(address.ipv4.netNum), 0);
         break;
      case NetAddress::IPV6Address:
         value = hash((U8*)address.ipv6.netNum, sizeof(address.ipv6.netNum), 0);
         break;
      default:
         value = 0;
         break;
   }
   return value;
}

bool Net::isAddressTypeAvailable(NetAddress::Type addressType)
{
   switch (addressType)
   {
      case NetAddress::IPAddress:
         return PlatformNetState::udpSocket != NetSocket::INVALID;
      case NetAddress::IPV6Address:
         return PlatformNetState::udp6Socket != NetSocket::INVALID;
      case NetAddress::IPBroadcastAddress:
         return PlatformNetState::udpSocket != NetSocket::INVALID;
      case NetAddress::IPV6MulticastAddress:
         return PlatformNetState::multicast6Socket != NetSocket::INVALID;
      default:
         return false;
   }
}




#else



// STUB NETWORK

#define STUB_BUFFER_SIZE 65536

struct PlatformStubSocket
{
   U32 mAllocNumber;
   U8 mGeneration : 7;

   U8* mBuffer;
   U32 mHead;
   U32 mTail;
   NetAddress mAddress;
   bool mIsListening;

   enum
   {
      PacketMarker = 0xB0FCF0F1
   };

   PlatformStubSocket() : mAllocNumber(0), mGeneration(0)
   {
      reset();
      mBuffer = new U8[STUB_BUFFER_SIZE];
   }

   ~PlatformStubSocket()
   {
      delete[] mBuffer;
   }

   inline void reset()
   {
      mHead = mTail = 0;
      mIsListening = false;
      memset(&mAddress, 0, sizeof(NetAddress));
   }

   bool readPacket(NetAddress &origin, U8* outBuffer, U16* outSize)
   {
      U32 marker = 0;
      if (read(&marker, sizeof(U32)) != sizeof(U32))
      {
         return false;
      }

      if (marker != PacketMarker)
         return false;

      if (read(&origin, sizeof(NetAddress)) != sizeof(NetAddress))
      {
         return false;
      }

      if (read(outSize, sizeof(U16)) != sizeof(U16))
      {
         return false;
      }

      if (read(outBuffer, *outSize) != *outSize)
      {
         return false;
      }

      return true;
   }

   bool writePacket(NetAddress src, const U8 *data, size_t size)
   {
      U32 oldHead = mHead;

      U32 marker = PacketMarker;
      if (write(&marker, sizeof(U32)) != sizeof(U32))
      {
         return false;
      }

      if (write(&src, sizeof(NetAddress)) != sizeof(NetAddress))
      {
         mHead = oldHead;
         return false;
      }

      U16 sz = (U16)size;
      if (write(&sz, sizeof(U16)) != sizeof(U16))
      {
         mHead = oldHead;
         return false;
      }

      if (write(data, sz) != sz)
      {
         mHead = oldHead;
         return false;
      }

      return true;
   }

   S32 write(const void *data, size_t size)
   {
      const U8* cdata = (U8*)data;
       for (size_t i = 0; i < size; i++)
       {
           if ((mHead + 1) % STUB_BUFFER_SIZE == mTail)
           {
               // Full
               return i;
           }
           mBuffer[mHead] = cdata[i];
           mHead = (mHead + 1) % STUB_BUFFER_SIZE;
       }
       return size;
   }

   S32 read(void *data, size_t size)
   {
      U8* cdata = (U8*)data;
       for (size_t i = 0; i < size; i++)
       {
           if (mHead == mTail)
           {
               // Empty
               return i;
           }
           cdata[i] = mBuffer[mTail];
           mTail = (mTail + 1) % STUB_BUFFER_SIZE;
       }
       return size;
   }
};


namespace PlatformNetState
{
   static S32 initCount = 0;
   
   static const S32 defaultPort = 28000;
   static S32 netPort = 0;
   
   static NetSocket udpSocket = NetSocket::INVALID;

   typedef FreeListStruct<PlatformStubSocket, FreeListHandle::Basic32> SocketPool;

   SocketPool smSocketPool;
}


bool Net::init()
{
   return true;
}

void Net::shutdown()
{
}

//

// TCP listen handler; not needed
NetSocket Net::openListenPort(U16 port, NetAddress::Type addressType)
{
   return NetSocket::INVALID;
}

// TCP handler; not needed
NetSocket Net::openConnectTo(const char *addressString)
{
   return NetSocket::INVALID;
}

// TCP handler; not needed
void Net::closeConnectTo(NetSocket handleFd)
{
}

// TCP send handler; not needed
Net::Error Net::sendtoSocket(NetSocket handleFd, const U8 *buffer, S32  bufferSize, S32 *outBufferWritten)
{
   return NotASocket;
}

static U16 pickUnusedPort(NetAddress address)
{
   bool found = false;
   U16 portID = 0;

   Con::printf("pickUnusedPort");
   
   while (!found)
   {
      portID = (rand()+1) % 65536;
      address.port = portID;
   Con::printf("port %u", portID);
      
      auto itr = std::find_if(PlatformNetState::smSocketPool.mItems.begin(), PlatformNetState::smSocketPool.mItems.end(), [address](PlatformStubSocket& socket) {
         return socket.mAllocNumber != 0 && socket.mAddress.isEqual(address);
      });
      
      if (itr == PlatformNetState::smSocketPool.mItems.end())
      {
         found = true;
      }
   }
   return portID;
}

bool Net::openPort(S32 port, bool doBind)
{
   if (PlatformNetState::udpSocket != NetSocket::INVALID)
   {
      closeSocket(PlatformNetState::udpSocket);
      PlatformNetState::udpSocket = NetSocket::INVALID;
   }
   
   NetAddress address;
   NetAddress listenAddress;
   char listenAddressStr[256];

   if (Net::getListenAddress(NetAddress::IPAddress, &address) == Net::NoError)
   {

      // Alloc new socket
      PlatformStubSocket* socketPtr = NULL;
      printf("SocketPool::allocItem\n");
      FreeListHandle::Basic32 ret = PlatformNetState::smSocketPool.allocItem(&socketPtr);
      printf("SocketPool::allocItem DONE\n");
      if (port == 0)
         address.port = pickUnusedPort(address);
      
      socketPtr->mAddress = address;
      PlatformNetState::udpSocket.setHandle(ret.value);
      
      Net::Error error = NoError;
      if (doBind)
      {
         error = bindAddress(address, PlatformNetState::udpSocket, true);
      }
      
      if (error == NoError)
      {
         listenAddress = socketPtr->mAddress;
         if (error == NoError)
         {
            Net::addressToString(&listenAddress, listenAddressStr);
            Con::printf("UDP initialized on ipv4 %s", listenAddressStr);
         }
      }
      
      if (error != NoError)
      {
         closeSocket(PlatformNetState::udpSocket);
         PlatformNetState::udpSocket = NetSocket::INVALID;
         Con::printf("Unable to initialize UDP on ipv4 - error %d", error);
      }
   }
   else
   {
      Con::errorf("Unable to initialize UDP on ipv4 - invalid address.");
      PlatformNetState::udpSocket = NetSocket::INVALID;
      return false;
   }

   return true;
}

NetSocket Net::getPort()
{
   return PlatformNetState::udpSocket;
}

void Net::closePort()
{
   if (PlatformNetState::udpSocket != NetSocket::INVALID)
      closeSocket(PlatformNetState::udpSocket);
}

Net::Error Net::sendto(const NetAddress *address, const U8 *buffer, S32 bufferSize)
{
   // Find corresponding socket for address
   auto itr = std::find_if(PlatformNetState::smSocketPool.mItems.begin(), PlatformNetState::smSocketPool.mItems.end(), [address](PlatformStubSocket& socket) {
      return socket.mAllocNumber != 0 && socket.mAddress.isEqual(*address);
   });
   
   PlatformStubSocket* outSocket = itr != PlatformNetState::smSocketPool.mItems.end() ? itr : NULL;
   PlatformStubSocket* serverSocket = PlatformNetState::smSocketPool.getItem(PlatformNetState::udpSocket.getHandle());

   if (outSocket == NULL || serverSocket == NULL)
      return NoError;

   // Send packet to inbox of socket
   if (!outSocket->writePacket(serverSocket->mAddress, buffer, bufferSize))
      return NoError;

   return NoError;
}

void Net::process()
{
   processListenSocket(PlatformNetState::udpSocket);
}

void Net::processListenSocket(NetSocket socketHandle)
{
   #if TOFIX
   if (socketHandle == NetSocket::INVALID)
      return;
   PacketReceiveEvent receiveEvent;

   for (;;)
   {
      // We store the packet header in the ring buffer in this case
      PlatformStubSocket* inSocket = PlatformNetState::smSocketPool.getItem(socketHandle.getHandle());
      if (inSocket == NULL)
         continue;

      U16 bytesRead = 0;
      if (!inSocket->readPacket(receiveEvent.sourceAddress, (U8 *)receiveEvent.data, &bytesRead))
         break;
      
      if (bytesRead == 0)
         break;
      
      if (receiveEvent.sourceAddress.type == NetAddress::IPAddress &&
          receiveEvent.sourceAddress.address.ipv4.netNum[0] == 127 &&
          receiveEvent.sourceAddress.address.ipv4.netNum[1] == 0 &&
          receiveEvent.sourceAddress.address.ipv4.netNum[2] == 0 &&
          receiveEvent.sourceAddress.address.ipv4.netNum[3] == 1 &&
          receiveEvent.sourceAddress.port == PlatformNetState::netPort)
         continue;
      
      receiveEvent.size = PacketReceiveEventHeaderSize + bytesRead;
      Game->postEvent(receiveEvent);
   }
   #endif
}

// TCP socket handler; not needed
NetSocket Net::openSocket()
{
   return NetSocket::INVALID;
}

// TCP socket handler; not needed
Net::Error Net::closeSocket(NetSocket handleFd)
{
   return NotASocket;
}

// TCP socket handler; not needed
Net::Error Net::connect(NetSocket handleFd, const NetAddress *address)
{
   return NoError;
}

// Enabled listening on TCP/UDP socket
Net::Error Net::listen(NetSocket handleFd, S32 backlog)
{
   // Grab socket
   PlatformStubSocket* inSocket = PlatformNetState::smSocketPool.getItem(handleFd.getHandle());
   if (inSocket == NULL)
      return NotASocket;

   // All stub sockets listen by default
   return NoError;
}

// TCP socket handler; not needed
NetSocket Net::accept(NetSocket handleFd, NetAddress *remoteAddress)
{
   return NetSocket::INVALID;
}

// Binds UDP/TCP socket address
Net::Error Net::bindAddress(const NetAddress &address, NetSocket handleFd, bool useUDP)
{
   // Grab socket
   PlatformStubSocket* inSocket = PlatformNetState::smSocketPool.getItem(handleFd.getHandle());
   if (inSocket == NULL)
      return NotASocket;

   // See if address is free

   auto itr = std::find_if(PlatformNetState::smSocketPool.mItems.begin(), PlatformNetState::smSocketPool.mItems.end(), [address](PlatformStubSocket& socket) {
      return socket.mAllocNumber != 0 && socket.mIsListening == true && socket.mAddress.getIPV4Code() == address.getIPV4Code() && socket.mAddress.port == address.port;
   });

   if (itr != PlatformNetState::smSocketPool.mItems.end())
   {
      Con::printf("Unable to open listen port %u, already used", address.port);
      return NotASocket;
   }

   // Set it!
   inSocket->mAddress = address;
   inSocket->mIsListening = true;
   return NoError;
}

Net::Error Net::setBufferSize(NetSocket handleFd, S32 bufferSize)
{
   return NoError;
}

Net::Error Net::setBroadcast(NetSocket handleFd, bool broadcast)
{
   return NoError;
}

Net::Error Net::setBlocking(NetSocket handleFd, bool blockingIO)
{
   return NotASocket;
}

Net::Error Net::getListenAddress(const NetAddress::Type type, NetAddress *address, bool forceDefaults)
{
   if (type == NetAddress::IPAddress)
   {
      const char* serverIP = forceDefaults ? NULL : Con::getVariable("pref::Net::BindAddress");
      if (!serverIP || serverIP[0] == '\0')
      {
         address->type = type;
         address->port = PlatformNetState::defaultPort;
         *((U32*)address->address.ipv4.netNum) = 0x0; // INADDR_ANY
         return Net::NoError;
      }
      else
      {
         return Net::stringToAddress(serverIP, address, false);
      }
   }
   else
   {
      return Net::WrongProtocolType;
   }
}

void Net::getIdealListenAddress(NetAddress *address)
{
   memset(address, '\0', sizeof(NetAddress));
   if (Net::getListenAddress(NetAddress::IPAddress, address) == NeedHostLookup)
   {
      Net::getListenAddress(NetAddress::IPAddress, address, true);
   }
}

// TCP send
Net::Error Net::send(NetSocket handleFd, const U8 *buffer, S32 bufferSize, S32 *outBytesWritten)
{
   return NotASocket;
}

// TCP recv
Net::Error Net::recv(NetSocket handleFd, U8 *buffer, S32 bufferSize, S32  *bytesRead)
{
   return NotASocket;
}

bool Net::compareAddresses(const NetAddress *a1, const NetAddress *a2)
{
   return a1->isSameAddressAndPort(*a2);
}

Net::Error Net::stringToAddress(const char *addressString, NetAddress  *address, bool hostLookup, NetAddress::Type requiredType)
{
   char addr[256];
   int port = 0;
   NetAddress::Type actualType = NetAddress::Invalid;
   if (!PlatformNetState::extractAddressParts(addressString, addr, port, actualType))
   {
      return WrongProtocolType;
   }
   
   // Make sure family matches (in cast we have IP: stuff in address)
   if (requiredType != NetAddress::Invalid && actualType != NetAddress::Invalid && (actualType != requiredType))
   {
      return WrongProtocolType;
   }
   
   if (actualType == NetAddress::Invalid)
   {
      actualType = requiredType;
   }
   
   addressString = addr;
   memset(address, '\0', sizeof(NetAddress));
   
   if (!dStricmp(addressString, "broadcast"))
   {
      address->type = NetAddress::IPBroadcastAddress;
      if (!(actualType == NetAddress::Invalid || actualType == NetAddress::IPAddress))
         return WrongProtocolType;
      
      if (port != 0)
         address->port = port;
      else
         address->port = PlatformNetState::defaultPort;
   }
   else if (!dStricmp(addressString, "multicast"))
   {
      address->type = NetAddress::IPV6MulticastAddress;
      if (!(actualType == NetAddress::Invalid || actualType == NetAddress::IPV6Address))
         return WrongProtocolType;
      
      if (port != 0)
         address->port = port;
      else
         address->port = PlatformNetState::defaultPort;
   }
   else
   {
      // Just do ipv4 here
      U32 parts[4] = {};
      int result = sscanf(addr, "%u.%u.%u.%u", &parts[0], &parts[1], &parts[2], &parts[3]);
      for (U32 i=0; i<4; i++)
      {
         address->address.ipv4.netNum[i] = parts[i];
      }

      address->type = NetAddress::IPAddress;
      
      if (port != 0)
         address->port = port;
      else
         address->port = PlatformNetState::defaultPort;
   }
   
   return NoError;
}

void Net::addressToString(const NetAddress *address, char  addressString[256])
{
   if(address->type == NetAddress::IPAddress || address->type == NetAddress::IPBroadcastAddress)
   {  
      if (address->getIPV4Code() == 0xffffffff || address->type == NetAddress::IPBroadcastAddress)
      {
         if (address->port == 0)
            dSprintf(addressString, 256, "IP:Broadcast");
         else
            dSprintf(addressString, 256, "IP:Broadcast:%d", address->port);
      }
      else
      {
         if (address->port == 0)
            dSprintf(addressString, 256, "IP:%u.%u.%u.%u",
               address->address.ipv4.netNum[0], address->address.ipv4.netNum[1],
               address->address.ipv4.netNum[2], address->address.ipv4.netNum[3]);
         else
            dSprintf(addressString, 256, "IP:%u.%u.%u.%u:%u",
               address->address.ipv4.netNum[0], address->address.ipv4.netNum[1],
               address->address.ipv4.netNum[2], address->address.ipv4.netNum[3], address->port);
      }
   }
   else
   {
      *addressString = 0;
      return;
   }
}

void Net::enableMulticast()
{
}

void Net::disableMulticast()
{
}

bool Net::isMulticastEnabled()
{
   return false;
}

U32 NetAddress::getHash() const
{
   U32 value = 0;
   switch (type)
   {
      case NetAddress::IPAddress:
         value = hash((U8*)&address.ipv4.netNum, sizeof(address.ipv4.netNum), 0);
         break;
      default:
         value = 0;
         break;
   }
   return value;
}

bool Net::isAddressTypeAvailable(NetAddress::Type addressType)
{
   switch (addressType)
   {
      case NetAddress::IPAddress:
         return PlatformNetState::udpSocket != NetSocket::INVALID;
      default:
         return false;
   }
}


#endif


