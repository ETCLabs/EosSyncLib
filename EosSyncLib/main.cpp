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

#include "EosSyncLib.h"
#include "EosTimer.h"
#include <time.h>
#include <sstream>

////////////////////////////////////////////////////////////////////////////////

void FlushLogQ(EosSyncLib &eosSyncLib)
{
	EosLog::LOG_Q q;
	eosSyncLib.GetLog().Flush(q);
	for(EosLog::LOG_Q::const_iterator i=q.begin(); i!=q.end(); i++)
	{
		const EosLog::sLogMsg &msg = *i;
		const char *str = msg.text.c_str();
		if( str )
		{
			tm *t = localtime( &msg.timestamp );
			const char *type = 0;
			switch( msg.type )
			{
				case EosLog::LOG_MSG_TYPE_DEBUG:	type="Debug"; break;
				case EosLog::LOG_MSG_TYPE_INFO:		type="Info"; break;
				case EosLog::LOG_MSG_TYPE_WARNING:	type="Warning"; break;
				case EosLog::LOG_MSG_TYPE_ERROR:	type="Error"; break;
			}
			if( type )
				printf("[ %.2d:%.2d:%.2d - %s ] %s\n", t->tm_hour, t->tm_min, t->tm_sec, type, str);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void PrintSummary(const EosSyncData &syncData)
{
	const EosSyncData::SHOW_DATA &showData = syncData.GetShowData();

	for(EosSyncData::SHOW_DATA::const_iterator i=showData.begin(); i!=showData.end(); i++)
	{
		EosTarget::EnumEosTargetType targetType = i->first;
		const EosSyncData::TARGETLIST_DATA &listData = i->second;
		const char *targetName = EosTarget::GetNameForTargetType(targetType);

		for(EosSyncData::TARGETLIST_DATA::const_iterator j=listData.begin(); j!=listData.end(); j++)
		{
			int listNumber = j->first;
			const EosTargetList *targetList = j->second;

			if(listNumber > 0)
				printf("%s list %d:\t%d\n", targetName, listNumber, static_cast<int>(targetList->GetNumTargets()));
			else
				printf("%s:\t%d\n", targetName, static_cast<int>(targetList->GetNumTargets()));
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

int main(int argc, char **argv)
{
	char *ip = "127.0.0.1";
	unsigned short port = EosSyncLib::DEFAULT_PORT;

	EosTimer::Init();
	
	printf("Connecting...\n");

	EosSyncLib eosSyncLib;
	if( eosSyncLib.Initialize(ip, port) )
	{
		bool wasConnected = false;
		bool wasSyncd = false;
		
		for(;;)
		{
			eosSyncLib.Tick();
			FlushLogQ(eosSyncLib);

			bool isConnected = eosSyncLib.IsConnected();
			bool isSyncd = (eosSyncLib.GetData().GetStatus().GetValue() == EosSyncStatus::SYNC_STATUS_COMPLETE);

			if(wasConnected != isConnected)
			{
				if( isConnected )
				{
					printf("Connected\n");
					printf("Synchronizing\n");
				}
				else
				{
					printf("Disconnected\n");
					break;
				}
			}

			if( isConnected )
			{
				if(!isSyncd && eosSyncLib.GetData().GetStatus().GetDirty())
					printf(".");

				if(wasSyncd != isSyncd)
				{
					if( isSyncd )
					{
						printf("Synchronized\n");
						PrintSummary( eosSyncLib.GetData() );
					}
					else
						printf("Not Synchronized...\n");
				}

				eosSyncLib.ClearDirty();
			}
			
			fflush(stdout);
			
			wasConnected = isConnected;
			wasSyncd = isSyncd;
			
			EosTimer::SleepMS(10);
		}

		eosSyncLib.Shutdown();
	}

	return 0;
}

////////////////////////////////////////////////////////////////////////////////
