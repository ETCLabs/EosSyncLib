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
#ifndef EOS_OSC_H
#define EOS_OSC_H

#ifndef OSC_PARSER_H
#include "OSCParser.h"
#endif

#include <vector>
#include <queue>

class EosTcp;
class EosLog;
class EosTimer;

////////////////////////////////////////////////////////////////////////////////

class EosOsc
	: public OSCParserClient
{
public:
	struct sCommand
	{
		sCommand();
		~sCommand();
		void clear();
		std::string	path;
		OSCArgument	*args;
		size_t		argCount;
		char		*buf;
	};

	typedef std::queue<sCommand*> CMD_Q;

	EosOsc(EosLog &log);
	~EosOsc();

	bool Send(EosTcp &tcp, const OSCPacketWriter &packet, bool immediate);
	void Recv(EosTcp &tcp, unsigned int timeoutMS, CMD_Q &cmdQ);
	void Tick(EosTcp &tcp);
	void OSCParserClient_Log(const std::string &message);
	void OSCParserClient_Send(const char* /*buf*/, size_t /*size*/) {}

private:
	struct sQueuedPacket
	{
		sQueuedPacket(char *Data, size_t Size) : data(Data), size(Size) {}
		char	*data;
		size_t	size;
	};

	typedef std::vector<sQueuedPacket> Q;

	struct sInputBuffer
	{
		char	*data;
		size_t	size;
		size_t	capacity;
	};

	OSCParser		m_Parser;
	Q				m_Q;
	EosLog			*m_pLog;
	sQueuedPacket	m_SendPacket;
	sInputBuffer	m_InputBuffer;

	virtual bool SendPacket(EosTcp &tcp, char *data, size_t size);
};

////////////////////////////////////////////////////////////////////////////////

#endif
