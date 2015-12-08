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

#include "EosOsc.h"
#include "EosTcp.h"
#include "EosLog.h"
#include "EosTimer.h"

////////////////////////////////////////////////////////////////////////////////

EosOsc::sCommand::sCommand()
	: args(0)
	, argCount(0)
	, buf(0)
{
}

////////////////////////////////////////////////////////////////////////////////

EosOsc::sCommand::~sCommand()
{
	clear();
}

////////////////////////////////////////////////////////////////////////////////

void EosOsc::sCommand::clear()
{
	if( args )
	{
		delete[] args;
		args = 0;
	}

	if( buf )
	{
		delete[] buf;
		buf = 0;
	}

	argCount = 0;
	path.clear();
}

////////////////////////////////////////////////////////////////////////////////

EosOsc::EosOsc(EosLog &log)
	: m_pLog(&log)
	, m_SendPacket(0, 0)
{
	m_Parser.SetRoot(new OSCMethod());
	memset(&m_InputBuffer, 0, sizeof(m_InputBuffer));
}

////////////////////////////////////////////////////////////////////////////////

EosOsc::~EosOsc()
{
	if( m_SendPacket.data )
	{
		delete[] m_SendPacket.data;
		m_SendPacket.data = 0;
	}

	if( m_InputBuffer.data )
		delete[] m_InputBuffer.data;

	memset(&m_InputBuffer, 0, sizeof(m_InputBuffer));

	for(Q::const_iterator i=m_Q.begin(); i!=m_Q.end(); i++)
		delete[] i->data;
	m_Q.clear();
}

////////////////////////////////////////////////////////////////////////////////

bool EosOsc::Send(EosTcp &tcp, const OSCPacketWriter &packet, bool immediate)
{
	bool success = false;

	size_t len;
	char *buf = packet.Create(len);
	if( buf )
	{
		if( immediate )
		{
			if( SendPacket(tcp,buf,len) )
				success = true;
		}
		else
		{
			m_Q.push_back( sQueuedPacket(buf,len) );
			success = true;
		}
	}
	else
		m_pLog->AddError("OSC packet creation failed");

	return success;
}

////////////////////////////////////////////////////////////////////////////////

void EosOsc::Recv(EosTcp &tcp, unsigned int timeoutMS, CMD_Q &cmdQ)
{
	size_t size;
	const char *buf = tcp.Recv(*m_pLog, timeoutMS, size);
	if(buf && size!=0)
	{
		// append incoming data
		if( !m_InputBuffer.data )
		{
			m_InputBuffer.capacity = m_InputBuffer.size = size;
			m_InputBuffer.data = new char[m_InputBuffer.capacity];
			memcpy(m_InputBuffer.data, buf, size);
		}
		else
		{
			size_t requiredCapacity = (m_InputBuffer.size + size);
			if(m_InputBuffer.capacity < requiredCapacity)
			{
				// expand
				char *prevBuf = m_InputBuffer.data;
				size_t prevSize = m_InputBuffer.size;
				m_InputBuffer.capacity = m_InputBuffer.size = requiredCapacity;
				m_InputBuffer.data = new char[m_InputBuffer.capacity];
				memcpy(m_InputBuffer.data, prevBuf, prevSize);
				memcpy(&m_InputBuffer.data[prevSize], buf, size);
				delete[] prevBuf;
			}
			else
			{
				// fits
				memcpy(&m_InputBuffer.data[m_InputBuffer.size], buf, size);
				m_InputBuffer.size += size;
			}
		}

		// do we have a complete osc packet?
		int32_t oscPacketLen = 0;
		while(m_InputBuffer.data && m_InputBuffer.size>=sizeof(oscPacketLen))
		{
			memcpy(&oscPacketLen, m_InputBuffer.data, sizeof(oscPacketLen));
			OSCArgument::Swap32(&oscPacketLen);
			if(oscPacketLen < 0)
				oscPacketLen = 0;
			size_t totalSize = (sizeof(oscPacketLen) + static_cast<size_t>(oscPacketLen));
			if(oscPacketLen>0 && m_InputBuffer.size>=totalSize)
			{
				// yup, great success
				char *oscData = &m_InputBuffer.data[sizeof(oscPacketLen)];

				char text[128];
				sprintf(text, "Received Osc Packet [%d]", static_cast<int>(oscPacketLen));
				m_pLog->AddDebug(text);

				m_Parser.PrintPacket(*this, oscData, oscPacketLen);

				sCommand *cmd = new sCommand;

				cmd->buf = new char[oscPacketLen];
				memcpy(cmd->buf, oscData, oscPacketLen);

				// find osc path null terminator
				for(int32_t i=0; i<oscPacketLen; i++)
				{
					if(cmd->buf[i] == 0)
					{
						cmd->path = cmd->buf;
						cmd->argCount = 0xffffffff;
						cmd->args = OSCArgument::GetArgs(cmd->buf , static_cast<size_t>(oscPacketLen), cmd->argCount);
						break;
					}
				}

				cmdQ.push(cmd);

				// shift away processed data
				m_InputBuffer.size -= totalSize;
				if(m_InputBuffer.size != 0)
					memcpy(m_InputBuffer.data, &m_InputBuffer.data[totalSize], m_InputBuffer.size);
			}
			else
			{
				// awaiting more data
				break;
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void EosOsc::Tick(EosTcp &tcp)
{
	if( !m_Q.empty() )
	{
		sQueuedPacket &packet = m_Q.front();
		SendPacket(tcp, packet.data, packet.size);
		m_Q.erase( m_Q.begin() );
	}
}

////////////////////////////////////////////////////////////////////////////////

bool EosOsc::SendPacket(EosTcp &tcp, char *data, size_t size)
{
	bool success = false;

	if(data && size!=0)
	{
		int32_t header = static_cast<int32_t>(size);
		size_t totalSize = (size + sizeof(header));

		if(m_SendPacket.data && m_SendPacket.size<totalSize)
		{
			delete[] m_SendPacket.data;
			m_SendPacket.data = 0;
		}

		if( !m_SendPacket.data )
		{
			m_SendPacket.size = totalSize;
			m_SendPacket.data = new char[m_SendPacket.size];
		}

		memcpy(m_SendPacket.data, &header, sizeof(header));
		OSCArgument::Swap32(m_SendPacket.data);
		memcpy(&m_SendPacket.data[sizeof(header)], data, size);

		if( tcp.Send(*m_pLog,m_SendPacket.data,totalSize) )
		{
			char text[128];
			sprintf(text, "Sent Osc Packet [%d]", static_cast<int>(totalSize));
			m_pLog->AddDebug(text);
			m_Parser.PrintPacket(*this, data, size);

			success = true;
		}

		delete[] data;
	}

	return success;
}

////////////////////////////////////////////////////////////////////////////////

void EosOsc::OSCParserClient_Log(const std::string &message)
{
	m_pLog->AddDebug(message);
}

////////////////////////////////////////////////////////////////////////////////
