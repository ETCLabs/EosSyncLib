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
#include "EosTcp.h"

#include <time.h>
#include <set>

////////////////////////////////////////////////////////////////////////////////

#define TCP_RECV_TIMEOUT	10
#define TARGET_DECIMALS		3

////////////////////////////////////////////////////////////////////////////////

EosSyncStatus::EosSyncStatus()
	: m_Value(SYNC_STATUS_UNINTIALIZED)
	, m_Dirty(false)
{
	ResetTimestamp();
}

////////////////////////////////////////////////////////////////////////////////

void EosSyncStatus::SetValue(EnumSyncStatus value)
{
	if(m_Value != value)
	{
		m_Value = value;
		m_Dirty = true;
	}

	ResetTimestamp();
}

////////////////////////////////////////////////////////////////////////////////

void EosSyncStatus::ResetTimestamp()
{
	m_Timestamp = time(0);
}

////////////////////////////////////////////////////////////////////////////////

void EosSyncStatus::UpdateFromChild(const EosSyncStatus &childStatus)
{
	if(m_Value==SYNC_STATUS_COMPLETE && childStatus.m_Value!=SYNC_STATUS_COMPLETE)
		SetValue(SYNC_STATUS_RUNNING);

	if( childStatus.m_Dirty )
	{
		m_Dirty = true;
		if(childStatus.m_Timestamp > m_Timestamp)
			m_Timestamp = childStatus.m_Timestamp;
	}
}

////////////////////////////////////////////////////////////////////////////////

bool EosTarget::sDecimalNumber::operator==(const sDecimalNumber &other) const
{
	return (whole==other.whole && decimal==other.decimal);
}

////////////////////////////////////////////////////////////////////////////////

bool EosTarget::sDecimalNumber::operator<(const sDecimalNumber &other) const
{
	return ((whole==other.whole) ? (decimal<other.decimal) : (whole<other.whole));
}

////////////////////////////////////////////////////////////////////////////////

bool EosTarget::sTargetKey::operator==(const sTargetKey &other) const
{
	return (num==other.num && part==other.part);
}

////////////////////////////////////////////////////////////////////////////////

bool EosTarget::sTargetKey::operator<(const sTargetKey &other) const
{
	return ((num==other.num) ? (part<other.part) : (num<other.num));
}

////////////////////////////////////////////////////////////////////////////////

EosTarget::EosTarget(EnumEosTargetType type)
{
	InitializePropGroupsForTargetType(type, m_PropGroups);
}

////////////////////////////////////////////////////////////////////////////////

void EosTarget::Clear()
{
	for(PROP_GROUPS::iterator i=m_PropGroups.begin(); i!=m_PropGroups.end(); i++)
	{
		sPropertyGroup &group = i->second;
		group.props.clear();
		group.initialized = false;
	}
}

////////////////////////////////////////////////////////////////////////////////

void EosTarget::Recv(EosLog &log, EosOsc::sCommand &command, const sPathData &pathData)
{
	switch( m_Status.GetValue() )
	{
		case EosSyncStatus::SYNC_STATUS_UNINTIALIZED:
			m_Status.SetValue(EosSyncStatus::SYNC_STATUS_RUNNING);
			// intentional fall-through

		case EosSyncStatus::SYNC_STATUS_RUNNING:
			{
				PROP_GROUPS::iterator i = m_PropGroups.find(pathData.group);
				if(i != m_PropGroups.end())
				{
					sPropertyGroup &group = i->second;
					size_t propOffset = 0;

					size_t numProps = (pathData.isList ? pathData.listSize : command.argCount);
					if(numProps != 0)
						group.props.resize(numProps);
					if( group.initialized )
					{
						if(group.props.size() != numProps)
						{
							std::string text("invalid reply \"");
							text.append( command.path );
							text.append("\", existing property count ");
							char buf[33];
							sprintf(buf, "%d", static_cast<int>(group.props.size()));
							text.append(buf);
							text.append(" does not match new count ");
							sprintf(buf, "%d", static_cast<int>(numProps));
							text.append(buf);
							log.AddError(text);
						}
					}
					else
						group.initialized = true;

					if( command.args )
					{
						for(size_t j=0; j<command.argCount; j++)
						{
							size_t propIndex = (propOffset + j);
							if(propIndex < group.props.size())
							{
								sProperty &prop = group.props[propIndex];
								if( !command.args[j].GetString(prop.value) )
									prop.value.clear();
								prop.initialized = true;
							}
							else
							{
								std::string text("invalid property \"");
								text.append( command.path );
								text.append("\" at index ");
								char buf[33];
								sprintf(buf, "%d", static_cast<int>(propIndex));
								text.append(buf);
								text.append(" of ");
								sprintf(buf, "%d", static_cast<int>(group.props.size()));
								text.append(buf);
								log.AddError(text);
							}
						}
					}

					// did we get all the properties we are expecting?
					bool gotAllProps = true;
					for(PROP_GROUPS::const_iterator j=m_PropGroups.begin(); j!=m_PropGroups.end() && gotAllProps; j++)
					{
						const sPropertyGroup &g = j->second;
						if( g.initialized )
						{
							for(PROPS::const_iterator k=g.props.begin(); k!=g.props.end() && gotAllProps; k++)
							{
								if( !k->initialized )
									gotAllProps = false;
							}
						}
						else
							gotAllProps = false;
					}

					if( gotAllProps )
						m_Status.SetValue(EosSyncStatus::SYNC_STATUS_COMPLETE);	// yup!
				}
				else
				{
					std::string text("ignored reply \"");
					text.append( command.path );
					text.append("\", unexpected property group");
					log.AddWarning(text);
				}
			}
			break;

		default:
			{
				std::string text("ignored unsolicited reply \"");
				text.append( command.path );
				text.append("\"");
				log.AddInfo(text);
			}
			break;
	}
}

////////////////////////////////////////////////////////////////////////////////

void EosTarget::ClearDirty()
{
	m_Status.ClearDirty();
}

////////////////////////////////////////////////////////////////////////////////

const char* EosTarget::GetNameForTargetType(EnumEosTargetType type)
{
	switch( type )
	{
		case EOS_TARGET_PATCH:		return "patch";
		case EOS_TARGET_CUELIST:	return "cuelist";
		case EOS_TARGET_CUE:		return "cue";
		case EOS_TARGET_GROUP:		return "group";
		case EOS_TARGET_MACRO:		return "macro";
		case EOS_TARGET_SUB:		return "sub";
		case EOS_TARGET_PRESET:		return "preset";
		case EOS_TARGET_IP:			return "ip";
		case EOS_TARGET_FP:			return "fp";
		case EOS_TARGET_CP:			return "cp";
		case EOS_TARGET_BP:			return "bp";
		case EOS_TARGET_CURVE:		return "curve";
		case EOS_TARGET_FX:			return "fx";
		case EOS_TARGET_SNAP:		return "snap";
		case EOS_TARGET_PIXMAP:		return "pixmap";
		case EOS_TARGET_MS:			return "ms";
	}

	return "";
}

////////////////////////////////////////////////////////////////////////////////

void EosTarget::InitializePropGroupsForTargetType(EnumEosTargetType type, PROP_GROUPS &propGroups)
{
	propGroups.clear();
	std::string general;
	sPropertyGroup emptyGroup;

	switch( type )
	{
		case EOS_TARGET_PATCH:
			propGroups[general] = emptyGroup;
			propGroups["notes"] = emptyGroup;
			break;

		case EOS_TARGET_CUELIST:
			propGroups[general] = emptyGroup;
			propGroups["links"] = emptyGroup;
			break;

		case EOS_TARGET_CUE:
			propGroups[general] = emptyGroup;
			propGroups["fx"] = emptyGroup;
			propGroups["links"] = emptyGroup;
			propGroups["actions"] = emptyGroup;
			break;

		case EOS_TARGET_GROUP:
			propGroups[general] = emptyGroup;
			propGroups["channels"] = emptyGroup;
			break;

		case EOS_TARGET_MACRO:
			propGroups[general] = emptyGroup;
			propGroups["text"] = emptyGroup;
			break;

		case EOS_TARGET_SUB:
			propGroups[general] = emptyGroup;
			propGroups["fx"] = emptyGroup;
			break;

		case EOS_TARGET_PRESET:
			propGroups[general] = emptyGroup;
			propGroups["channels"] = emptyGroup;
			propGroups["byType"] = emptyGroup;
			propGroups["fx"] = emptyGroup;
			break;

		case EOS_TARGET_IP:
		case EOS_TARGET_FP:
		case EOS_TARGET_CP:
		case EOS_TARGET_BP:
			propGroups[general] = emptyGroup;
			propGroups["channels"] = emptyGroup;
			propGroups["byType"] = emptyGroup;
			break;

		case EOS_TARGET_CURVE:
		case EOS_TARGET_FX:
		case EOS_TARGET_SNAP:
		case EOS_TARGET_MS:
			propGroups[general] = emptyGroup;
			break;

		case EOS_TARGET_PIXMAP:
			propGroups[general] = emptyGroup;
			propGroups["channels"] = emptyGroup;
			break;
	}
}

////////////////////////////////////////////////////////////////////////////////

bool EosTarget::ExtractPathData(const std::string &path, size_t offset, sPathData &pathData)
{
	// expecting the following variants:
	//
	// target only:
	// <id>
	// <id>/<part>
	//
	// target + group:
	// <id>/<group>
	// <id>/<part>/<group>
	//
	// target + list
	// <id>/list/<index>/<total>
	// <id>/<part>/list/<index>/<total>
	//
	// target + group + list
	// <id>/<group>/list/<index>/<total>
	// <id>/<part>/<group>/list/<index>/<total>
	
	std::string part;
	bool gotId = false;
	bool gotPart = false;
	bool gotList = false;
	bool gotListIndex = false;
	for(size_t i=offset; i<path.size(); )
	{
		// get next part
		part.clear();
		size_t n = path.find("/", i);
		if(n != std::string::npos)
		{
			size_t len = (n-i);
			if(len != 0)
				part = path.substr(i, n-i);
		}
		else
			part = path.substr(i, path.size()-i);
		
		if( !part.empty() )
		{
			// is it a number?
			sDecimalNumber num;
			if( GetNumberFromString(part,num) )
			{
				if( gotList )
				{
					if( gotListIndex )
					{
						// list total
						if(num.whole>=0 && num.decimal==0)
						{
							pathData.listSize = static_cast<unsigned int>(num.whole);
							return true;	// done
						}
						else
						{
							// invalid list total
							return false;
						}
					}
					else
					{
						// list index
						if(num.whole>=0 && num.decimal==0)
						{
							pathData.listIndex = static_cast<unsigned int>(num.whole);
							gotListIndex = true;
						}
						else
						{
							// invalid list index
							return false;
						}
					}
				}
				else if( gotId )
				{
					if( gotPart )
					{
						// unhandled number
						return false;
					}
					else if(num.decimal == 0)
					{
						pathData.key.part = num.whole;
						gotPart = true;
					}
					else
					{
						// part cannot have decimal
						return false;
					}
				}
				else
				{
					// got id
					pathData.key.num = num;
					gotId = true;
				}
			}
			else if( gotId )
			{
				// must be list for group
				if(part == "list")
				{
					if( gotList )
					{
						// 2nd list keyword!?
						return false;
					}
					else
						gotList = true;
				}
				else
					pathData.group = part;
			}
			else
			{
				// must start with id
				return false;
			}
		}
		
		i += (part.size() + 1);
	}
	
	// must have at least an id, and a list would have returned true once it got the list size above
	return (gotId && !gotList);
}

////////////////////////////////////////////////////////////////////////////////

bool EosTarget::GetNumberFromString(const std::string &str, sDecimalNumber &num)
{
	// get id portion
	if( !str.empty() )
	{
		size_t n = str.find(".");
		if(n == std::string::npos)
		{
			// whole number
			const char *s = str.c_str();
			if( OSCArgument::IsIntString(s) )
			{
				num.whole = atoi(s);
				num.decimal = 0;
				return true;
			}
		}
		else
		{
			// decimal number
			bool decimalNegative = false;
			if(n != 0)
			{
				std::string wholeStr(str, 0, n);
				const char *s = wholeStr.c_str();
				if(wholeStr == "-")
				{
					num.whole = 0;
					decimalNegative = true;
				}
				else if( OSCArgument::IsIntString(s) )
				{
					num.whole = atoi(s);
					if(num.whole==0 && s && (*s)=='-')
						decimalNegative = true;
				}
				else
					return false;
			}
			else
				num.whole = 0;
			
			if(++n < str.size())
			{
				std::string decimalStr( str.substr(n,str.size()-n) );
				const char *s = decimalStr.c_str();

				if( s )
				{
					num.decimal = 0;

					int exp = TARGET_DECIMALS;
					for(const char *p=s; *p; p++)
					{
						if(--exp < 0)
							break;

						int digit = (*p - '0');
						if(digit<0 || digit>9)
							return false;	// non-digit

						num.decimal += (digit * static_cast<int>(pow(10.0,exp)));
					}

					if(num.decimal>0 && decimalNegative)
						num.decimal *= -1;

					return true;
				}
			}
			else
				num.decimal = 0;
			
			return true;
		}
	}
	
	return false;
}

////////////////////////////////////////////////////////////////////////////////

void EosTarget::GetStringFromNumber(const sDecimalNumber &num, std::string &str)
{
	str.clear();

	if(num.whole==0 && num.decimal<0)
		str = "-";

	char buf[64];
	sprintf(buf, "%d", num.whole);
	str.append(buf);

	if(num.decimal != 0)
	{
		int n = abs(num.decimal);
		if(n < pow(10.0,TARGET_DECIMALS))
		{
			char fmt[64];
			strcpy(fmt, "%.");
			sprintf(&fmt[2], "%d", TARGET_DECIMALS);
			strcat(fmt, "d");
			sprintf(buf, fmt, n);
			// chop trailing zeroes
			for(int i=(static_cast<int>(strlen(buf))-1); i>=0; i--)
			{
				if(buf[i] == '0')
					buf[i] = 0;
				else
					break;
			}
			if( *buf )
			{
				str.append(".");
				str.append(buf);
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

const EosTargetList	EosTargetList::sm_InvalidTargetList(EosTarget::EOS_TARGET_INVALID, 0);

////////////////////////////////////////////////////////////////////////////////

EosTargetList::EosTargetList(EosTarget::EnumEosTargetType type, int listId)
	: m_Type(type)
	, m_ListId(listId)
	, m_NumTargets(0)
{
}

////////////////////////////////////////////////////////////////////////////////

EosTargetList::~EosTargetList()
{
	Clear();
}

////////////////////////////////////////////////////////////////////////////////

void EosTargetList::Clear()
{
	for(TARGETS::const_iterator i=m_Targets.begin(); i!=m_Targets.end(); i++)
	{
		const PARTS &parts = i->second.list;
		for(PARTS::const_iterator j=parts.begin(); j!=parts.end(); j++)
			delete j->second;
	}
	m_Targets.clear();
	m_NumTargets = 0;
	m_UIDLookup.clear();
	m_InitialSync = sInitialSyncInfo();
	m_Status.SetValue(EosSyncStatus::SYNC_STATUS_UNINTIALIZED);
	m_StatusInternal.SetValue(EosSyncStatus::SYNC_STATUS_UNINTIALIZED);
}

////////////////////////////////////////////////////////////////////////////////

void EosTargetList::Tick(EosTcp &tcp, EosOsc &osc)
{
	switch( m_StatusInternal.GetValue() )
	{
		case EosSyncStatus::SYNC_STATUS_UNINTIALIZED:
			{
				std::string path("/eos/get/");
				path.append( EosTarget::GetNameForTargetType(m_Type) );
				if(m_Type == EosTarget::EOS_TARGET_CUE)
				{
					path.append("/");
					char buf[33];
					sprintf(buf, "%d", m_ListId);					
					path.append(buf);
				}
				path.append("/count");

				if( osc.Send(tcp,OSCPacketWriter(path),/*immediate*/false) )
					m_StatusInternal.SetValue(EosSyncStatus::SYNC_STATUS_RUNNING);
			}
			break;

		case EosSyncStatus::SYNC_STATUS_COMPLETE:
			{
				if(m_Status.GetValue() == EosSyncStatus::SYNC_STATUS_RUNNING)
				{
					bool allTargetsComplete = true;

					for(TARGETS::iterator i=m_Targets.begin(); i!=m_Targets.end(); i++)
					{
						sParts &parts = i->second;
						if( !parts.initialized )
						{
							// Notify created a placeholder for a newly added target, request info
							std::string path("/eos/get/");
							path.append( EosTarget::GetNameForTargetType(m_Type) );
							if(m_Type == EosTarget::EOS_TARGET_CUE)
							{
								path.append("/");
								char buf[33];
								sprintf(buf, "%d", m_ListId);					
								path.append(buf);
							}

							std::string numStr;
							EosTarget::GetStringFromNumber(i->first, numStr);
							if( !numStr.empty() )
							{
								path.append("/");
								path.append(numStr);
								if( osc.Send(tcp,OSCPacketWriter(path),/*immediate*/false) )
									parts.initialized = true;
							}

							allTargetsComplete = false;
						}
						else if( parts.list.empty() )
						{
							// waiting for reply
							allTargetsComplete = false;
						}
						else
						{
							for(PARTS::const_iterator j=parts.list.begin(); j!=parts.list.end(); j++)
							{
								EosTarget *target = j->second;
								if(target->GetStatus().GetValue() != EosSyncStatus::SYNC_STATUS_COMPLETE)
									allTargetsComplete = false;
							}
						}
					}

					if( allTargetsComplete )
					{
						if( m_InitialSync.complete )
						{
							m_Status.SetValue(EosSyncStatus::SYNC_STATUS_COMPLETE);
						}
						else if(m_NumTargets >= m_InitialSync.count)
						{
							m_InitialSync.complete = true;
							m_Status.SetValue(EosSyncStatus::SYNC_STATUS_COMPLETE);
						}
					}
				}
			}
			break;
	}
}

////////////////////////////////////////////////////////////////////////////////

void EosTargetList::Recv(EosTcp &tcp, EosOsc &osc, EosLog &log, EosOsc::sCommand &command)
{
	switch( m_StatusInternal.GetValue() )
	{
		case EosSyncStatus::SYNC_STATUS_RUNNING:
			{
				std::string path("/eos/out/get/");
				path.append( EosTarget::GetNameForTargetType(m_Type) );
				if(m_Type == EosTarget::EOS_TARGET_CUE)
				{
					path.append("/");
					char buf[33];
					sprintf(buf, "%d", m_ListId);
					path.append(buf);
				}
				std::string countPath(path);
				countPath.append("/count");
				if(command.path == countPath)
				{
					if(command.args && command.argCount!=0)
					{
						unsigned int count = 0;
						if( !command.args[0].GetUInt(count) )
						{
							count = 0;
							std::string text("ignored reply \"");
							text.append( command.path );
							text.append("\", missing argument");
							log.AddError(text);
						}

						m_InitialSync.count = count;

						// request all targets
						path = "/eos/get/";
						path.append( EosTarget::GetNameForTargetType(m_Type) );
						if(m_Type == EosTarget::EOS_TARGET_CUE)
						{
							path.append("/");
							char buf[33];
							sprintf(buf, "%d", m_ListId);
							path.append(buf);
						}
						path.append("/index/");
						for(size_t i=0; i<m_InitialSync.count; i++)
						{
							char buf[33];
							sprintf(buf, "%u", static_cast<unsigned int>(i));
							std::string indexPath(path);
							indexPath.append(buf);

							if( !osc.Send(tcp,OSCPacketWriter(indexPath),/*immediate*/false) )
							{
								std::string text("failed to send command \"");
								text.append(indexPath);
								text.append("\"");
								log.AddError(text);
							}
						}

						m_Status.SetValue(EosSyncStatus::SYNC_STATUS_RUNNING);
						m_StatusInternal.SetValue(EosSyncStatus::SYNC_STATUS_COMPLETE);
					}
				}
				else
				{
					std::string text("ignored reply \"");
					text.append( command.path );
					text.append("\", unhandled command");
					log.AddError(text);
				}
			}
			break;

		case EosSyncStatus::SYNC_STATUS_COMPLETE:
			{
				std::string path("/eos/out/get/");
				path.append( EosTarget::GetNameForTargetType(m_Type) );
				if(m_Type == EosTarget::EOS_TARGET_CUE)
				{
					path.append("/");
					char buf[33];
					sprintf(buf, "%d", m_ListId);
					path.append(buf);
				}
				path.append("/");
				if(command.path.find(path) == 0)
				{
					// extract path data (target & list info)
					EosTarget::sPathData pathData;
					if( EosTarget::ExtractPathData(command.path,path.size(),pathData) )
					{
						if( pathData.key.valid() )
						{
							ProcessReceviedTarget(log, command, pathData);
						}
						else
						{
							std::string text("ignored reply \"");
							text.append( command.path );
							text.append("\", invalid target");
							log.AddError(text);
						}
					}
					else
					{
						std::string text("ignored reply \"");
						text.append( command.path );
						text.append("\", could not extract target");
						log.AddError(text);
					}
				}
				else
				{
					std::string text("ignored reply \"");
					text.append( command.path );
					text.append("\", unexpected format");
					log.AddError(text);
				}
			}
			break;

		default:
			{
				std::string text("ignored unsolicited reply \"");
				text.append( command.path );
				text.append("\"");
				log.AddInfo(text);
			}
			break;
	}
}

////////////////////////////////////////////////////////////////////////////////

void EosTargetList::ProcessReceviedTarget(EosLog &log, EosOsc::sCommand &command, const EosTarget::sPathData &pathData)
{
	int part = pathData.key.part;
	switch( m_Type )
	{
		case EosTarget::EOS_TARGET_CUE:
			{
				if(part < 0)
				{
					std::string text("Invalid part number specified \"");
					text.append( command.path );
					text.append("\"");
					log.AddError(text);
					return;
				}
			}
			break;

		case EosTarget::EOS_TARGET_PATCH:
			{
				if(part < 1)
				{
					std::string text("Invalid part number specified \"");
					text.append( command.path );
					text.append("\"");
					log.AddError(text);
					return;
				}
			}
			break;

		default:
			{
				if(part != 0)
				{
					std::string text("Invalid part number specified \"");
					text.append( command.path );
					text.append("\"");
					log.AddWarning(text);
					part = 0;
				}
			}
			break;
	}

	bool baseTargetInfo = pathData.group.empty();

	// get UID
	std::string uid;
	if(baseTargetInfo && command.args && command.argCount>1)
	{
		if( !command.args[1].GetString(uid) )
			uid.clear();
	}

	// add or remove?	
	if(baseTargetInfo && uid.empty())
	{
		// remove target
		TARGETS::iterator i = m_Targets.find( pathData.key.num );
		if(i != m_Targets.end())
		{
			PARTS &parts = i->second.list;
			PARTS::iterator j = parts.find(part);
			if(j != parts.end())
			{
				DeleteTarget( j->second );
				parts.erase(j);
				m_Status.SetDirty();
			}

			if( parts.empty() )
				m_Targets.erase(i);
		}
	}
	else
	{
		// add target
		bool added = false;
		EosTarget *target = 0;
		TARGETS::iterator i = m_Targets.find( pathData.key.num );
		if(i == m_Targets.end())
		{
			// nope, add it
			sParts parts;
			parts.initialized = true;
			target = new EosTarget(m_Type);
			parts.list[part] = target;
			m_Targets[ pathData.key.num ] = parts;
			added = true;
		}
		else
		{
			sParts &parts = i->second;
			parts.initialized = true;
			PARTS::iterator j = parts.list.find(part);
			if(j == parts.list.end())
			{
				// nope, add it
				target = new EosTarget(m_Type);
				parts.list[part] = target;
				added = true;
			}
			else
				target = j->second;
		}

		// forward incoming data to target
		if( target )
		{
			if( added )
			{
				m_NumTargets++;

				if( uid.empty() )
				{
					std::string text("target reply missing UID \"");
					text.append( command.path );
					text.append("\"");
					log.AddError(text);
				}
				else
					m_UIDLookup[uid] = target;

				m_Status.SetDirty();
			}

			target->Recv(log, command, pathData);
			m_Status.UpdateFromChild( target->GetStatus() );
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void EosTargetList::Notify(EosLog &log, EosOsc::sCommand &command)
{
	if( m_InitialSync.complete )
	{
		// extract targets from arguments
		if(command.args && command.argCount>1)		// NOTE: first arg is sequence number for UDP-only
		{
			bool badArgs = false;
			std::set<EosTarget::sDecimalNumber> targets;
			for(size_t i=1; i<command.argCount; i++)
			{
				OSCArgument &arg = command.args[i];
				double d;
				if( arg.GetDouble(d) )
				{
					std::string str;
					if( arg.GetString(str) )
					{
						EosTarget::sDecimalNumber num;
						EosTarget::GetNumberFromString(str, num);
						targets.insert(num);
					}
				}
				else
				{
					// not a simple number, so expecting a range "a-b"
					bool gotRange = false;
					
					std::string str;
					if( arg.GetString(str) )
					{
						if(str.size() > 2)
						{
							size_t n = str.find("-");
							if(	n!=std::string::npos &&
								n!=0 &&
								n<(str.size()-1) )
							{
								std::string range(str, 0, n++);
								const char *s = range.c_str();
								if( OSCArgument::IsIntString(s) )
								{
									int rangeStart = atoi(s);
									range = str.substr(n, str.size()-n);
									s = range.c_str();
									if( OSCArgument::IsIntString(s) )
									{
										int rangeEnd = atoi(s);
										if(rangeStart <= rangeEnd)
										{
											for(int j=rangeStart; j<=rangeEnd; j++)
												targets.insert(j);
											gotRange = true;
										}
									}
								}
							}
						}
					}
					
					if( !gotRange )
						badArgs = true;
				}
			}

			if( badArgs )
			{
				std::string text("invalid arguments in notify \"");
				text.append( command.path );
				text.append("\"");
				log.AddError(text);
			}
			else
			{
				for(std::set<EosTarget::sDecimalNumber>::const_iterator i=targets.begin(); i!=targets.end(); i++)
				{
					const EosTarget::sDecimalNumber &targetNumber = *i;
					TARGETS::iterator j = m_Targets.find(targetNumber);
					if(j == m_Targets.end())
					{
						// new target added, insert placeholder
						m_Targets[targetNumber] = sParts();
						m_Status.SetValue(EosSyncStatus::SYNC_STATUS_RUNNING);
					}
					else
					{
						// existing target changed, remove exising and leave placeholder
						sParts &parts = j->second;
						for(PARTS::iterator k=parts.list.begin(); k!=parts.list.end(); k++)
							DeleteTarget( k->second );
						parts.list.clear();
						parts.initialized = false;
						m_Status.SetValue(EosSyncStatus::SYNC_STATUS_RUNNING);
					}
				}
			}
		}
		else
		{
			// no arguments, so assume entire list is dirty
			Clear();
		}
	}
	else
	{
		// notified during initial sync, must start over
		std::string text("Notified during initial sync \"");
		text.append( command.path );
		text.append("\", restarting...");
		log.AddInfo(text);
		Clear();
	}
}

////////////////////////////////////////////////////////////////////////////////

void EosTargetList::DeleteTarget(EosTarget *target)
{
	for(UID_LOOKUP::iterator i=m_UIDLookup.begin(); i!=m_UIDLookup.end(); )
	{
		if(i->second == target)
			m_UIDLookup.erase(i++);
		else
			i++;
	}

	delete target;
	m_NumTargets--;
}

////////////////////////////////////////////////////////////////////////////////

void EosTargetList::ClearDirty()
{
	if( m_Status.GetDirty() )
	{
		for(TARGETS::const_iterator i=m_Targets.begin(); i!=m_Targets.end(); i++)
		{
			const PARTS &parts = i->second.list;
			for(PARTS::const_iterator j=parts.begin(); j!=parts.end(); j++)
				j->second->ClearDirty();
		}
		m_Status.ClearDirty();
	}
}

////////////////////////////////////////////////////////////////////////////////

void EosTargetList::InitializeAsDummy()
{
	Clear();
	m_StatusInternal.SetValue(EosSyncStatus::SYNC_STATUS_COMPLETE);
	m_Status.SetValue(EosSyncStatus::SYNC_STATUS_COMPLETE);
	m_InitialSync.complete = true;
}

////////////////////////////////////////////////////////////////////////////////

EosSyncData::EosSyncData()
{
}

////////////////////////////////////////////////////////////////////////////////

EosSyncData::~EosSyncData()
{
	Clear();
}

////////////////////////////////////////////////////////////////////////////////

void EosSyncData::Clear()
{
	for(SHOW_DATA::const_iterator i=m_ShowData.begin(); i!=m_ShowData.end(); i++)
	{
		const TARGETLIST_DATA &targetListData = i->second;
		for(TARGETLIST_DATA::const_iterator j=targetListData.begin(); j!=targetListData.end(); j++)
			delete j->second;
	}
	m_ShowData.clear();
	m_Status.SetValue(EosSyncStatus::SYNC_STATUS_UNINTIALIZED);
}

////////////////////////////////////////////////////////////////////////////////

void EosSyncData::Initialize()
{
	Clear();

	// add default targets	
	for(unsigned int i=0; i<EosTarget::EOS_TARGET_COUNT; i++)
	{
		EosTarget::EnumEosTargetType type = static_cast<EosTarget::EnumEosTargetType>(i);
		if(type != EosTarget::EOS_TARGET_CUE)
			m_ShowData[type][0] = new EosTargetList(type, /*listId*/0);
	}

	m_Status.SetValue(EosSyncStatus::SYNC_STATUS_RUNNING);
}

////////////////////////////////////////////////////////////////////////////////

const EosTargetList* EosSyncData::GetTargetList(EosTarget::EnumEosTargetType type, int listId) const
{
	SHOW_DATA::const_iterator i = m_ShowData.find(type);
	if(i != m_ShowData.end())
	{
		const TARGETLIST_DATA &lists = i->second;
		TARGETLIST_DATA::const_iterator j = lists.find(listId);
		if(j != lists.end())
			return j->second;
	}

	return 0;
}

////////////////////////////////////////////////////////////////////////////////

void EosSyncData::TickRunning(EosTcp &tcp, EosOsc &osc, EosLog &log)
{
	bool allShowDataComplete = true;

	for(SHOW_DATA::const_iterator i=m_ShowData.begin(); i!=m_ShowData.end(); i++)
	{
		const TARGETLIST_DATA &targetListData = i->second;
		for(TARGETLIST_DATA::const_iterator j=targetListData.begin(); j!=targetListData.end(); j++)
		{
			EosTargetList *t = j->second;
			if(t->GetStatus().GetValue() != EosSyncStatus::SYNC_STATUS_COMPLETE)
			{
				bool wasInitialSyncComplete = t->GetInitialSync().complete;

				t->Tick(tcp, osc);

				m_Status.UpdateFromChild( t->GetStatus() );

				if(!wasInitialSyncComplete && t->GetInitialSync().complete)
					OnTargeListInitialSyncComplete(*t);

				if(t->GetStatus().GetValue() != EosSyncStatus::SYNC_STATUS_COMPLETE)
					allShowDataComplete = false;
			}
		}
	}

	if( allShowDataComplete )
	{
		m_Status.SetValue(EosSyncStatus::SYNC_STATUS_COMPLETE);
		log.AddInfo("synchronization complete");
	}
}

////////////////////////////////////////////////////////////////////////////////

void EosSyncData::OnTargeListInitialSyncComplete(EosTargetList &targetList)
{
	if(targetList.GetType() == EosTarget::EOS_TARGET_CUELIST)
	{
		// add all cues in the cuelist
		const EosTargetList::TARGETS &targets = targetList.GetTargets();
		if( !targets.empty() )
		{
			for(EosTargetList::TARGETS::const_iterator i=targets.begin(); i!=targets.end(); i++)
			{
				int cueListId = i->first.whole;
				SHOW_DATA::iterator j = m_ShowData.find(EosTarget::EOS_TARGET_CUE);
				if(j == m_ShowData.end())
				{
					m_ShowData[EosTarget::EOS_TARGET_CUE][cueListId] = new EosTargetList(EosTarget::EOS_TARGET_CUE, cueListId);
				}
				else
				{
					TARGETLIST_DATA &targetListData = j->second;
					TARGETLIST_DATA::iterator k = targetListData.find(cueListId);
					if(k == targetListData.end())
					{
						targetListData[cueListId] = new EosTargetList(EosTarget::EOS_TARGET_CUE, cueListId);
					}
					else
					{
						delete k->second;
						k->second = new EosTargetList(EosTarget::EOS_TARGET_CUE, cueListId);
					}
				}
			}
		}
		else
		{
			// no cues, so add dummy list
			int cueListId = 0;
			SHOW_DATA::iterator j = m_ShowData.find(EosTarget::EOS_TARGET_CUE);
			if(j == m_ShowData.end())
			{
				EosTargetList *dummyCueList = new EosTargetList(EosTarget::EOS_TARGET_CUE, cueListId);
				dummyCueList->InitializeAsDummy();
				m_ShowData[EosTarget::EOS_TARGET_CUE][cueListId] = dummyCueList;
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void EosSyncData::Recv(EosTcp &tcp, EosOsc &osc, EosLog &log)
{
	EosOsc::CMD_Q cmdQ;
	osc.Recv(tcp, TCP_RECV_TIMEOUT, cmdQ);

	while( !cmdQ.empty() )
	{
		RecvCmd(tcp, osc, log, *cmdQ.front());
		delete cmdQ.front();
		cmdQ.pop();
	}
}

////////////////////////////////////////////////////////////////////////////////

void EosSyncData::RecvCmd(EosTcp &tcp, EosOsc &osc, EosLog &log, EosOsc::sCommand &cmd)
{
	// is this a /get reply?
	static const std::string sGetReply("/eos/out/get/");
	if(cmd.path.find(sGetReply) == 0)
	{
		if(m_Status.GetValue() == EosSyncStatus::SYNC_STATUS_RUNNING)
		{
			// route to proper target
			bool found = false;
			for(SHOW_DATA::iterator i=m_ShowData.begin(); i!=m_ShowData.end(); i++)
			{
				TARGETLIST_DATA &targetData = i->second;
				EosTarget::EnumEosTargetType type = i->first;
				std::string replyPath(sGetReply);
				replyPath.append( EosTarget::GetNameForTargetType(type) );
				if(cmd.path.find(replyPath) == 0)
				{
					// found the matching target

					// get listId when applicable
					int listId = 0;
					if(type == EosTarget::EOS_TARGET_CUE)
					{
						if(cmd.path.size() > replyPath.size())
						{
							std::string listStr(cmd.path, replyPath.size()+1);
							const char *s = listStr.c_str();
							if( s )
								listId = atoi(s);
						}

						if(listId < 1)
							break;	// invalid list id
					}

					TARGETLIST_DATA::iterator j = targetData.find(listId);
					if(j != targetData.end())
					{
						EosTargetList *targetList = j->second;
						targetList->Recv(tcp, osc, log, cmd);
						m_Status.UpdateFromChild( targetList->GetStatus() );
						if(type == EosTarget::EOS_TARGET_CUELIST)
							RemoveOrphanedCues();
						found = true;
					}

					// done
					break;
				}
			}

			if( !found )
			{
				std::string text("ignored unrecognized reply target \"");
				text.append( cmd.path );
				text.append("\"");
				log.AddWarning(text);
			}
		}
		else
		{
			std::string text("ignored unsolicited reply \"");
			text.append( cmd.path );
			text.append("\"");
			log.AddInfo(text);
		}
	}
	else if(m_Status.GetValue() != EosSyncStatus::SYNC_STATUS_UNINTIALIZED)
	{
		// is it a notification about show data changes?
		static const std::string sNotify("/eos/out/notify/");
		if(cmd.path.find(sNotify) == 0)
		{
			// route to proper target
			bool found = false;
			for(SHOW_DATA::iterator i=m_ShowData.begin(); i!=m_ShowData.end(); i++)
			{
				TARGETLIST_DATA &targetData = i->second;
				EosTarget::EnumEosTargetType type = i->first;
				std::string notifyPath(sNotify);
				notifyPath.append( EosTarget::GetNameForTargetType(type) );
				if(cmd.path.find(notifyPath) == 0)
				{
					// found the matching target

					// get listId when applicable
					int listId = 0;
					if(type == EosTarget::EOS_TARGET_CUE)
					{
						if(cmd.path.size() > notifyPath.size())
						{
							std::string listStr(cmd.path, notifyPath.size()+1);
							const char *s = listStr.c_str();
							if( s )
								listId = atoi(s);
						}

						if(listId < 1)
							break;	// invalid list id
					}

					TARGETLIST_DATA::iterator j = targetData.find(listId);
					EosTargetList *targetList = ((j==targetData.end()) ? 0 : j->second);

					// target list does not exist
					if(!targetList && type==EosTarget::EOS_TARGET_CUE)
					{
						// new cue list created, add placeholder cue
						targetList = new EosTargetList(EosTarget::EOS_TARGET_CUE, listId);
						targetList->InitializeAsDummy();
						targetData[listId] = targetList;
					}

					if( targetList )
					{
						targetList->Notify(log, cmd);
						m_Status.UpdateFromChild( targetList->GetStatus() );
						found = true;
					}

					// done
					break;
				}
			}

			if( !found )
			{
				std::string text("ignored unrecognized notify target \"");
				text.append( cmd.path );
				text.append("\"");
				log.AddWarning(text);
			}
		}
		else
		{
			static const std::string sShowLoaded("/eos/out/event/show/loaded");
			if(cmd.path.find(sShowLoaded) == 0)
			{
				log.AddInfo("reset sync data, new show loaded");
				Clear();
			}
			else
			{
				static const std::string sShowCleared("/eos/out/event/show/cleared");
				if(cmd.path.find(sShowCleared) == 0)
				{
					log.AddInfo("reset sync data, show cleared");
					Clear();
				}
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void EosSyncData::RemoveOrphanedCues()
{
	SHOW_DATA::const_iterator showDataConstIter = m_ShowData.find(EosTarget::EOS_TARGET_CUELIST);
	const TARGETLIST_DATA *cueListList = ((showDataConstIter==m_ShowData.end()) ? 0 : (&showDataConstIter->second));
	if( cueListList )
	{
		TARGETLIST_DATA::const_iterator targetListConstIter = cueListList->find(0);
		const EosTargetList *cueList = ((targetListConstIter==cueListList->end()) ? 0 : targetListConstIter->second);
		if( cueList )
		{
			const EosTargetList::TARGETS &cueListTargets = cueList->GetTargets();

			SHOW_DATA::iterator showDataIter = m_ShowData.find(EosTarget::EOS_TARGET_CUE);
			TARGETLIST_DATA *cues = ((showDataIter==m_ShowData.end()) ? 0 : (&showDataIter->second));
			if( cues )
			{
				for(TARGETLIST_DATA::iterator i=cues->begin(); i!=cues->end(); )
				{
					int listId = i->first;
					if(listId != 0)
					{
						if(cueListTargets.find(listId) == cueListTargets.end())
						{
							delete i->second;
							TARGETLIST_DATA::iterator eraseMe = i++;
							cues->erase(eraseMe);
						}
						else
							i++;
					}
					else
						i++;
				}
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void EosSyncData::Tick(EosTcp &tcp, EosOsc &osc, EosLog &log)
{
	switch( m_Status.GetValue() )
	{
		case EosSyncStatus::SYNC_STATUS_UNINTIALIZED:
			Initialize();
			break;

		case EosSyncStatus::SYNC_STATUS_RUNNING:
			TickRunning(tcp, osc, log);
			break;
	}

	Recv(tcp, osc, log);
}

////////////////////////////////////////////////////////////////////////////////

void EosSyncData::ClearDirty()
{
	if( m_Status.GetDirty() )
	{
		for(SHOW_DATA::const_iterator i=m_ShowData.begin(); i!=m_ShowData.end(); i++)
		{
			const TARGETLIST_DATA &targetData = i->second;
			for(TARGETLIST_DATA::const_iterator j=targetData.begin(); j!=targetData.end(); j++)
				j->second->ClearDirty();
		}

		m_Status.ClearDirty();
	}
}

////////////////////////////////////////////////////////////////////////////////

EosSyncLib::EosSyncLib()
{
	m_Tcp = EosTcp::Create();
	m_Osc = new EosOsc(m_Log);
}

////////////////////////////////////////////////////////////////////////////////

EosSyncLib::~EosSyncLib()
{
	Shutdown();
	delete m_Osc;
	delete m_Tcp;
}

////////////////////////////////////////////////////////////////////////////////

bool EosSyncLib::Initialize(const char *ip, unsigned short port)
{
	return m_Tcp->Initialize(m_Log,ip, port);
}

////////////////////////////////////////////////////////////////////////////////

void EosSyncLib::Shutdown()
{
	OSCPacketWriter subscribePacket("/eos/subscribe");
	subscribePacket.AddFalse();
	m_Osc->Send(*m_Tcp, subscribePacket, /*immediate*/true);

	m_Data.Clear();
	m_Tcp->Shutdown();
}

////////////////////////////////////////////////////////////////////////////////

bool EosSyncLib::IsRunning() const
{
	return (m_Tcp->GetConnectState() != EosTcp::CONNECT_NOT_CONNECTED);
}

////////////////////////////////////////////////////////////////////////////////

bool EosSyncLib::IsConnected() const
{
	return (m_Tcp->GetConnectState() == EosTcp::CONNECT_CONNECTED);
}

////////////////////////////////////////////////////////////////////////////////

bool EosSyncLib::IsSynchronized() const
{
	return (m_Data.GetStatus().GetValue() == EosSyncStatus::SYNC_STATUS_COMPLETE);
}

////////////////////////////////////////////////////////////////////////////////

void EosSyncLib::Tick()
{
	bool wasConnected = IsConnected();

	m_Tcp->Tick(m_Log);

	if( IsConnected() )
	{
		if( !wasConnected )
		{
			OSCPacketWriter subscribePacket("/eos/subscribe");
			subscribePacket.AddTrue();
			m_Osc->Send(*m_Tcp, subscribePacket, /*immediate*/false);
		}

		m_Data.Tick(*m_Tcp, *m_Osc, m_Log);
		m_Osc->Tick(*m_Tcp);
	}
}

////////////////////////////////////////////////////////////////////////////////

bool EosSyncLib::Send(OSCPacketWriter &packet, bool immediate)
{
	return (IsConnected() && m_Osc->Send(*m_Tcp,packet,immediate));
}

////////////////////////////////////////////////////////////////////////////////

const EosTargetList& EosSyncLib::GetPatch() const
{
	const EosTargetList *list = m_Data.GetTargetList(EosTarget::EOS_TARGET_PATCH, /*listId*/0);
	return (list ? (*list) : EosTargetList::sm_InvalidTargetList);
}

////////////////////////////////////////////////////////////////////////////////

const EosTargetList& EosSyncLib::GetCueList() const
{
	const EosTargetList *list = m_Data.GetTargetList(EosTarget::EOS_TARGET_CUELIST, /*listId*/0);
	return (list ? (*list) : EosTargetList::sm_InvalidTargetList);
}

////////////////////////////////////////////////////////////////////////////////

const EosTargetList& EosSyncLib::GetCue(int listId) const
{
	const EosTargetList *list = m_Data.GetTargetList(EosTarget::EOS_TARGET_CUE, listId);
	return (list ? (*list) : EosTargetList::sm_InvalidTargetList);
}

////////////////////////////////////////////////////////////////////////////////

const EosTargetList& EosSyncLib::GetGroups() const
{
	const EosTargetList *list = m_Data.GetTargetList(EosTarget::EOS_TARGET_GROUP, /*listId*/0);
	return (list ? (*list) : EosTargetList::sm_InvalidTargetList);
}

////////////////////////////////////////////////////////////////////////////////

const EosTargetList& EosSyncLib::GetMacros() const
{
	const EosTargetList *list = m_Data.GetTargetList(EosTarget::EOS_TARGET_MACRO, /*listId*/0);
	return (list ? (*list) : EosTargetList::sm_InvalidTargetList);
}

////////////////////////////////////////////////////////////////////////////////

const EosTargetList& EosSyncLib::GetSubs() const
{
	const EosTargetList *list = m_Data.GetTargetList(EosTarget::EOS_TARGET_SUB, /*listId*/0);
	return (list ? (*list) : EosTargetList::sm_InvalidTargetList);
}

////////////////////////////////////////////////////////////////////////////////

const EosTargetList& EosSyncLib::GetPresets() const
{
	const EosTargetList *list = m_Data.GetTargetList(EosTarget::EOS_TARGET_PRESET, /*listId*/0);
	return (list ? (*list) : EosTargetList::sm_InvalidTargetList);
}

////////////////////////////////////////////////////////////////////////////////

const EosTargetList& EosSyncLib::GetIntensityPalettes() const
{
	const EosTargetList *list = m_Data.GetTargetList(EosTarget::EOS_TARGET_IP, /*listId*/0);
	return (list ? (*list) : EosTargetList::sm_InvalidTargetList);
}

////////////////////////////////////////////////////////////////////////////////

const EosTargetList& EosSyncLib::GetFocusPalettes() const
{
	const EosTargetList *list = m_Data.GetTargetList(EosTarget::EOS_TARGET_FP, /*listId*/0);
	return (list ? (*list) : EosTargetList::sm_InvalidTargetList);
}

////////////////////////////////////////////////////////////////////////////////

const EosTargetList& EosSyncLib::GetColorPalettes() const
{
	const EosTargetList *list = m_Data.GetTargetList(EosTarget::EOS_TARGET_CP, /*listId*/0);
	return (list ? (*list) : EosTargetList::sm_InvalidTargetList);
}

////////////////////////////////////////////////////////////////////////////////

const EosTargetList& EosSyncLib::GetBeamPalettes() const
{
	const EosTargetList *list = m_Data.GetTargetList(EosTarget::EOS_TARGET_BP, /*listId*/0);
	return (list ? (*list) : EosTargetList::sm_InvalidTargetList);
}

////////////////////////////////////////////////////////////////////////////////

const EosTargetList& EosSyncLib::GetCurves() const
{
	const EosTargetList *list = m_Data.GetTargetList(EosTarget::EOS_TARGET_CURVE, /*listId*/0);
	return (list ? (*list) : EosTargetList::sm_InvalidTargetList);
}

////////////////////////////////////////////////////////////////////////////////

const EosTargetList& EosSyncLib::GetEffects() const
{
	const EosTargetList *list = m_Data.GetTargetList(EosTarget::EOS_TARGET_FX, /*listId*/0);
	return (list ? (*list) : EosTargetList::sm_InvalidTargetList);
}

////////////////////////////////////////////////////////////////////////////////

const EosTargetList& EosSyncLib::GetSnapshots() const
{
	const EosTargetList *list = m_Data.GetTargetList(EosTarget::EOS_TARGET_SNAP, /*listId*/0);
	return (list ? (*list) : EosTargetList::sm_InvalidTargetList);
}

////////////////////////////////////////////////////////////////////////////////

const EosTargetList& EosSyncLib::GetPixelMaps() const
{
	const EosTargetList *list = m_Data.GetTargetList(EosTarget::EOS_TARGET_PIXMAP, /*listId*/0);
	return (list ? (*list) : EosTargetList::sm_InvalidTargetList);
}

////////////////////////////////////////////////////////////////////////////////

const EosTargetList& EosSyncLib::GetMagicSheets() const
{
	const EosTargetList *list = m_Data.GetTargetList(EosTarget::EOS_TARGET_MS, /*listId*/0);
	return (list ? (*list) : EosTargetList::sm_InvalidTargetList);
}

////////////////////////////////////////////////////////////////////////////////
