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

#include "EosTcp_Win.h"
#include "EosLog.h"

#define RECV_BUF_SIZE 1024

////////////////////////////////////////////////////////////////////////////////

EosTcp_Win::EosTcp_Win()
	: m_Socket(INVALID_SOCKET)
	, m_RecvBuf(0)
{
}

////////////////////////////////////////////////////////////////////////////////

EosTcp_Win::~EosTcp_Win()
{
	Shutdown();
}

////////////////////////////////////////////////////////////////////////////////

bool EosTcp_Win::Initialize(EosLog &log, const char *ip, unsigned short port)
{
	if(m_Socket == INVALID_SOCKET)
	{
		SetLogPrefix("tcp client", ip, port, m_LogPrefix);

		if(ip && *ip)
		{
			WSADATA wsaData;
			int result = WSAStartup(MAKEWORD(2,2), &wsaData);
			if(result == 0)
			{
				m_Socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
				if(m_Socket != INVALID_SOCKET)
				{
					sockaddr_in addr;
					memset(&addr, 0, sizeof(addr));
					addr.sin_family = AF_INET;
					addr.sin_addr.s_addr = inet_addr(ip);
					addr.sin_port = htons(port);

					// temporarily make socket non-blocking during connect					
					SetSocketBlocking(log, m_LogPrefix, m_Socket, false);

					if(connect(m_Socket,reinterpret_cast<const sockaddr*>(&addr),static_cast<int>(sizeof(addr))) == 0)
					{
						char text[256];
						sprintf(text, "%s connected", GetLogPrefix(m_LogPrefix));
						log.AddInfo(text);
						m_ConnectState = CONNECT_CONNECTED;
						SetSocketBlocking(log, m_LogPrefix, m_Socket, true);
					}
					else if(WSAGetLastError() == WSAEWOULDBLOCK)
					{
						char text[256];
						sprintf(text, "%s connecting...", GetLogPrefix(m_LogPrefix));
						log.AddInfo(text);
						m_ConnectState = CONNECT_IN_PROGRESS;
					}
					else
					{
						char text[256];
						sprintf(text, "%s connect failed with error %d", GetLogPrefix(m_LogPrefix), WSAGetLastError());
						log.AddError(text);
						shutdown(m_Socket, SD_BOTH);
						closesocket(m_Socket);
						m_Socket = INVALID_SOCKET;
						WSACleanup();
					}
				}
				else
				{
					char text[256];
					sprintf(text, "%s socket failed with error %d", GetLogPrefix(m_LogPrefix), WSAGetLastError());
					log.AddError(text);
					WSACleanup();
				}
			}
			else
			{
				char text[256];
				sprintf(text, "%s WSAStartup failed with error %d", GetLogPrefix(m_LogPrefix), result);
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

	return (m_Socket != INVALID_SOCKET);
}

////////////////////////////////////////////////////////////////////////////////

bool EosTcp_Win::InitializeAccepted(EosLog &log, void *pSocket)
{
	if(m_Socket == INVALID_SOCKET)
	{
		if( pSocket )
			m_Socket = *static_cast<SOCKET*>(pSocket);
		
		if(m_Socket != INVALID_SOCKET)
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

	return (m_Socket != INVALID_SOCKET);
}

////////////////////////////////////////////////////////////////////////////////

void EosTcp_Win::Shutdown()
{
	if(m_Socket != INVALID_SOCKET)
	{
		shutdown(m_Socket, SD_BOTH);
		closesocket(m_Socket);
		m_Socket = INVALID_SOCKET;
		WSACleanup();
	}

	if( m_RecvBuf )
	{
		delete[] m_RecvBuf;
		m_RecvBuf = 0;
	}

	m_ConnectState = CONNECT_NOT_CONNECTED;
	m_LogPrefix.clear();
}

////////////////////////////////////////////////////////////////////////////////

void EosTcp_Win::Tick(EosLog &log)
{
	if(m_Socket != INVALID_SOCKET)
	{
		if(m_ConnectState == CONNECT_IN_PROGRESS)
		{
			fd_set writefds;
			writefds.fd_count = 1;
			writefds.fd_array[0] = m_Socket;

			TIMEVAL timeout;
			timeout.tv_sec = 0;
			timeout.tv_usec = 10000;

			int result = select(0, 0, &writefds, 0, &timeout);
			if(result > 0)
			{
				char text[256];
				sprintf(text, "%s connected", GetLogPrefix(m_LogPrefix));
				log.AddInfo(text);
				m_ConnectState = CONNECT_CONNECTED;
				SetSocketBlocking(log, m_LogPrefix, m_Socket, true);
			}
			else if(result < 0)
			{
				char text[256];
				sprintf(text, "%s select failed with error %d", GetLogPrefix(m_LogPrefix), WSAGetLastError());
				log.AddError(text);
				Shutdown();
			}
		}
	}
	else
	{
		char text[256];
		sprintf(text, "%s tick failed, not initialized", GetLogPrefix(m_LogPrefix));
		log.AddError(text);
	}
}

////////////////////////////////////////////////////////////////////////////////

bool EosTcp_Win::Send(EosLog &log, const char *data, size_t size)
{
	if(m_Socket != INVALID_SOCKET)
	{
		if(m_ConnectState == CONNECT_CONNECTED)
		{
			int result = send(m_Socket, data, static_cast<int>(size), 0);
			if(result == SOCKET_ERROR)
			{
				char text[256];
				sprintf(text, "%s send failed with error %d", GetLogPrefix(m_LogPrefix), WSAGetLastError());
				log.AddError(text);
				m_ConnectState = CONNECT_NOT_CONNECTED;
			}
			else if(result != static_cast<int>(size))
			{
				char text[256];
				sprintf(text, "%s send truncated %d of %d", GetLogPrefix(m_LogPrefix), result, static_cast<int>(size));
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

const char* EosTcp_Win::Recv(EosLog &log, unsigned int timeoutMS, size_t &size)
{
	if(m_Socket != INVALID_SOCKET)
	{
		if(m_ConnectState == CONNECT_CONNECTED)
		{
			fd_set readfds;
			readfds.fd_count = 1;
			readfds.fd_array[0] = m_Socket;

			TIMEVAL timeout;
			timeout.tv_sec = timeoutMS/1000;
			timeout.tv_usec = ((timeoutMS - timeoutMS/1000) * 1000);

			int result = select(0, &readfds, 0, 0, &timeout);
			if(result > 0)
			{
				if( !m_RecvBuf )
					m_RecvBuf = new char[RECV_BUF_SIZE];

				result = recv(m_Socket, m_RecvBuf, RECV_BUF_SIZE, 0);
				if(result == SOCKET_ERROR)
				{
					char text[256];
					sprintf(text, "%s recv failed with error %d", GetLogPrefix(m_LogPrefix), WSAGetLastError());
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
				sprintf(text, "%s selected failed with error %d", GetLogPrefix(m_LogPrefix), WSAGetLastError());
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

bool EosTcp_Win::SetSocketBlocking(EosLog &log, const std::string &logPrefix, SOCKET socket, bool b)
{
	if(socket != INVALID_SOCKET)
	{
		u_long nonBlockingMode = (b ? 0 : 1);
		if(ioctlsocket(socket,FIONBIO,&nonBlockingMode) == 0)
		{
			return true;
		}
		else
		{
			char text[256];
			sprintf(text, "%s ioctlsocket(non-blocking) failed with error %d", GetLogPrefix(logPrefix), WSAGetLastError());
			log.AddInfo(text);
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

EosTcpServer_Win::EosTcpServer_Win()
	: m_Socket(INVALID_SOCKET)
{
}

////////////////////////////////////////////////////////////////////////////////

EosTcpServer_Win::~EosTcpServer_Win()
{
	Shutdown();
}

////////////////////////////////////////////////////////////////////////////////

bool EosTcpServer_Win::Initialize(EosLog &log, unsigned short port)
{
	return Initialize(log, 0, port);
}

////////////////////////////////////////////////////////////////////////////////

bool EosTcpServer_Win::Initialize(EosLog &log, const char *ip, unsigned short port)
{
	if(m_Socket == INVALID_SOCKET)
	{
		const char *actualIP = (ip ? ip : "0.0.0.0");
		EosTcp::SetLogPrefix("tcp server", actualIP, port, m_LogPrefix);

		WSADATA wsaData;
		int result = WSAStartup(MAKEWORD(2,2), &wsaData);
		if(result == 0)
		{
			m_Socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
			if(m_Socket != INVALID_SOCKET)
			{
				int optval = 1;
				if(setsockopt(m_Socket,SOL_SOCKET,SO_REUSEADDR,(const char*)&optval,sizeof(optval)) == SOCKET_ERROR)
				{
					char text[256];
					sprintf(text, "%s setsockopt(SO_REUSEADDR) failed with error %d", EosTcp::GetLogPrefix(m_LogPrefix), WSAGetLastError());
					log.AddWarning(text);
				}

				sockaddr_in addr;
				memset(&addr, 0, sizeof(addr));
				addr.sin_family = AF_INET;
				addr.sin_addr.S_un.S_addr = inet_addr(actualIP);
				addr.sin_port = htons(port);
				result = bind(m_Socket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
				if(result != SOCKET_ERROR)
				{
					result = listen(m_Socket, SOMAXCONN);
					if(result != SOCKET_ERROR)
					{
						EosTcp_Win::SetSocketBlocking(log, m_LogPrefix, m_Socket, false);

						char text[256];
						sprintf(text, "%s socket intialized",EosTcp::GetLogPrefix(m_LogPrefix));
						log.AddInfo(text);

						m_Listening = true;
					}
					else
					{
						char text[256];
						sprintf(text, "%s listen failed with error %d", EosTcp::GetLogPrefix(m_LogPrefix), WSAGetLastError());
						log.AddError(text);
						closesocket(m_Socket);
						m_Socket = INVALID_SOCKET;
						WSACleanup();
					}
				}
				else
				{
					char text[256];
					sprintf(text, "%s bind failed with error %d", EosTcp::GetLogPrefix(m_LogPrefix), WSAGetLastError());
					log.AddError(text);
					closesocket(m_Socket);
					m_Socket = INVALID_SOCKET;
					WSACleanup();
				}
			}
			else
			{
				char text[256];
				sprintf(text, "%s socket failed with error %d", EosTcp::GetLogPrefix(m_LogPrefix), WSAGetLastError());
				log.AddError(text);
				WSACleanup();
			}
		}
		else
		{
			char text[256];
			sprintf(text, "%s WSAStartup failed with error %d", EosTcp::GetLogPrefix(m_LogPrefix), result);
			log.AddError(text);
		}
	}
	else
	{
		char text[256];
		sprintf(text, "%s initialize failed, already initialized", EosTcp::GetLogPrefix(m_LogPrefix));
		log.AddError(text);
	}

	return (m_Socket != INVALID_SOCKET);
}

////////////////////////////////////////////////////////////////////////////////

void EosTcpServer_Win::Shutdown()
{
	if(m_Socket != INVALID_SOCKET)
	{
		closesocket(m_Socket);
		m_Socket = INVALID_SOCKET;
		WSACleanup();
	}

	m_Listening = false;
}

////////////////////////////////////////////////////////////////////////////////

EosTcp* EosTcpServer_Win::Recv(EosLog &log, unsigned int timeoutMS, void *addr, int *addrSize)
{
	EosTcp *newConnection = 0;

	if(m_Socket != INVALID_SOCKET)
	{
		if( m_Listening )
		{
			fd_set readfds;
			readfds.fd_count = 1;
			readfds.fd_array[0] = m_Socket;

			TIMEVAL timeout;
			timeout.tv_sec = timeoutMS/1000;
			timeout.tv_usec = ((timeoutMS - timeoutMS/1000) * 1000);

			int result = select(0, &readfds, 0, 0, &timeout);
			if(result > 0)
			{
				SOCKET s = accept(m_Socket, static_cast<sockaddr*>(addr), addrSize);
				if(s != INVALID_SOCKET)
				{
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
					sprintf(text, "%s accept failed with error %d", EosTcp::GetLogPrefix(m_LogPrefix), WSAGetLastError());
					log.AddError(text);
				}
			}
			else if(result < 0)
			{
				char text[256];
				sprintf(text, "%s select failed with error %d", EosTcp::GetLogPrefix(m_LogPrefix), WSAGetLastError());
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
