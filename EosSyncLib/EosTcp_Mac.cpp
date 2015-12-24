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

#include "EosTcp_Mac.h"
#include "EosLog.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <fcntl.h>

#define RECV_BUF_SIZE 1024

////////////////////////////////////////////////////////////////////////////////

EosTcp_Mac::EosTcp_Mac()
	: m_Socket(-1)
	, m_RecvBuf(0)
{
}

////////////////////////////////////////////////////////////////////////////////

EosTcp_Mac::~EosTcp_Mac()
{
	Shutdown();
}

////////////////////////////////////////////////////////////////////////////////

bool EosTcp_Mac::Initialize(EosLog &log, const char *ip, unsigned short port)
{
	if(m_Socket == -1)
	{
		SetLogPrefix("tcp client", ip, port, m_LogPrefix);
	
		if(ip && *ip)
		{
			m_Socket = socket(AF_INET, SOCK_STREAM, 0);
			if(m_Socket != -1)
			{
				sockaddr_in addr;
				memset(&addr, 0, sizeof(addr));
				addr.sin_family = AF_INET;
				addr.sin_addr.s_addr = inet_addr(ip);
				addr.sin_port = htons(port);

				int optval = 1;
				if(setsockopt(m_Socket,SOL_SOCKET,SO_NOSIGPIPE,(const char*)&optval,sizeof(optval)) == -1)
				{
					char text[256];
					sprintf(text, "%s setsockopt(SO_NOSIGPIPE) failed with error %d", GetLogPrefix(m_LogPrefix), errno);
					log.AddWarning(text);
				}
				
				// temporarily make socket non-blocking during connect					
				SetSocketBlocking(log, m_LogPrefix, m_Socket, false);
				
				if(connect(m_Socket,reinterpret_cast<const sockaddr*>(&addr),static_cast<int>(sizeof(addr))) == 0)
				{
					char text[256];
					sprintf(text, "%s connected", GetLogPrefix(m_LogPrefix));
					log.AddInfo(text);
					m_ConnectState = CONNECT_CONNECTED;
					SetSocketBlocking(log, m_LogPrefix, m_Socket, true);
					return true;
				}
				else if(errno == EINPROGRESS)
				{
					char text[256];
					sprintf(text, "%s connecting...", GetLogPrefix(m_LogPrefix));
					log.AddInfo(text);
					m_ConnectState = CONNECT_IN_PROGRESS;
					return true;
				}
				else
				{
					char text[256];
					sprintf(text, "%s connect failed with error %d", GetLogPrefix(m_LogPrefix), errno);
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
	
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool EosTcp_Mac::InitializeAccepted(EosLog &log, void *pSocket)
{
	if(m_Socket == -1)
	{
		if( pSocket )
			m_Socket = *static_cast<int*>(pSocket);
		
		if(m_Socket != -1)
		{
			m_ConnectState = CONNECT_CONNECTED;
		}
		else
		{
			char text[256];
			sprintf(text, "%s initialize accepted failed, invalid arguments", GetLogPrefix(m_LogPrefix));
			log.AddError(text);
		}
	}
	else
	{
		char text[256];
		sprintf(text, "%s initialize accepted failed, already initialized", GetLogPrefix(m_LogPrefix));
		log.AddError(text);
	}

	return (m_Socket != -1);
}

////////////////////////////////////////////////////////////////////////////////

void EosTcp_Mac::Shutdown()
{
	if(m_Socket != -1)
	{
		close(m_Socket);
		m_Socket = -1;
	}

	if( m_RecvBuf )
	{
		delete[] m_RecvBuf;
		m_RecvBuf = 0;
	}
	
	m_ConnectState = CONNECT_NOT_CONNECTED;
}

////////////////////////////////////////////////////////////////////////////////

void EosTcp_Mac::Tick(EosLog &log)
{
	if(m_Socket != -1)
	{
		if(m_ConnectState == CONNECT_IN_PROGRESS)
		{
			fd_set writefds;
			FD_ZERO(&writefds);
			FD_SET(m_Socket, &writefds);

			timeval timeout;
			timeout.tv_sec = 0;
			timeout.tv_usec = 1000;

			int result = select(m_Socket+1, 0, &writefds, 0, &timeout);
			if(result > 0)
			{
				int optValue = 0;
				socklen_t optLen = sizeof(optValue);
				if(getsockopt(m_Socket,SOL_SOCKET,SO_ERROR,&optValue,&optLen) == 0)
				{
					if(optValue == 0)
					{
						char text[256];
						sprintf(text, "%s connected", GetLogPrefix(m_LogPrefix));
						log.AddInfo(text);
						m_ConnectState = CONNECT_CONNECTED;
						SetSocketBlocking(log, m_LogPrefix, m_Socket, true);
					}
					else
					{
						char text[256];
						sprintf(text, "%s connect failed with error %d", GetLogPrefix(m_LogPrefix), optValue);
						log.AddError(text);
						Shutdown();
					}
				}
				else
				{
					char text[256];
					sprintf(text, "%s getsockopt failed with error %d", GetLogPrefix(m_LogPrefix), errno);
					log.AddError(text);
					Shutdown();
				}
			}
			else if(result < 0)
			{
				char text[256];
				sprintf(text, "%s connect wait failed with error %d", GetLogPrefix(m_LogPrefix), errno);
				log.AddError(text);
				Shutdown();
			}
		}
	}
	else
	{
		char text[256];
		sprintf(text, "%s tick failed, not initialized", GetLogPrefix(m_LogPrefix));
		log.AddWarning(text);
	}
}

////////////////////////////////////////////////////////////////////////////////

bool EosTcp_Mac::Send(EosLog &log, const char *data, size_t size)
{
	if(m_Socket != -1)
	{
		if(m_ConnectState == CONNECT_CONNECTED)
		{
			ssize_t result = send(m_Socket, data, size, 0);
			if(result == -1)
			{
				char text[256];
				sprintf(text, "%s send failed with error %d", GetLogPrefix(m_LogPrefix), errno);
				log.AddError(text);
				m_ConnectState = CONNECT_NOT_CONNECTED;
			}
			else if(static_cast<size_t>(result) != size)
			{
				char text[256];
				sprintf(text, "%s send truncated %d of %d", GetLogPrefix(m_LogPrefix), static_cast<int>(result), static_cast<int>(size));
				log.AddError(text);
				m_ConnectState = CONNECT_NOT_CONNECTED;
			}
			else
				return true;
		}
		else
		{
			char text[256];
			sprintf(text, "%s send failed, not connected", GetLogPrefix(m_LogPrefix));
			log.AddError(text);
		}
	}
	else
	{
		char text[256];
		sprintf(text, "%s send failed, not initialized", GetLogPrefix(m_LogPrefix));
		log.AddError(text);
	}
	
	return false;
}

////////////////////////////////////////////////////////////////////////////////

const char* EosTcp_Mac::Recv(EosLog &log, unsigned int timeoutMS, size_t &size)
{
	if(m_Socket != -1)
	{
		if(m_ConnectState == CONNECT_CONNECTED)
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
				if( !m_RecvBuf )
					m_RecvBuf = new char[RECV_BUF_SIZE];

				result = recv(m_Socket, m_RecvBuf, RECV_BUF_SIZE, 0);
				if(result == -1)
				{
					char text[256];
					sprintf(text, "%s recv failed with error %d", GetLogPrefix(m_LogPrefix), errno);
					log.AddError(text);
					m_ConnectState = CONNECT_NOT_CONNECTED;
				}
				else if(result > 0)
				{
					size = static_cast<size_t>(result);
					return m_RecvBuf;
				}
			}
			else if(result < 0)
			{
				char text[256];
				sprintf(text, "%s select failed with error %d", GetLogPrefix(m_LogPrefix), errno);
				log.AddError(text);
				m_ConnectState = CONNECT_NOT_CONNECTED;
			}
		}
		else
		{
			char text[256];
			sprintf(text, "%s recv failed, not connected", GetLogPrefix(m_LogPrefix));
			log.AddError(text);
		}
	}
	else
	{
		char text[256];
		sprintf(text, "%s recv failed, not initialized", GetLogPrefix(m_LogPrefix));
		log.AddError(text);
	}
	
	return 0;
}

////////////////////////////////////////////////////////////////////////////////

bool EosTcp_Mac::SetSocketBlocking(EosLog &log, const std::string &logPrefix, int socket, bool b)
{
	if(socket != -1)
	{
		int flags = fcntl(socket, F_GETFL, 0);
		if(flags == -1)
		{
			char text[256];
			sprintf(text, "%s fnctl(get) failed with error %d", GetLogPrefix(logPrefix), errno);
			log.AddInfo(text);
		}
		else
		{
			flags = (b ? (flags&~O_NONBLOCK) : (flags|O_NONBLOCK));
			if(fcntl(socket,F_SETFL,flags) != -1)
			{
				return true;
			}
			else
			{
				char text[256];
				sprintf(text, "%s fnctl(set) failed with error %d", GetLogPrefix(logPrefix), errno);
				log.AddInfo(text);
			}
		}
	}
	else
	{
		char text[256];
		sprintf(text, "%s setblocking(%s) failed, not initialized", GetLogPrefix(logPrefix), b?"true":"false");
		log.AddWarning(text);
	}
	
	return false;
}

////////////////////////////////////////////////////////////////////////////////

EosTcpServer_Mac::EosTcpServer_Mac()
	: m_Socket(-1)
{
}

////////////////////////////////////////////////////////////////////////////////

EosTcpServer_Mac::~EosTcpServer_Mac()
{
	Shutdown();
}

////////////////////////////////////////////////////////////////////////////////

bool EosTcpServer_Mac::Initialize(EosLog &log, unsigned short port)
{
	return Initialize(log, 0, port);
}

////////////////////////////////////////////////////////////////////////////////

bool EosTcpServer_Mac::Initialize(EosLog &log, const char *ip, unsigned short port)
{
	if(m_Socket == -1)
	{
		const char *actualIP = (ip ? ip : "0.0.0.0");
		EosTcp::SetLogPrefix("tcp server", actualIP, port, m_LogPrefix);
		
		m_Socket = socket(AF_INET, SOCK_STREAM, 0);
		if(m_Socket != -1)
		{
			int optval = 1;
			if(setsockopt(m_Socket,SOL_SOCKET,SO_REUSEADDR,(const char*)&optval,sizeof(optval)) == -1)
			{
				char text[256];
				sprintf(text, "%s setsockopt(SO_REUSEADDR) failed with error %d", EosTcp::GetLogPrefix(m_LogPrefix), errno);
				log.AddWarning(text);
			}

			optval = 1;
			if(setsockopt(m_Socket,SOL_SOCKET,SO_NOSIGPIPE,(const char*)&optval,sizeof(optval)) == -1)
			{
				char text[256];
				sprintf(text, "%s setsockopt(SO_NOSIGPIPE) failed with error %d", EosTcp::GetLogPrefix(m_LogPrefix), errno);
				log.AddWarning(text);
			}
			
			sockaddr_in addr;
			memset(&addr, 0, sizeof(addr));
			addr.sin_family = AF_INET;
			addr.sin_addr.s_addr = inet_addr(actualIP);
			addr.sin_port = htons(port);
			
			int result = bind(m_Socket, reinterpret_cast<sockaddr*>(&addr), static_cast<socklen_t>(sizeof(addr)));
			if(result != -1)
			{
				result = listen(m_Socket, SOMAXCONN);
				if(result != -1)
				{
					EosTcp_Mac::SetSocketBlocking(log, m_LogPrefix, m_Socket, false);

					char text[256];
					sprintf(text, "%s socket intialized", EosTcp::GetLogPrefix(m_LogPrefix));
					log.AddInfo(text);

					m_Listening = true;
				}
				else
				{
					char text[256];
					sprintf(text, "%s listen failed with error %d", EosTcp::GetLogPrefix(m_LogPrefix), errno);
					log.AddError(text);
					close(m_Socket);
					m_Socket = -1;
				}
			}
			else
			{
				char text[256];
				sprintf(text, "%s bind failed with error %d", EosTcp::GetLogPrefix(m_LogPrefix), errno);
				log.AddError(text);
				close(m_Socket);
				m_Socket = -1;
			}
		}
		else
		{
			char text[256];
			sprintf(text, "%s socket failed with error %d", EosTcp::GetLogPrefix(m_LogPrefix), errno);
			log.AddError(text);
		}
	}
	else
	{
		char text[256];
		sprintf(text, "%s initialize failed, already initialized", EosTcp::GetLogPrefix(m_LogPrefix));
		log.AddWarning(text);
	}

	return (m_Socket != -1);
}

////////////////////////////////////////////////////////////////////////////////

void EosTcpServer_Mac::Shutdown()
{
	if(m_Socket != -1)
	{
		close(m_Socket);
		m_Socket = -1;
	}

	m_Listening = false;
}

////////////////////////////////////////////////////////////////////////////////

EosTcp* EosTcpServer_Mac::Recv(EosLog &log, unsigned int timeoutMS, void *addr, int *addrSize)
{
	EosTcp *newConnection = 0;

	if(m_Socket != -1)
	{
		if( m_Listening )
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
				socklen_t addrLen = 0;
				int s = accept(m_Socket, static_cast<sockaddr*>(addr), &addrLen);
				if(s != -1)
				{
					*addrSize = static_cast<int>(addrLen);
					newConnection = EosTcp::Create();
					newConnection->InitializeAccepted(log, &s);

					sockaddr_in *addr_in = static_cast<sockaddr_in*>(addr);
					char *ip = inet_ntoa( addr_in->sin_addr );
					char text[256];
					sprintf(text, "%s new connection(%s:%u)", EosTcp::GetLogPrefix(m_LogPrefix), ip ? ip : "", addr_in->sin_port);
					log.AddInfo(text);
				}
				else
				{
					char text[256];
					sprintf(text, "%s accept failed with error %d", EosTcp::GetLogPrefix(m_LogPrefix), errno);
					log.AddError(text);
				}
			}
			else if(result < 0)
			{
				char text[256];
				sprintf(text, "%s select failed with error %d", EosTcp::GetLogPrefix(m_LogPrefix), errno);
				log.AddError(text);

				m_Listening = false;
			}
		}
		else
		{
			char text[256];
			sprintf(text, "%s recv failed, not listening", EosTcp::GetLogPrefix(m_LogPrefix));
			log.AddError(text);
		}
	}
	else
	{
		char text[256];
		sprintf(text, "%s recv failed, not initialized", EosTcp::GetLogPrefix(m_LogPrefix));
		log.AddError(text);
	}

	return newConnection;
}

////////////////////////////////////////////////////////////////////////////////
