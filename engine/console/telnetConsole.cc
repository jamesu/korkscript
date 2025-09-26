//-----------------------------------------------------------------------------
// Copyright (c) 2013 GarageGames, LLC
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

#include "platform/platform.h"
#include "embed/api.h"
#include "embed/internalApi.h"
#include "console/consoleNamespace.h"
#include "console/telnetConsole.h"
#include "platform/platformNetwork.h"

#if TOFIX

ConsoleFunction( telnetSetParameters, void, 4, 5, "(int port, string consolePass, string listenPass, bool remoteEcho)"
                "Initialize and open the telnet console.\n\n"
                "@param port        Port to listen on for console connections (0 will shut down listening).\n"
                "@param consolePass Password for read/write access to console.\n"
                "@param listenPass  Password for read access to console."
                "@param remoteEcho  [optional] Enable echoing back to the client, off by default.")
{
   if (TelConsole)
   {
	   TelConsole->setTelnetParameters(dAtoi(argv[1]), argv[2], argv[3], argc == 5 ? dAtob( argv[4] ) : false);
   }
}
#endif

static void telnetCallback(U32 level, const char *consoleLine, void* userPtr)
{
   TelnetConsole* con = (TelnetConsole*)userPtr;
   
   if (con)
   {
      con->processConsoleLine(consoleLine);
   }
}

TelnetConsole::TelnetConsole(KorkApi::VmInternal* vm)
{
   mVMInternal = vm;
   mVMInternal->mConfig.telnetLogFn = telnetCallback;
   mVMInternal->mConfig.telnetLogUser = this;

   mAcceptPort = -1;
   mClientList = NULL;
   mRemoteEchoEnabled = false;

   mValid = mVMInternal->mConfig.iTelnet.StartListenFn != NULL &&
   mVMInternal->mConfig.iTelnet.StopListenFn != NULL &&
   mVMInternal->mConfig.iTelnet.CheckSocketActiveFn != NULL &&
   mVMInternal->mConfig.iTelnet.CheckAcceptFn != NULL &&
   mVMInternal->mConfig.iTelnet.CheckListenFn != NULL &&
   mVMInternal->mConfig.iTelnet.SendDataFn != NULL &&
   mVMInternal->mConfig.iTelnet.RecvDataFn != NULL;
}

TelnetConsole::~TelnetConsole()
{
   if (mVMInternal->mConfig.telnetLogUser == this)
   {
      mVMInternal->mConfig.telnetLogFn = NULL;
      mVMInternal->mConfig.telnetLogUser = NULL;
   }

   if (mValid)
   {
      mVMInternal->mConfig.iTelnet.StopListenFn(mVMInternal->mConfig.telnetUser, KorkApi::TELNET_CONSOLE);
   }
}

void TelnetConsole::setTelnetParameters(S32 port, const char *telnetPassword, const char *listenPassword, bool remoteEcho)
{
   if (port == mAcceptPort || !mValid)
      return;
   
   mRemoteEchoEnabled = remoteEcho;

   if (mVMInternal->mConfig.iTelnet.StartListenFn(mVMInternal->mConfig.telnetUser, KorkApi::TELNET_CONSOLE, port))
   {
      mAcceptPort = port;
   }
   else
   {
      mAcceptPort = -1;
   }

   dStrncpy(mTelnetPassword, telnetPassword, PasswordMaxLength);
   dStrncpy(mListenPassword, listenPassword, PasswordMaxLength);
}

void TelnetConsole::processConsoleLine(const char *consoleLine)
{
   if (mClientList==NULL || !mValid) return;  // just escape early.  don't even do another step...

   // ok, spew this line out to all our subscribers...
   S32 len = dStrlen(consoleLine)+1;
   for(TelnetClient *walk = mClientList; walk; walk = walk->nextClient)
   {
      if (walk->state == FullAccessConnected || walk->state == ReadOnlyConnected)
      {
         if (walk->socket != 0)
         {
            mVMInternal->mConfig.iTelnet.SendDataFn(mVMInternal->mConfig.telnetUser, walk->socket, len, (const unsigned char*)consoleLine);
            mVMInternal->mConfig.iTelnet.SendDataFn(mVMInternal->mConfig.telnetUser, walk->socket, 2, (const unsigned char*)"\r\n");
         }
      }
   }
}

void TelnetConsole::process()
{
   NetAddress address;
   KorkApi::Config& cfg = mVMInternal->mConfig;
   KorkApi::TelnetInterface& tel = cfg.iTelnet;

   if (!mValid)
   {
      return;
   }
   
   if(mAcceptPort != -1)
   {
      // ok, see if we have any new connections:
      U32 newConnection = tel.CheckAcceptFn(cfg.telnetUser, KorkApi::TELNET_CONSOLE);
      
      if(newConnection != 0)
      {
         char buffer[256];
         tel.GetSocketAddressFn(cfg.telnetUser, newConnection, buffer);

         printf("Telnet connection from %s", buffer);
         
         TelnetClient *cl = new TelnetClient;
         cl->socket = newConnection;
         cl->curPos = 0;
#if defined(TORQUE_SHIPPING) && defined(TORQUE_DISABLE_TELNET_CONSOLE_PASSWORD)
         // disable the password in a ship build? WTF.  lets make an error:
         PleaseMakeSureYouKnowWhatYouAreDoingAndCommentOutThisLineIfSo.
#elif !defined(TORQUE_SHIPPING) && defined(TORQUE_DISABLE_TELNET_CONSOLE_PASSWORD)
         cl->state = FullAccessConnected;
#else
         cl->state = PasswordTryOne;
#endif
         
         const char *prompt = "";// TOFIX Con::getVariable("Con::Prompt");
         char connectMessage[1024];
         dSprintf(connectMessage, sizeof(connectMessage),
                  "Torque Telnet Remote Console\r\n\r\n%s",
                  cl->state == FullAccessConnected ? prompt : "Enter Password:");
         
         if (cl->socket != 0)
         {
            tel.SendDataFn(cfg.telnetUser, cl->socket, dStrlen(connectMessage)+1, (const unsigned char*)connectMessage);
         }

         cl->nextClient = mClientList;
         mClientList = cl;
      }
   }
   else if (!tel.CheckListenFn(cfg.telnetUser, KorkApi::TELNET_CONSOLE))
   {
      disconnect();
   }
   
   char recvBuf[256];
   char reply[1024];
   
   // see if we have any input to process...
   
   for(TelnetClient *client = mClientList; client; client = client->nextClient)
   {
      U32 numBytes = 0;
      if (!tel.RecvDataFn(cfg.telnetUser, client->socket, (unsigned char*)recvBuf, sizeof(recvBuf), &numBytes))
      {
         tel.StopSocketFn(cfg.telnetUser, client->socket);
         client->socket = 0;
         continue;
      }

      if (numBytes == 0)
      {
         continue;
      }
      
      S32 replyPos = 0;
      for(S32 i = 0; i < numBytes;i++)
      {
         if(recvBuf[i] == '\r')
            continue;
         // execute the current command
         
         if(recvBuf[i] == '\n')
         {
            reply[replyPos++] = '\r';
            reply[replyPos++] = '\n';
            
            client->curLine[client->curPos] = 0;
            client->curPos = 0;
            
            if (client->state == FullAccessConnected)
            {
               if (client->socket != 0)
               {
                  tel.SendDataFn(cfg.telnetUser, client->socket, replyPos, (const unsigned char*)reply);
               }
               replyPos = 0;
               
               if (cfg.iTelnet.QueueEvaluateFn)
               {
                  tel.QueueEvaluateFn(cfg.telnetUser, client->curLine);
               }
               
               // note - send prompt next
               KorkApi::ConsoleValue promptV = mVMInternal->mVM->getGlobalVariable(StringTable->insert("Con::Prompt"));
               const char* prompt = mVMInternal->valueAsString(promptV);

               if (client->socket != 0)
               {
                  tel.SendDataFn(cfg.telnetUser, client->socket, dStrlen(prompt), (const unsigned char*)prompt);
               }
            }
            else if(client->state == ReadOnlyConnected)
            {
               if (client->socket != 0)
               {
                  tel.SendDataFn(cfg.telnetUser, client->socket, replyPos, (const unsigned char*)reply);
               }
               replyPos = 0;
            }
            else
            {
               client->state++;
               if(!dStrncmp(client->curLine, mTelnetPassword, PasswordMaxLength))
               {
                  if (client->socket != 0)
                  {
                     tel.SendDataFn(cfg.telnetUser, client->socket, replyPos, (const unsigned char*)reply);
                  }
                  replyPos = 0;
                  
                  // send prompt
                  const char *prompt = ""; // TOFIX Con::getVariable("Con::Prompt");
                  if (client->socket != 0)
                  {
                     tel.SendDataFn(cfg.telnetUser, client->socket, dStrlen(prompt), (const unsigned char*)prompt);
                  }
                  client->state = FullAccessConnected;
               }
               else if(!dStrncmp(client->curLine, mListenPassword, PasswordMaxLength))
               {
                  if (client->socket != 0)
                  {
                     tel.SendDataFn(cfg.telnetUser, client->socket, replyPos, (const unsigned char*)reply);
                  }
                  replyPos = 0;
                  
                  // send prompt
                  const char *listenConnected = "Connected.\r\n";
                  if (client->socket != 0)
                  {
                     tel.SendDataFn(cfg.telnetUser, client->socket, dStrlen(listenConnected), (const unsigned char*)listenConnected);
                  }
                  client->state = ReadOnlyConnected;
               }
               else
               {
                  const char *sendStr;
                  if(client->state == DisconnectThisDude)
                     sendStr = "Too many tries... cya.";
                  else
                     sendStr = "Nope... try agian.\r\nEnter Password:";
                  
                  if (client->socket != 0)
                  {
                     tel.SendDataFn(cfg.telnetUser, client->socket, dStrlen(sendStr), (const unsigned char*)sendStr);
                  }

                  if (client->state == DisconnectThisDude)
                  {
                     tel.StopSocketFn(cfg.telnetUser, client->socket);
                     client->socket = 0;
                  }
               }
            }
         }
         else if(recvBuf[i] == '\b')
         {
            // pull the old backspace manuever...
            if(client->curPos > 0)
            {
               client->curPos--;
               if(client->state == FullAccessConnected)
               {
                  reply[replyPos++] = '\b';
                  reply[replyPos++] = ' ';
                  reply[replyPos++] = '\b';
               }
            }
         }
         else if(client->curPos < KorkApi::MaxLineLength-1)
         {
            client->curLine[client->curPos++] = recvBuf[i];
            // don't echo password chars...
            if(client->state == FullAccessConnected)
               reply[replyPos++] = recvBuf[i];
         }
      }
      
      // Echo the character back to the user, unless the remote echo
      // is disabled (by default)
      if(replyPos && mRemoteEchoEnabled)
      {
         if (client->socket != 0)
         {
            tel.SendDataFn(cfg.telnetUser, client->socket, replyPos, (const unsigned char*)reply);
         }
      }
   }
   
   TelnetClient ** walk = &mClientList;
   TelnetClient *cl;
   while((cl = *walk) != NULL)
   {
      if(cl->socket == 0)
      {
         *walk = cl->nextClient;
         delete cl;
      }
      else
         walk = &cl->nextClient;
   }
}

void TelnetConsole::disconnect()
{
   if (!mValid)
   {
      return;
   }

   KorkApi::Config& cfg = mVMInternal->mConfig;
   KorkApi::TelnetInterface& tel = cfg.iTelnet;
   TelnetClient* client = NULL;

   for (client = mClientList; client; client = client->nextClient)
   {
      tel.StopSocketFn(cfg.telnetUser, client->socket);
   }

   client = mClientList;
   while (client != NULL)
   {
      TelnetClient* delClient = client;
      client = mClientList->nextClient;
      delete delClient;
   }
}

