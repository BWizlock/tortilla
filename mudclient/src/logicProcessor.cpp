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
            m_pHost->preprocessGameCmd(&cmd);
            processSystemCommand(cmd);
        }
        else
        {
            // it is game command
            m_pHost->preprocessGameCmd(&cmd);
            WCHAR br[2] = { 10, 0 };
            cmd.append(br);
            processIncoming(cmd.c_str(), cmd.length(), SKIP_ACTIONS|SKIP_SUBS|SKIP_HIGHLIGHTS|GAME_CMD);
            sendToNetwork(cmd);
        }
    }
}

void LogicProcessor::processStackTick()
{
   if (m_incoming_stack.empty())
        return;
   //if (m_parser.isLastFinished())
   //     return;
   WCHAR tmp[2] = { 10, 0 };
   //processIncoming(tmp, 1, 0, 0);
   //processIncoming(tmp, 1, SKIP_ACTIONS | SKIP_SUBS | SKIP_HIGHLIGHTS, 0);
}

void LogicProcessor::processIncoming(const WCHAR* text, int text_len, int flags, int window)
{
   // parse incoming text data
   if (flags & (START_BR | GAME_CMD) && !(flags & FROM_STACK))
   {
       if (window == 0 && !m_pHost->isLastStringPrompt(0))
       {
           // � ����, ���� ������ ����� �������� ������� � ���� (��� prompt, �������� ��� ������ ������).
           stack_el e;
           e.text.assign(text, text_len);
           e.flags = flags | FROM_STACK;
           m_incoming_stack.push_back(e);
           return;
       }
   }
   // ���� ��������:
   // 1. ������, ��� ����������� ������ ������ - ��
   // 2. �������, �� ����� prompt - ��
   // 3. �������, �� �� ����� �� ������� - ������� ������� 

   parseData parse_data;
   m_parser.parse(text, text_len, &parse_data);
 
   if (flags & FROM_STACK)  // todo
   {
       parseDataStrings& ps = parse_data.strings;
       MARKINVERSED(ps);
   }
   
   if (flags & GAME_CMD)
   {
       parseDataStrings& ps = parse_data.strings;
       for (int i = 0, e = ps.size(); i < e; ++i)
           ps[i]->gamecmd = true;
   }

   // start from new string forcibly //todo - �������� ������, �.�. � ����� ������ ��� ������� �� �1.-3
   if (flags & START_BR)
       parse_data.update_prev_string = false;

   // accumulate last string in one
   m_pHost->accLastString(window, &parse_data);

   // collect strings in parse_data in one with same colors params
   ColorsCollector pc;
   pc.process(&parse_data);

   // ������� ������� ����� �� ���� ������, ���� ��� ������� ������
   if (window == 0 && !(flags & (START_BR | GAME_CMD)))
   {
       if (processStack(parse_data, flags))
           return;
   }  
   printIncoming(parse_data, flags, window);
}

void LogicProcessor::printIncoming(parseData& parse_data, int flags, int window)
{
    // final step for data
    // preprocess data via plugins
    if (!(flags & SKIP_PLUGINS))
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
    if (!(flags & SKIP_PLUGINS))
        m_pHost->postprocessText(window, &parse_data);

    int log = m_wlogs[window];
    if (log != -1)
        m_logs.writeLog(log, parse_data);     // write log
    m_pHost->addText(window, &parse_data);    // send processed text to view

    for (int i = 0, e = new_cmds.size(); i < e; ++i) // process actions' result
        processCommand(new_cmds[i]);
}

bool LogicProcessor::processStack(parseData& parse_data, int flags)
{
    // find prompts in parse data (place to insert stack -> last gamecmd/prompt string)
    int last_game_cmd = -1;
    bool use_template = propData->recognize_prompt ? true : false;
    for (int i = 0, e = parse_data.strings.size(); i < e; ++i)
    {
        MudViewString *s = parse_data.strings[i];
        if (s->gamecmd || s->prompt) { last_game_cmd = i; continue; }
        if (use_template)
        {
            // recognize prompt string via template
            tstring text;  s->getText(&text);
            m_prompt_pcre.find(text);
            if (m_prompt_pcre.getSize())
            {
                s->setPrompt(m_prompt_pcre.getLast(0));
                last_game_cmd = i;
            }
        }
    }

#ifdef _DEBUG //todo
    tchar b[32];
    wsprintf(b, L"insert: %d\r\n", last_game_cmd);
    OutputDebugString(b);
#endif

    if (last_game_cmd == -1)       // skip stack insertion (no insert place)
        return false;
    
    // div current parseData for 2 parts
    parseData pd;
    pd.update_prev_string = parse_data.update_prev_string;
    pd.strings.assign(parse_data.strings.begin(), parse_data.strings.begin()+last_game_cmd);
    printIncoming(pd, flags, 0);
    // insert stack between 2 parts
    for (int i = 0, e = m_incoming_stack.size(); i < e; ++i)
    {
        stack_el &s = m_incoming_stack[i];
        processIncoming(s.text.c_str(), s.text.length(), s.flags, 0);
    }
    pd.update_prev_string = false;
    pd.strings.assign(parse_data.strings.begin() + last_game_cmd, parse_data.strings.end());
    printIncoming(pd, flags, 0);
    pd.strings.clear();    
    return true;
}

void LogicProcessor::updateProps()
{
    m_helper.updateProps();
    m_input.updateProps(propData);
    m_logs.updateProps(propData);

    if (propData->recognize_prompt)
    {
        // calc regexp from template
        tstring tmpl(propData->recognize_prompt_template);
        Pcre16 t1;
        t1.setRegExp(L"\\\\\\*");
        t1.findAllMatches(tmpl);
        std::vector<tstring> parts;
        int pos = 0;
        for (int i = 0, e = t1.getSize(); i < e;  ++i)
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
            propData->recognize_prompt = 0;
        }
    }
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

void LogicProcessor::simpleLog(const tstring& cmd)
{
    tstring log(cmd);
    log.append(L"\r\n");
    processIncoming(log.c_str(), log.length(), SKIP_ACTIONS|SKIP_SUBS|SKIP_PLUGINS|START_BR);
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
        processIncoming(log.c_str(), log.length(), SKIP_ACTIONS|SKIP_SUBS|SKIP_PLUGINS|START_BR, window);
    }
}

void LogicProcessor::updateActiveObjects(int type)
{
    m_helper.updateProps(type);
}

bool LogicProcessor::checkActiveObjectsLog(int type)
{
    MessageCmdHelper mh(propData);
    int state = mh.getState(type);
    return (!state) ? false : true;
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
