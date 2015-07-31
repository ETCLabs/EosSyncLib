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
#ifndef EOS_TCP_H
#define EOS_TCP_H

#include <string>
#include <stdlib.h>

class EosLog;

////////////////////////////////////////////////////////////////////////////////

class EosTcp
{
public:
	enum EnumConnectState
	{
		CONNECT_NOT_CONNECTED,
		CONNECT_IN_PROGRESS,
		CONNECT_CONNECTED
	};

	EosTcp();
	virtual ~EosTcp() {}

	virtual bool Initialize(EosLog &log, const char *ip, unsigned short port) = 0;
	virtual bool InitializeAccepted(EosLog &log, void *pSocket) = 0;
	virtual void Shutdown() = 0;
	virtual void Tick(EosLog &log) = 0;
	virtual EnumConnectState GetConnectState() const {return m_ConnectState;}
	virtual bool Send(EosLog &log, const char *data, size_t size) = 0;
	virtual const char* Recv(EosLog &log, unsigned int timeoutMS, size_t &size) = 0;

	static EosTcp* Create();
	static void SetLogPrefix(const char *name, const char *ip, unsigned short port, std::string &logPrefix);
	static const char* GetLogPrefix(const std::string &logPrefix);

protected:
	EnumConnectState	m_ConnectState;
	std::string			m_LogPrefix;
};

////////////////////////////////////////////////////////////////////////////////

class EosTcpServer
{
public:
	EosTcpServer();
	virtual ~EosTcpServer() {}

	virtual bool Initialize(EosLog &log, unsigned short port) = 0;
	virtual bool Initialize(EosLog &log, const char *ip, unsigned short port) = 0;
	virtual void Shutdown() = 0;
	virtual bool GetListening() const {return m_Listening;}
	virtual EosTcp* Recv(EosLog &log, unsigned int timeoutMS, void *addr, int *addrSize) = 0;

	static EosTcpServer* Create();

protected:
	bool		m_Listening;
	std::string	m_LogPrefix;
};

////////////////////////////////////////////////////////////////////////////////

#endif
