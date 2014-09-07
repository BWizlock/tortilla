// In that file - Code for processing game data
#include "stdafx.h"
#include "logicProcessor.h"

LogicProcessor::LogicProcessor(PropertiesData *data, LogicProcessorHost *host) :
propData(data), m_pHost(host), m_connected(false), m_helper(data)
{
    for (int i=0; i<OUTPUT_WINDOWS+1; ++i)
        m_wlogs[i] = -1;
}

LogicProcessor::~LogicProcessor()
{
}

void LogicProcessor::processTick()
{
    if (!m_connected || !propData->timers_on)
        return;
    std::vector<tstring> timers_cmds;
    m_helper.processTimers(&timers_cmds);
    for (int i=0,e=timers_cmds.size(); i<e; ++i)
        processCommand(timers_cmds[i]);
}

void LogicProcessor::processNetworkData(const WCHAR* text, int text_len)
{
#ifdef _DEBUG
    tstring label(text, text_len);
    label.append(L"\r\n");
    //OutputDebugString(label.c_str());
#endif
    processIncoming(text, text_len);
}

void LogicProcessor::processNetworkConnect()
{
    propData->timers_on = 0;
    m_helper.resetTimers();
    m_connected = true;
}

bool LogicProcessor::processHotkey(const tstring& hotkey)
{
    if (hotkey.empty())
        return false;

    tstring newcmd;
    if (m_helper.processHotkeys(hotkey, &newcmd))
    {
        processCommand(newcmd);
        return true;
    }
    return false;
}

void LogicProcessor::processCommand(const tstring& cmd)
{
    std::vector<tstring> loops;
    WCHAR cmd_prefix = propData->cmd_prefix;
    m_input.process(cmd, &m_helper, &loops);
    
    if (!loops.empty())
    {
        tstring msg;
        int size = loops.size();
        if (size == 1) {
            msg.append(L"������ '"); msg.append(loops[0]); msg.append(L"' ��������. ���������� ����������.");
        }
        else {
            msg.append(L"������� '");
            for (int i = 0; i < size; ++i) { if (i != 0) msg.append(L","); msg.append(loops[i]); }
            msg.append(L"' ���������. �� ���������� ����������.");
        }
        tmcLog(msg);
    }
    
    for (int i=0,e=m_input.commands.size(); i<e; ++i)
    {
        tstring cmd = m_input.commands[i]->full_command;
        if (!cmd.empty() && cmd.at(0) == cmd_prefix)
        {
            //it is system command for client (not game command)
            m_pHost->preprocessGameCmd(cmd);
            processSystemCommand(cmd);
        }
        else
        {
            // it is game command
            m_pHost->preprocessGameCmd(cmd);
            WCHAR br[2] = { 10, 0 };
            cmd.append(br);
            processIncoming(cmd.c_str(), cmd.length(), SKIP_ACTIONS|SKIP_SUBS|SKIP_HIGHLIGHTS|GAME_CMD);
            sendToNetwork(cmd);
        }
    }
}

void LogicProcessor::processIncoming(const WCHAR* text, int text_len, int flags, int window)
{
   // parse incoming text
   parseData parse_data;
   m_parser.parse(text, text_len, &parse_data);
   if (flags & GAME_CMD)
   {
       parseDataStrings &s = parse_data.strings;
       for (int i=0,e=s.size(); i<e; ++i)
           s[i]->gamecmd = true;
   }

   // start from new string forcibly
   if (flags & START_BR)
       parse_data.update_prev_string = false;

   // accamulate last string in one
   m_pHost->accLastString(window, &parse_data);

   // collect strings in parse_data in one with same colors params
   ColorsCollector pc;
   pc.process(&parse_data);
   
   // preprocess data via plugins
   m_pHost->preprocessText(window, &parse_data);

   // array for new cmds from actions
   std::vector<tstring> new_cmds;
   if (!(flags & SKIP_ACTIONS))
       m_helper.processActions(&parse_data, &new_cmds);

   if (!(flags & SKIP_SUBS))
   {
       m_helper.processAntiSubs(&parse_data);
       m_helper.processGags(&parse_data);
       m_helper.processSubs(&parse_data);
   }

   if (!(flags & SKIP_HIGHLIGHTS))
       m_helper.processHighlights(&parse_data);

   // postprocess data via plugins
   m_pHost->postprocessText(window, &parse_data);

   int log = m_wlogs[window];
   if (log != -1)
       m_logs.writeLog(log, parse_data);    // write log 
   m_pHost->addText(window, &parse_data);   // send processed text to view

   for (int i=0,e=new_cmds.size(); i<e; ++i) // process actions' result
         processCommand(new_cmds[i]);
}

void LogicProcessor::updateProps()
{
    m_helper.updateProps();
    m_input.updateProps(propData);
    m_logs.updateProps(propData);
}

void LogicProcessor::processNetworkDisconnect()
{
    tmcLog(L"���������� ���������(�����).");
    m_connected = false;
}

void LogicProcessor::processNetworkConnectError()
{
    tmcLog(L"�� ������� ������������.");
    m_connected = false;
}

void LogicProcessor::processNetworkError()
{
    tmcLog(L"������ c���. ���������� ���������.");
    m_connected = false;
}

void LogicProcessor::processNetworkMccpError()
{
    tmcLog(L"������ � ��������� ������. ���������� ���������.");
    m_connected = false;
}

void LogicProcessor::tmcLog(const tstring& cmd)
{
    tstring log(L"[tortilla] ");
    log.append(cmd);
    simpleLog(log);
}

void LogicProcessor::tmcSysLog(const tstring& cmd)
{
    if (propData->show_system_commands)
        tmcLog(cmd);
}

void LogicProcessor::simpleLog(const tstring& cmd)
{
    tstring log(cmd);
    log.append(L"\r\n");
    processIncoming(log.c_str(), log.length(), SKIP_ACTIONS|SKIP_SUBS|START_BR);
}

void LogicProcessor::pluginLog(const tstring& cmd)
{
    if (!propData->plugins_logs)
        return;
    int window = propData->plugins_logs_window;
    if (window >= 0 && window <= OUTPUT_WINDOWS)
    {
        tstring log(L"[plugins] ");
        log.append(cmd);
        processIncoming(log.c_str(), log.length(), SKIP_ACTIONS|SKIP_SUBS|START_BR, window);
    }
}

void LogicProcessor::updateActiveObjects(int type)
{
    m_helper.updateProps(type);
}

bool LogicProcessor::addSystemCommand(const tstring& cmd)
{
    PropertiesList &p = propData->tabwords_commands;
    if (p.find(cmd) != -1)
        return false;
    m_plugins_cmds.push_back(cmd);
    propData->tabwords_commands.add(-1, cmd);
    return true;
}

bool LogicProcessor::deleteSystemCommand(const tstring& cmd)
{
    std::vector<tstring>::iterator it = std::find(m_plugins_cmds.begin(), m_plugins_cmds.end(), cmd);
    if (it == m_plugins_cmds.end())
        return false;
    m_plugins_cmds.erase(it);
    PropertiesList &p = propData->tabwords_commands;
    int index = p.find(cmd);
    p.del(index);
    return true;
}

void LogicProcessor::updateLog(const tstring& msg)
{
    m_updatelog.append(msg);
}

bool LogicProcessor::sendToNetwork(const tstring& cmd)
{
    if (m_connected)
    {
        m_pHost->sendToNetwork(cmd);
        return true;
    }    
    tmcLog(L"��� �����������.");
    return false;
}