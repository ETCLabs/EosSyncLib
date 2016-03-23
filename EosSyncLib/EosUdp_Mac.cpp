// Copyright (c) 2015 Electronic Theatre Controls, Inc., http://www.etcconnect.com
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#include "EosUdp_Mac.h"
#include "EosLog.h"
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>

////////////////////////////////////////////////////////////////////////////////

EosUdpIn_Mac::EosUdpIn_Mac()
	: m_Socket(-1)
{
}

////////////////////////////////////////////////////////////////////////////////

EosUdpIn_Mac::~EosUdpIn_Mac()
{
	Shutdown();
}

////////////////////////////////////////////////////////////////////////////////

bool EosUdpIn_Mac::Initialize(EosLog &log, const char *ip, unsigned short port)
{
	if(	!IsInitialized() )
	{
		SetLogPrefix("udp input", ip, port, m_LogPrefix);
	
		if( ip )
		{
			m_Socket = socket(AF_INET, SOCK_DGRAM, 0);
			if(m_Socket != -1)
			{
				int optval = 1;
				if(setsockopt(m_Socket,SOL_SOCKET,SO_REUSEADDR,(const char*)&optval,sizeof(optval)) == -1)
				{
					char text[256];
					sprintf(text, "%s setsockopt(SO_REUSEADDR) failed with error %d", GetLogPrefix(m_LogPrefix), errno);
					log.AddWarning(text);
				}
				
				sockaddr_in addr;
				memset(&addr, 0, sizeof(addr));
				addr.sin_family = AF_INET;
				addr.sin_addr.s_addr = inet_addr(ip);
				addr.sin_port = htons(port);
				int result = bind(m_Socket, reinterpret_cast<sockaddr*>(&addr), static_cast<socklen_t>(sizeof(addr)));
				if(result != -1)
				{
					char text[256];
					sprintf(text, "%s socket intialized", GetLogPrefix(m_LogPrefix));
					log.AddInfo(text);
				}
				else
				{
					char text[256];
					sprintf(text, "%s bind failed with error %d", GetLogPrefix(m_LogPrefix), errno);
					log.AddError(text);
					close(m_Socket);
					m_Socket = -1;
				}
			}
			else
			{
				char text[256];
				sprintf(text, "%s socket failed with error %d", GetLogPrefix(m_LogPrefix), errno);
				log.AddError(text);
			}
		}
		else
		{
			char text[256];
			sprintf(text, "%s initialize failed, invalid arguments", GetLogPrefix(m_LogPrefix));
			log.AddError(text);
		}
	}
	else
	{
		char text[256];
		sprintf(text, "%s initialize failed, already initialized", GetLogPrefix(m_LogPrefix));
		log.AddWarning(text);
	}

	return IsInitialized();
}

////////////////////////////////////////////////////////////////////////////////

void EosUdpIn_Mac::Shutdown()
{
	if( IsInitialized() )
	{
		close(m_Socket);
		m_Socket = -1;
	}
}

////////////////////////////////////////////////////////////////////////////////

const char* EosUdpIn_Mac::RecvPacket(EosLog &log, unsigned int timeoutMS, unsigned int retryCount, int &len, void *addr, int *addrSize)
{
	if( IsInitialized() )
	{
		for(;;)
		{
			fd_set readfds;
			FD_ZERO(&readfds);
			FD_SET(m_Socket, &readfds);

			timeval timeout;
			timeout.tv_sec = timeoutMS/1000;
			timeout.tv_usec = ((timeoutMS - timeoutMS/1000) * 1000);

			int result = select(m_Socket+1, &readfds, 0, 0, &timeout);
			if(result > 0)
			{
				socklen_t fromSize = (addrSize ? static_cast<socklen_t>(*addrSize) : 0);
				len = static_cast<int>( recvfrom(m_Socket,m_RecvBuf,EOS_UDP_RECV_BUF_LEN,0,static_cast<sockaddr*>(addr),(addr && addrSize) ? (&fromSize) : 0) );
				if(len == -1)
				{
					char text[256];
					sprintf(text, "%s recvfrom failed with error %d", GetLogPrefix(m_LogPrefix), errno);
					log.AddError(text);
				}
				else if(len < 1)
				{
					char text[256];
					sprintf(text, "%s recvfrom failed, received %d bytes", GetLogPrefix(m_LogPrefix), len);
					log.AddWarning(text);
				}
				else
					return m_RecvBuf;
			}
			else if(result < 0)
			{
				char text[256];
				sprintf(text, "%s select failed with error %d", GetLogPrefix(m_LogPrefix), errno);
				log.AddError(text);
				break;
			}

			if(retryCount-- == 0)
				break;
		}
	}
	else
	{
		char text[256];
		sprintf(text, "%s RecvPacket failed, not initialized", GetLogPrefix(m_LogPrefix));
		log.AddError(text);
	}

	return 0;
}

////////////////////////////////////////////////////////////////////////////////

EosUdpOut_Mac::EosUdpOut_Mac()
	: m_Socket(-1)
{
}

////////////////////////////////////////////////////////////////////////////////

EosUdpOut_Mac::~EosUdpOut_Mac()
{
	Shutdown();
}

////////////////////////////////////////////////////////////////////////////////

bool EosUdpOut_Mac::Initialize(EosLog &log, const char *ip, unsigned short port)
{
	if(	!IsInitialized() )
	{
		EosUdpIn::SetLogPrefix("udp output", ip, port, m_LogPrefix);
	
		if( ip )
		{
			m_Socket = socket(AF_INET, SOCK_DGRAM, 0);
			if(m_Socket != -1)
			{
				int optval = 1;
				if(setsockopt(m_Socket,SOL_SOCKET,SO_BROADCAST,(const char*)&optval,sizeof(optval)) == -1)
				{
					char text[256];
					sprintf(text, "%s setsockopt(SO_BROADCAST) failed with error %d", EosUdpIn::GetLogPrefix(m_LogPrefix), errno);
					log.AddWarning(text);
				}
				
				memset(&m_Addr, 0, sizeof(m_Addr));
				m_Addr.sin_family = AF_INET;
				m_Addr.sin_addr.s_addr = inet_addr(ip);
				m_Addr.sin_port = htons(port);
				
				char text[256];
				sprintf(text, "%s socket intialized", EosUdpIn::GetLogPrefix(m_LogPrefix));
				log.AddInfo(text);
			}
			else
			{
				char text[256];
				sprintf(text, "%s socket failed with error %d", EosUdpIn::GetLogPrefix(m_LogPrefix), errno);
				log.AddError(text);
			}
		}
		else
		{
			char text[256];
			sprintf(text, "%s initialize failed, invalid arguments", EosUdpIn::GetLogPrefix(m_LogPrefix));
			log.AddError(text);
		}
	}
	else
	{
		char text[256];
		sprintf(text, "%s initialize failed, already initialized", EosUdpIn::GetLogPrefix(m_LogPrefix));
		log.AddWarning(text);
	}

	return IsInitialized();
}

////////////////////////////////////////////////////////////////////////////////

void EosUdpOut_Mac::Shutdown()
{
	if( IsInitialized() )
	{
		close(m_Socket);
		m_Socket = -1;
	}
}

////////////////////////////////////////////////////////////////////////////////

bool EosUdpOut_Mac::SendPacket(EosLog &log, const char *buf, int len)
{
	if( IsInitialized()  )
	{
		if(buf && len>0)
		{
			int bytesSent = static_cast<int>( sendto(m_Socket,buf,len,0,reinterpret_cast<sockaddr*>(&m_Addr),static_cast<socklen_t>(sizeof(m_Addr))) );
			if(bytesSent == -1)
			{
				char text[256];
				sprintf(text, "%s sendto failed with error %d", EosUdpIn::GetLogPrefix(m_LogPrefix), errno);
				log.AddError(text);
			}
			else if(bytesSent != len)
			{
				char text[256];
				sprintf(text, "%s sendto failed, sent %d of %d bytes", EosUdpIn::GetLogPrefix(m_LogPrefix), bytesSent, len);
				log.AddError(text);
			}
			else
				return true;
		}
	}
	else
	{
		char text[256];
		sprintf(text, "%s SendPacket failed, not initialized", EosUdpIn::GetLogPrefix(m_LogPrefix));
		log.AddError(text);
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////
