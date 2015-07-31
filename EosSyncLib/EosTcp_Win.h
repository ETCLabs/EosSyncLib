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

#pragma once
#ifndef EOS_TCP_WIN_H
#define EOS_TCP_WIN_H

#ifndef EOS_TCP_H
#include "EosTcp.h"
#endif

#include <WinSock2.h>
#include <Windows.h>

////////////////////////////////////////////////////////////////////////////////

class EosTcp_Win
	: public EosTcp
{
public:
	EosTcp_Win();
	virtual ~EosTcp_Win();

	virtual bool Initialize(EosLog &log, const char *ip, unsigned short port);
	virtual bool InitializeAccepted(EosLog &log, void *pSocket);
	virtual void Shutdown();
	virtual void Tick(EosLog &log);
	virtual bool Send(EosLog &log, const char *data, size_t size);
	virtual const char* Recv(EosLog &log, unsigned int timeoutMS, size_t &size);

	static bool SetSocketBlocking(EosLog &log, const std::string &logPrefix, SOCKET socket, bool b);

private:
	SOCKET	m_Socket;
	char	*m_RecvBuf;
};

////////////////////////////////////////////////////////////////////////////////

class EosTcpServer_Win
	: public EosTcpServer
{
public:
	EosTcpServer_Win();
	virtual ~EosTcpServer_Win();

	virtual bool Initialize(EosLog &log, unsigned short port);
	virtual bool Initialize(EosLog &log, const char *ip, unsigned short port);
	virtual void Shutdown();
	virtual EosTcp* Recv(EosLog &log, unsigned int timeoutMS, void *addr, int *addrSize);

private:
	SOCKET	m_Socket;
};

////////////////////////////////////////////////////////////////////////////////

#endif
