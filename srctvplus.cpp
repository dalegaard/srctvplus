// Fix INVALID_HANDLE_VALUE redefinition warning
#ifndef _LINUX
#define WIN32_LEAN_AND_MEAN
#include "windows.h"
#endif

#ifndef NULL
#define NULL nullptr
#endif
#include "FunctionRoute.h"

#include "interface.h"
#include "filesystem.h"
#include "engine/iserverplugin.h"
#include "game/server/iplayerinfo.h"
#include "eiface.h"
#include "igameevents.h"
#include "convar.h"
#include "Color.h"
#include "vstdlib/random.h"
#include "engine/IEngineTrace.h"
#include "tier2/tier2.h"
#include "ihltv.h"
#include "ihltvdirector.h"
#include "KeyValues.h"
#include "dt_send.h"
#include "server_class.h"

#include <vector>
#include <set>
#include <string>
#include <sstream>
#include <iomanip>
#include <cmath>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IServerGameDLL* g_pGameDLL;
IFileSystem* g_pFileSystem;
IHLTVDirector* g_pHLTVDirector;
IVEngineServer* engine;

class SrcTVPlus : public IServerPluginCallbacks {
public:

  virtual bool Load(CreateInterfaceFn interfaceFactory, CreateInterfaceFn gameServerFactory) override;

  // All of the following are just nops or return some static data
  virtual void Unload() override { }
  virtual void Pause() override { }
  virtual void UnPause() override { }
  virtual const char* GetPluginDescription() override { return "srctv+"; }
  virtual void LevelInit(const char* mapName) override { }
  virtual void ServerActivate(edict_t* pEdictList, int edictCount, int clientMax) override { }
  virtual void GameFrame(bool simulating) override { }
  virtual void LevelShutdown() override { }
  virtual void ClientActive(edict_t* pEntity) override { }
  virtual void ClientDisconnect(edict_t* pEntity) override { }
  virtual void ClientPutInServer(edict_t* pEntity, const char* playername) override { }
  virtual void SetCommandClient(int index) override { }
  virtual void ClientSettingsChanged(edict_t* pEdict) override { }
  virtual PLUGIN_RESULT ClientConnect(bool* bAllowConnect, edict_t* pEntity, const char* pszName, const char* pszAddress, char* reject, int maxrejectlen) override { return PLUGIN_CONTINUE; }
  virtual PLUGIN_RESULT ClientCommand(edict_t* pEntity, const CCommand& args) override { return PLUGIN_CONTINUE; }
  virtual PLUGIN_RESULT NetworkIDValidated(const char* pszUserName, const char* pszNetworkID) override { return PLUGIN_CONTINUE; }
  virtual void OnQueryCvarValueFinished(QueryCvarCookie_t iCookie, edict_t* pPlayerEntity, EQueryCvarValueStatus eStatus, const char* pCvarName, const char* pCvarValue) override { }
  virtual void OnEdictAllocated(edict_t* edict) { }
  virtual void OnEdictFreed(const edict_t* edict) { }

private:
  CFunctionRoute m_sendLocalDataTableHook;
  CFunctionRoute m_sendLocalWeaponDataTableHook;
  CFunctionRoute m_sendLocalActiveWeaponDataTableHook;
  static void* SendProxy_SendLocalDataTable(const SendProp *pProp, const void *pStructBase, const void *pData, CSendProxyRecipients *pRecipients, int objectID);
  static void* SendProxy_SendLocalWeaponDataTable(const SendProp *pProp, const void *pStructBase, const void *pData, CSendProxyRecipients *pRecipients, int objectID);
  static void* SendProxy_SendLocalActiveWeaponDataTable(const SendProp *pProp, const void *pStructBase, const void *pData, CSendProxyRecipients *pRecipients, int objectID);

  static const char** GetModEvents(IHLTVDirector *director);
  CFunctionRoute m_directorHook;
};

SrcTVPlus g_Plugin;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(SrcTVPlus, IServerPluginCallbacks, INTERFACEVERSION_ISERVERPLUGINCALLBACKS, g_Plugin);

typedef void* (*SendTableProxyFn)(
    const SendProp *pProp,
    const void *pStructBase,
    const void *pData,
    CSendProxyRecipients *pRecipients,
    int objectID);

// Calls a sendproxy and adds the HLTV pseudo client to the returned recipients list
void* SendProxy_IncludeHLTV(SendTableProxyFn fn, const SendProp* pProp, const void* pStructBase, const void* pData, CSendProxyRecipients* pRecipients, int objectID) {
  const char* ret = (const char*)fn(pProp, pStructBase, pData, pRecipients, objectID);
  if (ret) {
    if (engine->IsDedicatedServer()) {
      // Normal dedicated server
      auto server = g_pHLTVDirector->GetHLTVServer();
      if (server) {
        auto slot = server->GetHLTVSlot();
        if (slot >= 0) {
          pRecipients->m_Bits.Set(slot);
        }
      }
    } else {
      // Listen server
      pRecipients->m_Bits.Set(0);
    }
  }
  return (void*)ret;
}

void* SrcTVPlus::SendProxy_SendLocalDataTable(const SendProp *pProp, const void *pStructBase, const void *pData, CSendProxyRecipients *pRecipients, int objectID) {
  return SendProxy_IncludeHLTV(g_Plugin.m_sendLocalDataTableHook.CallOriginalFunction<SendTableProxyFn>(), pProp, pStructBase, pData, pRecipients, objectID);
}

void* SrcTVPlus::SendProxy_SendLocalWeaponDataTable(const SendProp *pProp, const void *pStructBase, const void *pData, CSendProxyRecipients *pRecipients, int objectID) {
  return SendProxy_IncludeHLTV(g_Plugin.m_sendLocalWeaponDataTableHook.CallOriginalFunction<SendTableProxyFn>(), pProp, pStructBase, pData, pRecipients, objectID);
}

void* SrcTVPlus::SendProxy_SendLocalActiveWeaponDataTable(const SendProp *pProp, const void *pStructBase, const void *pData, CSendProxyRecipients *pRecipients, int objectID) {
  return SendProxy_IncludeHLTV(g_Plugin.m_sendLocalActiveWeaponDataTableHook.CallOriginalFunction<SendTableProxyFn>(), pProp, pStructBase, pData, pRecipients, objectID);
}

// Loads events in a given resource file and inserts them into target set.
bool load_events(const char* filename, std::set<std::string>& target) {
  auto kv = new KeyValues(filename);
  KeyValues::AutoDelete autodelete_kv(kv);

  if(!kv->LoadFromFile(g_pFileSystem, filename, "GAME")) {
    Warning("Could not load events from %s\n", filename);
    return false;
  }

  KeyValues* subkey = kv->GetFirstSubKey();
  while(subkey) {
    target.insert(subkey->GetName());
    subkey = subkey->GetNextKey();
  }

  return true;
}

// Returns the normal IHLTVDirector::GetModEvents events, along with all other
// events available in the modevents, gameevents, and serverevents resource files.
typedef const char**(*IHLTVDirector_GetModEvents_t)(void *thisPtr);
const char** SrcTVPlus::GetModEvents(IHLTVDirector* director) {
  static std::set<std::string> events;
  static std::vector<const char*> list;

  if(!list.empty()) {
    return list.data();
  }

  auto orig = g_Plugin.m_directorHook.CallOriginalFunction<IHLTVDirector_GetModEvents_t>()(director);
  for(int i = 0; orig[i] != nullptr; i++) {
    events.insert(orig[i]);
  }

  load_events("resource/modevents.res", events);
  load_events("resource/gameevents.res", events);
  load_events("resource/serverevents.res", events);

  for(auto& e : events) {
    list.push_back(e.c_str());
  }
  list.push_back(nullptr);
  list.shrink_to_fit();

  return list.data();
}

// Finds a send prop within a send table
SendProp* GetSendPropInTable(SendTable* tbl, const char* propname) {
  for(int i = 0; i < tbl->GetNumProps(); i++) {
    auto prop = tbl->GetProp(i);

    if(!propname) {
      break;
    }

    if(strcmp(prop->GetName(), propname) != 0)
      continue;

    propname = strtok(nullptr, ".");
    if(propname) {
      auto child = prop->GetDataTable();
      if(!child) continue;
      auto ret = GetSendPropInTable(child, propname);
      if(ret) return ret;
    } else {
      return prop;
    }
  }

  return nullptr;
}

// Finds a send prop within a send table, given the class name
SendProp* GetSendProp(const char* tblname, const char* propname) {
  SendTable* tbl = nullptr;
  for(ServerClass* cls = g_pGameDLL->GetAllServerClasses(); cls != nullptr; cls = cls->m_pNext) {
    if(strcmp(cls->m_pNetworkName, tblname) == 0) {
      tbl = cls->m_pTable;
      break;
    }
  }
  if(!tbl) {
    return nullptr;
  }
  char name[256] = { 0 };
  strncpy(name, propname, sizeof(name)-1);
  propname = strtok(name, ".");
  return GetSendPropInTable(tbl, propname);
}

void* search_interface(CreateInterfaceFn factory, const char* name) {
  const char* end = name + strlen(name) - 1;
  int digits = 0;
  while (end > name && isdigit(*end) && digits <= 3) {
    end--;
    digits++;
  }

  if (digits > 0) {
    std::string ifname(name, strlen(name) - digits);

    int max = 1;
    for(auto i = 1; i <= digits; i++) {
      max *= 10;
    }
    for(auto i = 0; i < max; i++) {
      std::stringstream tmp;
      tmp << ifname << std::setfill('0') << std::setw(digits) << i;
      auto ret = factory(tmp.str().c_str(), nullptr);
      if (ret)
        return ret;
    }
  }

  // Fallback: try original name
  return factory(name, nullptr);
}

bool SrcTVPlus::Load(CreateInterfaceFn interfaceFactory, CreateInterfaceFn gameServerFactory) {
  Warning("[srctv+] Loading...\n");

  g_pGameDLL = (IServerGameDLL*)search_interface(gameServerFactory, INTERFACEVERSION_SERVERGAMEDLL);
  if(!g_pGameDLL) {
    Error("[srctv+] Could not find game DLL interface, aborting load\n");
    return false;
  }

  engine = (IVEngineServer*)search_interface(interfaceFactory, INTERFACEVERSION_VENGINESERVER);
  if (!engine) {
    Error("[srctv+] Could not find engine interface, aborting load\n");
    return false;
  }

  g_pFileSystem = (IFileSystem*)search_interface(interfaceFactory, FILESYSTEM_INTERFACE_VERSION);
  if(!g_pFileSystem) {
    Error("[srctv+] Could not find filesystem interface, aborting load\n");
    return false;
  }

  // HLTV director mod events hook, for sending all events to the HLTV client
  g_pHLTVDirector = (IHLTVDirector*)search_interface(gameServerFactory, INTERFACEVERSION_HLTVDIRECTOR);
  if(!g_pHLTVDirector) {
    Error("[srctv+] Could not find SrcTV director, aborting load\n");
    return false;
  }
  m_directorHook.RouteVirtualFunction(g_pHLTVDirector, &IHLTVDirector::GetModEvents, SrcTVPlus::GetModEvents);

  // Hook various SendProxies so they also send their data to the HLTV pseudo client.

  auto prop = GetSendProp("CBasePlayer", "localdata");
  if(!prop) {
    Error("[srctv+] Could not find CBasePlayer prop 'localdata'\n");
    return false;
  }
  m_sendLocalDataTableHook.RouteFunction((void*)prop->GetDataTableProxyFn(), (void*)&SrcTVPlus::SendProxy_SendLocalDataTable);

  prop = GetSendProp("CBaseCombatWeapon", "LocalWeaponData");
  if(!prop) {
    Error("[srctv+] Could not find CBaseCombatWeapon prop 'LocalWeaponData'\n");
    return false;
  }
  m_sendLocalWeaponDataTableHook.RouteFunction((void*)prop->GetDataTableProxyFn(), (void*)&SrcTVPlus::SendProxy_SendLocalWeaponDataTable);

  prop = GetSendProp("CBaseCombatWeapon", "LocalActiveWeaponData");
  if(!prop) {
    Error("[srctv+] Could not find CBaseCombatWeapon prop 'LocalActiveWeaponData'\n");
    return false;
  }
  m_sendLocalActiveWeaponDataTableHook.RouteFunction((void*)prop->GetDataTableProxyFn(), (void*)&SrcTVPlus::SendProxy_SendLocalActiveWeaponDataTable);

  Warning("[srctv+] Loaded!\n");
  return true;
}

