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
#ifndef EOS_SYNC_LIB_H
#define EOS_SYNC_LIB_H

#ifndef EOS_LOG_H
#include "EosLog.h"
#endif

#ifndef EOS_OSC_H
#include "EosOsc.h"
#endif

#include <map>
#include <string>

class EosTcp;

////////////////////////////////////////////////////////////////////////////////

class EosSyncStatus
{
public:
	enum EnumSyncStatus
	{
		SYNC_STATUS_UNINTIALIZED,
		SYNC_STATUS_RUNNING,
		SYNC_STATUS_COMPLETE
	};

	EosSyncStatus();

	virtual EnumSyncStatus GetValue() const {return m_Value;}
	virtual void SetValue(EnumSyncStatus value);
	virtual bool GetDirty() const {return m_Dirty;}
	virtual void SetDirty() {m_Dirty = true;}
	virtual void ClearDirty() {m_Dirty = false;}
	virtual const time_t& GetTimestamp() const {return m_Timestamp;}
	virtual void SetTimeStamp(const time_t &timestamp) {m_Timestamp = timestamp;}
	virtual void ResetTimestamp();
	virtual void UpdateFromChild(const EosSyncStatus &childStatus);

private:
	EnumSyncStatus	m_Value;
	bool			m_Dirty;
	time_t			m_Timestamp;
};

////////////////////////////////////////////////////////////////////////////////

class EosTarget
{
public:
	enum EnumEosTargetType
	{
		EOS_TARGET_PATCH		= 0,
		EOS_TARGET_CUELIST,
		EOS_TARGET_CUE,
		EOS_TARGET_GROUP,
		EOS_TARGET_MACRO,
		EOS_TARGET_SUB,
		EOS_TARGET_PRESET,
		EOS_TARGET_IP,
		EOS_TARGET_FP,
		EOS_TARGET_CP,
		EOS_TARGET_BP,
		EOS_TARGET_CURVE,
		EOS_TARGET_FX,
		EOS_TARGET_SNAP,
		EOS_TARGET_PIXMAP,
		EOS_TARGET_MS,

		EOS_TARGET_COUNT,
		EOS_TARGET_INVALID
	};

	struct sDecimalNumber
	{
		sDecimalNumber() : whole(0), decimal(0) {}
		sDecimalNumber(int Whole) : whole(Whole), decimal(0) {}
		sDecimalNumber(int Whole, int Decimal) : whole(Whole), decimal(Decimal) {}
		bool operator==(const sDecimalNumber &other) const;
		bool operator<(const sDecimalNumber &other) const;
		int whole;
		int decimal;
	};
	
	struct sTargetKey
	{
		sTargetKey() : part(0) {}
		sTargetKey(const sDecimalNumber &Num, int Part) : num(Num), part(Part) {}
		bool operator==(const sTargetKey &other) const;
		bool operator<(const sTargetKey &other) const;
		bool valid() const {return ((num.whole>=1) || (num.whole>=0 && num.decimal>0));}
		sDecimalNumber	num;
		int				part;
	};

	struct sPathData
	{
		sPathData() : isList(false) {}
		sTargetKey		key;
		std::string		group;
		bool			isList;
		unsigned int	listIndex;
		unsigned int	listSize;
	};

	struct sProperty
	{
		sProperty() : initialized(false) {}
		bool		initialized;
		std::string	value;
	};

	typedef std::vector<sProperty> PROPS;

	struct sPropertyGroup
	{
		sPropertyGroup() : initialized(false) {}
		bool	initialized;
		PROPS	props;
	};

	typedef std::map<std::string, sPropertyGroup> PROP_GROUPS;

	EosTarget(EnumEosTargetType type);
	virtual void Clear();
	virtual const EosSyncStatus& GetStatus() const {return m_Status;}
	virtual void Recv(EosLog &log, EosOsc::sCommand &command, const sPathData &pathData);
	virtual const PROP_GROUPS& GetPropGroups() const {return m_PropGroups;}
	virtual void ClearDirty();

	static const char* GetNameForTargetType(EnumEosTargetType type);
	static void InitializePropGroupsForTargetType(EnumEosTargetType type, PROP_GROUPS &propGroups);
	static bool ExtractPathData(const std::string &path, size_t offset, sPathData &pathData);
	static bool GetNumberFromString(const std::string &str, sDecimalNumber &num);
	static void GetStringFromNumber(const sDecimalNumber &num, std::string &str);

private:
	EosSyncStatus	m_Status;
	PROP_GROUPS		m_PropGroups;

	EosTarget& operator=(const EosTarget&) {return *this;}	// not allowed
};

////////////////////////////////////////////////////////////////////////////////

class EosTargetList
{
public:
	struct sInitialSyncInfo
	{
		sInitialSyncInfo()
			: count(0)
			, complete(false)
		{}
		size_t	count;
		bool	complete;
	};

	typedef std::map<int,EosTarget*> PARTS;

	struct sParts
	{
		sParts() : initialized(false) {}
		bool	initialized;
		PARTS	list;
	};

	typedef std::map<EosTarget::sDecimalNumber,sParts> TARGETS;
	typedef std::map<std::string,EosTarget*> UID_LOOKUP;

	EosTargetList(EosTarget::EnumEosTargetType type, int listId);
	virtual ~EosTargetList();
	virtual void Clear();
	virtual EosTarget::EnumEosTargetType GetType() const {return m_Type;}
	virtual int GetListId() const {return m_ListId;}
	virtual const EosSyncStatus& GetStatus() const {return m_Status;}
	virtual void Tick(EosTcp &tcp, EosOsc &osc);
	virtual void Recv(EosTcp &tcp, EosOsc &osc, EosLog &log, EosOsc::sCommand &command);
	virtual void Notify(EosLog &log, EosOsc::sCommand &command);
	virtual void ClearDirty();
	virtual const TARGETS& GetTargets() const {return m_Targets;}
	virtual const UID_LOOKUP& GetUIDLookup() const {return m_UIDLookup;}
	virtual size_t GetNumTargets() const {return m_NumTargets;}
	virtual const sInitialSyncInfo& GetInitialSync() const {return m_InitialSync;}
	virtual void InitializeAsDummy();

	static const EosTargetList	sm_InvalidTargetList;

private:
	const EosTarget::EnumEosTargetType	m_Type;
	const int							m_ListId;
	TARGETS								m_Targets;
	size_t								m_NumTargets;
	UID_LOOKUP							m_UIDLookup;
	EosSyncStatus						m_Status;
	EosSyncStatus						m_StatusInternal;	// used for getting target count only
	sInitialSyncInfo					m_InitialSync;

	virtual void DeleteTarget(EosTarget *target);
	virtual void ProcessReceviedTarget(EosLog &log, EosOsc::sCommand &command, const EosTarget::sPathData &pathData);

	EosTargetList& operator=(const EosTargetList&) {return *this;}		// not allowed
};

////////////////////////////////////////////////////////////////////////////////

class EosSyncData
{
public:
	typedef std::map<int, EosTargetList*> TARGETLIST_DATA;
	typedef std::map<EosTarget::EnumEosTargetType, TARGETLIST_DATA> SHOW_DATA;

	EosSyncData();
	virtual ~EosSyncData();

	virtual void Clear();
	virtual void Tick(EosTcp &tcp, EosOsc &osc, EosLog &log);
	virtual const EosSyncStatus& GetStatus() const {return m_Status;}
	virtual const SHOW_DATA& GetShowData() const {return m_ShowData;}
	virtual const EosTargetList* GetTargetList(EosTarget::EnumEosTargetType type, int listId) const;
	virtual void ClearDirty();

private:
	EosSyncStatus	m_Status;
	SHOW_DATA		m_ShowData;

	virtual void Initialize();
	virtual void TickRunning(EosTcp &tcp, EosOsc &osc, EosLog &log);
	virtual void Recv(EosTcp &tcp, EosOsc &osc, EosLog &log);
	virtual void RecvCmd(EosTcp &tcp, EosOsc &osc, EosLog &log, EosOsc::sCommand &command);
	virtual void OnTargeListInitialSyncComplete(EosTargetList &targetList);
	virtual void RemoveOrphanedCues();
};


////////////////////////////////////////////////////////////////////////////////

class EosSyncLib
{
public:
	enum EnumConstants
	{
		DEFAULT_PORT	= 3032
	};

	EosSyncLib();
	virtual ~EosSyncLib();

	virtual bool Initialize(const char *ip, unsigned short port);
	virtual void Shutdown();
	virtual void Tick();
	virtual bool IsRunning() const;
	virtual bool IsConnected() const;
	virtual bool IsSynchronized() const;
	virtual bool IsConnectedAndSynchronized() const {return (IsConnected() && IsSynchronized());}
	virtual EosLog& GetLog() {return m_Log;}
	virtual const EosSyncData& GetData() const {return m_Data;}
	virtual void ClearDirty() { m_Data.ClearDirty(); }
	virtual bool Send(OSCPacketWriter &packet, bool immediate);

	// convenience
	virtual const EosTargetList& GetPatch() const;
	virtual const EosTargetList& GetCueList() const;
	virtual const EosTargetList& GetCue(int listId) const;
	virtual const EosTargetList& GetGroups() const;
	virtual const EosTargetList& GetMacros() const;
	virtual const EosTargetList& GetSubs() const;
	virtual const EosTargetList& GetPresets() const;
	virtual const EosTargetList& GetIntensityPalettes() const;
	virtual const EosTargetList& GetFocusPalettes() const;
	virtual const EosTargetList& GetColorPalettes() const;
	virtual const EosTargetList& GetBeamPalettes() const;
	virtual const EosTargetList& GetCurves() const;
	virtual const EosTargetList& GetEffects() const;
	virtual const EosTargetList& GetSnapshots() const;
	virtual const EosTargetList& GetPixelMaps() const;
	virtual const EosTargetList& GetMagicSheets() const;

protected:
	EosLog		m_Log;
	EosTcp		*m_Tcp;
	EosOsc		*m_Osc;
	EosSyncData	m_Data;
};

////////////////////////////////////////////////////////////////////////////////

#endif
