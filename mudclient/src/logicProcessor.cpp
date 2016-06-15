#include "stdafx.h"
#include "accessors.h"
#include "logicProcessor.h"

LogicProcessor::LogicProcessor(LogicProcessorHost *host) :
m_pHost(host), m_connecting(false), m_connected(false),
m_prompt_mode(OFF), m_prompt_counter(0),
m_plugins_log_tocache(false), m_plugins_log_blocked(false)
{
    for (int i=0; i<OUTPUT_WINDOWS+1; ++i)
        m_wlogs[i] = -1;
}

LogicProcessor::~LogicProcessor()
{
}

void LogicProcessor::processTick()
{
    std::vector<tstring> cmds;
    m_waitcmds.tick(&cmds);
    if (!cmds.empty()) 
    {
        InputPlainCommands wait_cmds;
        for (int i=0,e=cmds.size();i<e;++i)
        {
            InputPlainCommands tmp(cmds[i]);
            wait_cmds.move(tmp);
        }
        processCommands(wait_cmds);
    }    
    if (!m_connected || !tortilla::getProperties()->timers_on)
        return;
    InputCommands timers_cmds;
    m_helper.processTimers(&timers_cmds);
    runCommands(timers_cmds);
}

void LogicProcessor::processNetworkData(const WCHAR* text, int text_len)
{
    processIncoming(text, text_len, SKIP_NONE, 0);
}

void LogicProcessor::processNetworkConnect()
{
    m_helper.resetTimers();
    m_connected = true;
    m_connecting = false;
}

bool LogicProcessor::processHotkey(const tstring& hotkey)
{
    if (hotkey.empty())
        return false;
    InputCommands newcmds;
    if (m_helper.processHotkeys(hotkey, &newcmds))
    {
        runCommands(newcmds);
        return true;
    }
    return false;
}

void LogicProcessor::processUserCommand(const InputPlainCommands& cmds)
{
    processCommands(cmds);
}

void LogicProcessor::processPluginCommand(const tstring& cmd)
{
    processCommand(cmd);
}

void LogicProcessor::processCommand(const tstring& cmd)
{
    InputPlainCommands cmds(cmd);
    processCommands(cmds);
}

void LogicProcessor::makeCommands(const InputPlainCommands& cmds, InputCommands* rcmds)
{
    PropertiesData* pdata = tortilla::getProperties();

    InputTemplateParameters p;
    p.prefix = pdata->cmd_prefix;
    p.separator = pdata->cmd_separator;

    InputTemplateCommands tcmds;
    tcmds.init(cmds, p);
    tcmds.makeTemplates();
    tcmds.makeCommands(rcmds, NULL);
}

void LogicProcessor::processCommands(const InputPlainCommands& cmds)
{
    InputCommands result;
    makeCommands(cmds, &result);
    runCommands(result);
}

void LogicProcessor::runCommands(InputCommands& cmds)
{
    if (!processAliases(cmds))
        return;
    InputCommandVarsProcessor vp;
    InputCommandsVarsFilter vf;
    int i=0,e=cmds.size();
    for (; i<e; ++i)
    {
        InputCommand cmd = cmds[i];
        if (!vf.checkFilter(cmd))
        {
          while (vp.makeCommand(cmd))
          {
             // found $var in cmd name -> run aliases again
             cmds.erase(i);
             InputCommands alias;
             alias.push_back(cmd);
             bool result = processAliases(alias);
             cmds.insert(i, alias);
             e = cmds.size();
             cmd = cmds[i];
             if (!result) return;
             if (vf.checkFilter(cmd))
                 break;
          }
        }

        // check repeat commands
        if (cmd->system && isOnlyDigits( cmd->command))
        {
            int repeats = 0;
            w2int(cmd->command, &repeats);
            std::vector<tstring>& p = cmd->parameters_list;
            if (repeats == 0)
            {
                m_commands_queue.clear();
                continue;
            }
            if (p.empty() || repeats < 0)
                continue;
            if (repeats > 100)
            {   
                tstring error(L"������: ������� ������� ���������� �������� [");
                error.append(cmd->srccmd);
                error.append(cmd->srcparameters);
                error.append(L"]");
                tmcLog(error);
                continue;
            }

            InputPlainCommands t;
            for (int j=0,je=cmd->parameters_list.size();j<je;++j)
                t.push_back(cmd->parameters_list[j]);
            InputCommands queue_cmds;
            makeCommands(t, &queue_cmds);
            queue_cmds.repeat(repeats);
            queue_cmds.push_back(cmds, i+1);
            m_commands_queue.push_back(queue_cmds);
            continue;
        }
        if (!m_commands_queue.empty())
        {
            cmds.erase(i);
            e = cmds.size();
            m_commands_queue.push_back(cmd);
            continue;
        }
        if (cmd->system)
            processSystemCommand(cmd); //it is system command for client
        else
            processGameCommand(cmd);   // it is game command
    }
}

bool LogicProcessor::processAliases(InputCommands& cmds)
{
    // to protect from loops in aliases
    bool loop = false;
    std::vector<tstring> loops;
    int queue_size = cmds.size();
    for (int i=0; i<queue_size;)
    {
        InputCommand cmd = cmds[i];
        if (cmd->command.empty()) 
            { i++; continue; }

        InputCommands newcmds;
        if (!m_helper.processAliases(cmd, &newcmds))
            { i++; continue; }

        loops.push_back( (cmd->system) ? cmd->srccmd : cmd->command);
 
        for (int j = 0, je = newcmds.size(); j < je; ++j)
        {
            InputCommand cmd2 = newcmds[j];
            const tstring& compare_cmd = (cmd2->system) ? cmd2->srccmd : cmd2->command;
            if (std::find(loops.begin(), loops.end(), compare_cmd) != loops.end())
            {
                loop = true;
                loops.push_back(compare_cmd);
                break;
            }
        }
        if (loop)
            break;
        cmds.erase(i);
        cmds.insert(i, newcmds);
        queue_size = cmds.size();
    }

    if (loop)
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
        return false;
    }
    return true;
}

void LogicProcessor::updateProps()
{
    m_helper.updateProps();
    PropertiesData *pdata = tortilla::getProperties();
    m_logs.updateProps(pdata);
    m_prompt_mode = OFF;
    if (pdata->recognize_prompt)
    {
        // calc regexp from template
        tstring tmpl(pdata->recognize_prompt_template);
        Pcre16 t1;
        t1.setRegExp(L"\\\\\\*");
        t1.findAllMatches(tmpl);
        std::vector<tstring> parts;
        int pos = 0;
        for (int i = 1,e=t1.getSize(); i<e;  ++i)
        {
            int last = t1.getFirst(i);
            parts.push_back(tmpl.substr(pos, last - pos));
            pos = t1.getLast(i);
        }
        parts.push_back(tmpl.substr(pos));
        for (int i = 0, e = parts.size(); i < e; ++i)
        {
            MaskSymbolsBySlash mask(parts[i], L"+/?|^$.[]()\\");
            parts[i] = mask;
            tstring_replace(&parts[i], L"*", L".*");
        }

        int last = parts.size() - 1;
        tmpl.clear();
        for (int i = 0; i < last; ++i)
        {
            tmpl.append(parts[i]);
            tmpl.append(L"\\*");
        }
        tmpl.append(parts[last]);

        bool result = m_prompt_pcre.setRegExp(tmpl, true);
        if (!result)
        {
            MessageBox(m_pHost->getMainWindow(), L"������ � ������� ��� ������������� Prompt-������!", L"������", MB_OK | MB_ICONERROR);
            pdata->recognize_prompt = 0;
        }
    }
}

void LogicProcessor::processNetworkDisconnect()
{
    processNetworkError(L"���������� ���������(�����).");
}

void LogicProcessor::processNetworkConnectError()
{
    processNetworkError(L"�� ������� ������������.");
}

void LogicProcessor::processNetworkError()
{
    processNetworkError(L"������ c���. ���������� ���������.");
}

void LogicProcessor::processNetworkMccpError()
{
    processNetworkError(L"������ � ��������� ������. ���������� ���������.");
}

void LogicProcessor::tmcLog(const tstring& cmd)
{
    tstring log(L"[tortilla] ");
    log.append(cmd);
    simpleLog(log);
}

void LogicProcessor::simpleLog(const tstring& cmd)
{
    tstring log(cmd);
    log.append(L"\r\n");
    processIncoming(log.c_str(), log.length(), SKIP_ACTIONS|SKIP_SUBS|GAME_LOG/*|SKIP_PLUGINS*/, 0);
}

void LogicProcessor::syscmdLog(const tstring& cmd)
{
    PropertiesData *pdata = tortilla::getProperties();
    if (!pdata->show_system_commands)
        return;
    tstring log(cmd);
    log.append(L"\r\n");
    processIncoming(log.c_str(), log.length(), SKIP_ACTIONS|SKIP_SUBS|SKIP_HIGHLIGHTS|GAME_LOG|GAME_CMD, 0);
}

void LogicProcessor::pluginLog(const tstring& cmd)
{
    PropertiesData *pdata = tortilla::getProperties();
    if (!pdata->plugins_logs)
        return;
    int window = pdata->plugins_logs_window;
    if (window >= 0 && window <= OUTPUT_WINDOWS)
    {
        tstring log(L"[plugin] ");
        log.append(cmd);
        log.append(L"\r\n");
        if (m_plugins_log_blocked)
            m_plugins_log_toblocked.push_back(log);
        else if (m_plugins_log_tocache)
            m_plugins_log_cache.push_back(log);
        else
            processIncoming(log.c_str(), log.length(), SKIP_ACTIONS|SKIP_SUBS|GAME_LOG/*|SKIP_PLUGINS*/, window);
    }
}

void LogicProcessor::updateActiveObjects(int type)
{
    m_helper.updateProps(type);
}

bool LogicProcessor::checkActiveObjectsLog(int type)
{
    PropertiesData *pdata = tortilla::getProperties();
    MessageCmdHelper mh(pdata);
    int state = mh.getState(type);
    return (!state) ? false : true;
}

bool LogicProcessor::addSystemCommand(const tstring& cmd)
{
    PropertiesData *pdata = tortilla::getProperties();
    PropertiesList &p = pdata->tabwords_commands;
    if (p.find(cmd) != -1)
        return false;
    m_plugins_cmds.push_back(cmd);
    pdata->tabwords_commands.add(-1, cmd);
    return true;
}

bool LogicProcessor::deleteSystemCommand(const tstring& cmd)
{
    PropertiesData *pdata = tortilla::getProperties();
    std::vector<tstring>::iterator it = std::find(m_plugins_cmds.begin(), m_plugins_cmds.end(), cmd);
    if (it == m_plugins_cmds.end())
        return false;
    m_plugins_cmds.erase(it);
    PropertiesList &p = pdata->tabwords_commands;
    int index = p.find(cmd);
    p.del(index);
    return true;
}

void LogicProcessor::windowOutput(int window, const std::vector<tstring>& msgs)
{
    if (window >= 0 && window <= OUTPUT_WINDOWS)
    {
       if (m_plugins_log_blocked || m_plugins_log_tocache)
       {
           pluginLog(L"print � before, after ������������ ������. ��. �������.");
       }
       else
       {
           printex(window, msgs);
       }
    }
}

void LogicProcessor::pluginsOutput(int window, const MudViewStringBlocks& v)
{
    if (window >= 0 && window <= OUTPUT_WINDOWS) {
    parseData data;
    MudViewString *new_string = new MudViewString();
    int count = v.size();
    new_string->blocks.resize(count);
    for (int i=0;i<count;++i)
       new_string->blocks[i] = v[i];
    new_string->system = true;
    data.strings.push_back(new_string);
    printIncoming(data, SKIP_SUBS|SKIP_ACTIONS|GAME_LOG, window);
    }
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

void LogicProcessor::processNetworkError(const tstring& error)
{
    tortilla::getProperties()->timers_on = 0;
    m_prompt_mode = OFF;
    m_prompt_counter = 0;
    if (m_connected || m_connecting)
        tmcLog(error.c_str());
    m_connected = false;
    m_connecting = false;
}
