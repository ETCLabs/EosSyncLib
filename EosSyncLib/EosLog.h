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
#ifndef EOS_LOG_H
#define EOS_LOG_H

#include <vector>
#include <string>

////////////////////////////////////////////////////////////////////////////////

class EosLog
{
public:
	enum EnumLogMsgType
	{
		LOG_MSG_TYPE_DEBUG,
		LOG_MSG_TYPE_INFO,
		LOG_MSG_TYPE_WARNING,
		LOG_MSG_TYPE_ERROR,
		LOG_MSG_TYPE_RECV,
		LOG_MSG_TYPE_SEND
	};

	struct sLogMsg
	{
		EnumLogMsgType	type;
		time_t			timestamp;
		std::string		text;
	};

	typedef std::vector<sLogMsg> LOG_Q;

	EosLog() {}

	void Clear() {m_Q.clear();}
	void Add(EnumLogMsgType type, const std::string &text);
	void AddDebug(const std::string &text) {Add(LOG_MSG_TYPE_DEBUG,text);}
	void AddInfo(const std::string &text) {Add(LOG_MSG_TYPE_INFO,text);}
	void AddWarning(const std::string &text) {Add(LOG_MSG_TYPE_WARNING,text);}
	void AddError(const std::string &text) {Add(LOG_MSG_TYPE_ERROR,text);}
	void AddLog(const EosLog &other) {AddQ(other.m_Q);}
	void AddQ(const LOG_Q &q) {m_Q.insert(m_Q.end(),q.begin(),q.end());}
	void Flush(LOG_Q &q);

private:
	LOG_Q	m_Q;
};

////////////////////////////////////////////////////////////////////////////////

#endif
