# EosSyncLib
A C++ library (with no 3rd party dependencies) for accessing Eos show data in real time

[Eos Integration via OSC.pdf](https://github.com/ETCLabs/EosSyncLib/raw/master/Eos%20Integration%20via%20OSC.pdf)

[Supported OSC Commands](https://github.com/ETCLabs/EosSyncLib/blob/master/Supported%20OSC%20Commands.pdf)

## About this ETCLabs Project
EosSyncLib is open-source software (developed by a combination of end users and ETC employees in their free time) designed to interact with Electronic Theatre Controls products. This is not official ETC software. For challenges using, integrating, compiling, or modifying this software, we encourage posting on the [Issues](https://github.com/ElectronicTheatreControlsLabs/EosSyncLib/issues) page of this project. ETC Support is not familiar with this software and will not be able to assist if issues arise. (Hopefully issues won't happen, and you'll have a lot of fun with these tools and toys!)

We also welcome pull requests for bug fixes and feature additions.

# Simple Example
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
