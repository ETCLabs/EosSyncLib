# EosSyncLib
A C++ library (with no 3rd party dependencies) for accessing Eos show data in real time

[Eos Integration via OSC.pdf](https://github.com/ElectronicTheatreControlsLabs/EosSyncLib/raw/master/Eos%20Integration%20via%20OSC.pdf)

[EosFamily_ShowControl_UserGuide_RevB](http://www.etcconnect.com/WorkArea/DownloadAsset.aspx?id=10737461372)

#Simple Example
```C++
#include "EosSyncLib.h"
#include "EosTimer.h"

int main(int argc, char **argv)
{
	// initialize library & connect to Eos
	EosTimer::Init();
	EosSyncLib eosSyncLib;
	if( eosSyncLib.Initialize("127.0.0.1",EosSyncLib::DEFAULT_PORT) )
	{
		// synchronize with Eos
		do
		{
			eosSyncLib.Tick();
			EosTimer::SleepMS(10);
		}
		while( !eosSyncLib.IsConnectedAndSynchronized() );

		// print all groups in the show
		std::string str;
		const EosTargetList::TARGETS &groups = eosSyncLib.GetGroups().GetTargets();
		for(EosTargetList::TARGETS::const_iterator i=groups.begin(); i!=groups.end(); i++)		
		{
			const EosTarget::sDecimalNumber &groupNumber = i->first;
			EosTarget::GetStringFromNumber(groupNumber, str);
			printf("Group %s\n", str.c_str());
		}

		eosSyncLib.Shutdown();
	}
	return 0;
}
```
