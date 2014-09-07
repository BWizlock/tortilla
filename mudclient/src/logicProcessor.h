#pragma once

#include "mudViewParser.h"
#include "logicHelper.h"
#include "inputProcessor.h"
#include "logsProcessor.h"
#include "IfProcessor.h"

class LogicProcessorHost
{
public:
    virtual void connectToNetwork(const tstring& address, int port) = 0;
    virtual void disconnectFromNetwork() = 0;
    virtual void sendToNetwork(const tstring& data) = 0;
    virtual void accLastString(int view, parseData* parse_data) = 0;
    virtual void preprocessText(int view, parseData* parse_data) = 0;
    virtual void postprocessText(int view, parseData* parse_data) = 0;
    virtual void addText(int view, parseData* parse_data) = 0;
    virtual void clearText(int view) = 0;
    virtual void showWindow(int view, bool show) = 0;
    virtual void setWindowName(int view, const tstring& name) = 0;
    virtual void getNetworkRatio(int *compressed, int *decompressed) = 0;
    virtual HWND getMainWindow() = 0;
    virtual void preprocessGameCmd(tstring& cmd) = 0;
};

class LogicProcessorMethods
{
public:
    virtual void tmcLog(const tstring& msg) = 0;
    virtual void tmcSysLog(const tstring& cmd) = 0;
    virtual void simpleLog(const tstring& msg) = 0;
    virtual void pluginLog(const tstring& msg) = 0;
    virtual void updateLog(const tstring& msg) = 0;
    virtual void updateActiveObjects(int type) = 0;
    virtual bool addSystemCommand(const tstring& cmd) = 0;
    virtual bool deleteSystemCommand(const tstring& cmd) = 0;
};

class parser;
#define DEF(fn) void impl_##fn(parser*);
typedef void(*syscmd_fun)(parser*);

class LogicProcessor : public LogicProcessorMethods
{
    PropertiesData *propData;
    LogicProcessorHost *m_pHost;
    MudViewParser m_parser;
    InputProcessor m_input;
    LogicHelper m_helper; 
    bool m_connected;
    tstring m_updatelog;
    LogsProcessor m_logs;
    int m_wlogs[OUTPUT_WINDOWS+1];    
    std::map<tstring, syscmd_fun> m_syscmds;
    std::vector<tstring> m_plugins_cmds;
    IfProcessor m_ifproc;

public:
    LogicProcessor(PropertiesData *data, LogicProcessorHost *host);
    ~LogicProcessor();
    bool init();    
    void processNetworkData(const WCHAR* text, int text_len);
    void processNetworkConnect();
    void processNetworkDisconnect();
    void processNetworkConnectError();
    void processNetworkError();
    void processNetworkMccpError();
    bool processHotkey(const tstring& hotkey);
    void processCommand(const tstring& cmd);
    void processSystemCommand(const tstring& cmd);
    void processTick();
    void updateProps();    
    void tmcLog(const tstring& cmd);
    void tmcSysLog(const tstring& cmd);
    void simpleLog(const tstring& cmd);
    void pluginLog(const tstring& cmd);
    void updateActiveObjects(int type);
    bool addSystemCommand(const tstring& cmd);
    bool deleteSystemCommand(const tstring& cmd);

private:    
    enum { SKIP_ACTIONS = 1, SKIP_SUBS = 2, SKIP_HIGHLIGHTS = 4, START_BR = 8, GAME_CMD = 16  };
    void processIncoming(const WCHAR* text, int text_len, int flags = 0, int window = 0 );
    void updateLog(const tstring& msg);
    void updateProps(int update, int options);
    void regCommand(const char* name, syscmd_fun f);
    bool sendToNetwork(const tstring& cmd);

public: // system commands
    DEF(drop);
    DEF(action);
    DEF(unaction);
    DEF(alias);
    DEF(unalias);
    DEF(clear);
    DEF(connect);
    DEF(cr);
    DEF(disconnect);
    DEF(sub);
    DEF(unsub);
    DEF(hotkey);
    DEF(unhotkey);
    DEF(help);
    DEF(hide);
    DEF(highlight);
    DEF(ifop);
    DEF(unhighlight);
    DEF(gag);
    DEF(ungag);
    DEF(antisub);
    DEF(unantisub);
    DEF(group);
    DEF(mccp);    
    DEF(wshow);
    DEF(whide);
    DEF(wpos);
    void printex(int view, const std::vector<tstring>& params);
    DEF(wprint);
    DEF(print);
    DEF(tab);
    DEF(untab);
    DEF(timer);
    DEF(hidewindow);
    DEF(showwindow);
    void wlogf_main(int log, const tstring& file, bool newlog);
    void logf(parser *p, bool newlog);
    DEF(logs);
    DEF(logn);
    void wlogf(parser *p, bool newlog);
    void invalidwindow(parser *p, int view0, int view);
    DEF(wlog);
    DEF(wlogn);
    DEF(wname);
    DEF(var);
    DEF(unvar);
};