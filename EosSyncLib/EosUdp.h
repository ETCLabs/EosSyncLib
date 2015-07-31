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
#ifndef EOS_UDP_H
#define EOS_UDP_H

#define EOS_UDP_RECV_BUF_LEN 2048

#include <string>

class EosLog;

////////////////////////////////////////////////////////////////////////////////

class EosUdpIn
{
public:
	EosUdpIn();
	virtual ~EosUdpIn();

	virtual bool Initialize(EosLog &log, const char *ip, unsigned short port) = 0;
	virtual bool IsInitialized() const = 0;
	virtual void Shutdown() = 0;
	virtual const char* RecvPacket(EosLog &log, unsigned int timeoutMS, unsigned int retryCount, int &len, void *addr, int *addrSize) = 0;

	static EosUdpIn* Create();
	static void SetLogPrefix(const char *name, const char *ip, unsigned short port, std::string &logPrefix);
	static const char* GetLogPrefix(const std::string &logPrefix);

protected:
	char		*m_RecvBuf;
	std::string	m_LogPrefix;
};

////////////////////////////////////////////////////////////////////////////////

class EosUdpOut
{
public:
	EosUdpOut() {}
	virtual ~EosUdpOut() {}

	virtual bool Initialize(EosLog &log, const char *ip, unsigned short port) = 0;
	virtual bool IsInitialized() const = 0;
	virtual void Shutdown() = 0;
	virtual bool SendPacket(EosLog &log, const char *buf, int len) = 0;

	static EosUdpOut* Create();

protected:
	std::string	m_LogPrefix;
};

////////////////////////////////////////////////////////////////////////////////

#endif
