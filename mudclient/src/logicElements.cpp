#include "stdafx.h"
#include "accessors.h"
#include "logicElements.h"

CompareData::CompareData(MudViewString *s) : string(s), start(0)
{
    reinit();
}

void CompareData::reinit()
{
    fullstr.clear();
    std::vector<MudViewStringBlock> &vb = string->blocks;
    for (int i=start,e=vb.size(); i<e; ++i)
       fullstr.append(vb[i].string);
}

void CompareData::fullinit()
{
    start = 0;
    reinit();
}

void CompareData::copy(int pos, MudViewStringBlock &b)
{
    std::vector<MudViewStringBlock> &vb = string->blocks;
    int size = vb.size();
    if (pos >=0 && pos < size)
    {
         vb[pos] = b;
         return;
    }
    assert(false);
}

void CompareData::insert(int pos, int count)
{
    std::vector<MudViewStringBlock> &vb = string->blocks;
    int size = vb.size();
    if (pos >=0 && pos < size && count > 0)
    {
        vb.insert(vb.begin()+pos, count);
        return;
    }
    assert(false);
}

void CompareData::del(int pos, int count)
{
    std::vector<MudViewStringBlock> &vb = string->blocks;
    int size = vb.size();
    if (pos >=0 && pos < size && count > 0 && (pos+count) <= size)
    {
        vb.erase(vb.begin()+pos, vb.begin()+pos+count);
        return;
    }
    assert(false);
}

void CompareData::del(CompareRange& range)
{
    if (!cut(range))
        return;
    std::vector<MudViewStringBlock> &vb = string->blocks;
    vb.erase(vb.begin()+range.begin, vb.begin()+range.end+1);
}

int CompareData::fold(CompareRange& range)
{
    if (!cut(range))
        return -1;
    if (range.begin != range.end)
    {
        std::vector<MudViewStringBlock> &vb = string->blocks;
        MudViewStringBlock &b = vb[range.begin];
        for (int i=range.begin+1; i<=range.end; ++i)
            b.string.append(vb[i].string);
        vb.erase(vb.begin()+range.begin+1, vb.begin()+range.end+1);
    }
    return range.begin;
}

bool CompareData::cut(CompareRange& range)
{
    int size = fullstr.length();
    if (range.begin >=0 && range.begin < size &&
        range.end > 0 && range.end <= size)
    {
        range.begin = cutpos(range.begin, 0);
        range.end = cutpos(range.end, 1);
        return true;
    }
    assert(false);
    return false;
}

bool CompareData::find(CompareRange& range)
{
    int size = fullstr.length();
    if (range.begin >=0 && range.begin < size &&
        range.end > 0 && range.end <= size)
    {
        range.begin = findpos(range.begin, 0);
        range.end = findpos(range.end, 1);
        return true;
    }
    return false;
}

int CompareData::cutpos(int pos, int d)
{
    std::vector<MudViewStringBlock> &vb = string->blocks;
    int p = 0; int bi = start; pos -= d;
    for (int i=start,e=vb.size(); i<e; ++i)
    {
        MudViewStringBlock& b = vb[i];
        int len = b.string.length();
        int last = p + len - 1;
        if (pos >= p && pos <= last)
            { bi = i; p = pos - p; break; }
        p = p + len;
    }

    if (p == 0 && d == 0)       // begin dir first symbol
        return bi;
    MudViewStringBlock& b = vb[bi];
    int last = b.string.length()-1;
    if (p == last && d == 1)    // end dir last symbol
        return bi;

    MudViewStringBlock nb;
    nb.string = b.string.substr(p+d);
    b.string = b.string.substr(0, p+d);
    nb.params = b.params;
    nb.subs_protected = b.subs_protected;
    vb.insert(vb.begin()+(bi+1), nb);
    return bi+(1-d);
}

int CompareData::findpos(int pos, int d)
{
    std::vector<MudViewStringBlock> &vb = string->blocks;
    int p = 0; int bi = start; pos -= d;
    for (int i=start,e=vb.size(); i<e; ++i)
    {
        MudViewStringBlock& b = vb[i];
        int len = b.string.length();
        int last = p + len - 1;
        if (pos >= p && pos <= last)
            { bi = i; break; }
        p = p + len;
    }
    return bi;
}

class AliasParameters : public InputParameters
{
    const InputCommand m_pCmd;
    bool m_process_not_values;
public:
    AliasParameters(const InputCommand cmd, bool process_not_values) : m_pCmd(cmd), m_process_not_values(process_not_values) {}
    void getParameters(std::vector<tstring>* params) const
    {
        if (!m_pCmd->srcparameters.empty())
            params->push_back(m_pCmd->srcparameters.substr(1));
        const std::vector<tstring>&p = m_pCmd->parameters_list;
        params->insert(params->end(), p.begin(), p.end());
    }
    void doNoValues(tstring* cmd) const 
    {
        if (m_process_not_values)
            cmd->append(m_pCmd->srcparameters);
    }
};

Alias::Alias() {}
void Alias::init(const property_value& v, const InputTemplateParameters& p)
{
    m_compare.initOnlyVars(v.key);
    InputPlainCommands plain(v.value);
    m_cmds.init(plain, p);
    m_cmds.makeTemplates();
}

bool Alias::processing(const InputCommand cmd, InputCommands *newcmds)
{
    int cmdlen = 0;
    if (cmd->system)
    {
        if (!m_compare.compare(cmd->srccmd))
            return false;
        cmdlen = cmd->srccmd.size();
    }
    else
    {
        if (!m_compare.compare(cmd->command))
            return false;
        cmdlen = cmd->command.size();
    }

    // check full alias len
    CompareRange cr;
    m_compare.getRange(&cr);
    if (cr.begin != 0 || cr.end != cmdlen)
        return false;

    AliasParameters ap(cmd, m_cmds.size() == 1);
    m_cmds.makeCommands(newcmds, &ap);

    const tstring& alias = cmd->alias.empty() ? cmd->srccmd : cmd->alias;
    for (int i=0,e=newcmds->size();i<e;++i)
        newcmds->operator[](i)->alias.assign(alias);
    return true;
}

Hotkey::Hotkey() {}
void Hotkey::init(const property_value& v, const InputTemplateParameters& p)
{
    m_key = v.key;
    InputPlainCommands plain(v.value);
    m_cmds.init(plain, p);
    m_cmds.makeTemplates();
}

bool Hotkey::processing(const tstring& key, InputCommands *newcmds)
{
    if (key != m_key)
        return false;
    m_cmds.makeCommands(newcmds, NULL);
    return true;
}

class ActionParameters : public InputParameters
{
    const CompareObject *m_pCompareObject;
public:
    ActionParameters(const CompareObject* co) : m_pCompareObject(co) { assert(m_pCompareObject); }
    void getParameters(std::vector<tstring>* params) const {
        m_pCompareObject->getParameters(params);
    }
    void doNoValues(tstring* cmd) const {
    }
};

Action::Action() {}
void Action::init(const property_value& v, const InputTemplateParameters& p)
{
    m_compare.init(v.key, true);
    InputPlainCommands plain(v.value);
    m_cmds.init(plain, p);
    m_cmds.makeTemplates();
}

bool Action::processing(CompareData& data, bool incompl_flag,InputCommands* newcmds)
{
    if (incompl_flag && m_compare.isFullstrReq())
        return false;
    if (!m_compare.compare(data.fullstr))
        return false;
    ActionParameters ap(&m_compare);
    m_cmds.makeCommands(newcmds, &ap);
    for (int i=0,e=newcmds->size(); i<e; ++i)
    {
        if (newcmds->operator[](i)->command == L"drop")
                data.string->dropped = true;
    }
    return true;
}

Sub::Sub() : m_phelper(NULL) {}
Sub::~Sub() { delete m_phelper; }

void Sub::init(const property_value& v)
{
    delete m_phelper;
    m_phelper = new ParamsHelper(v.value, ParamsHelper::EXTENDED);
    m_value = v.value;
    m_compare.init(v.key, false);
}

bool Sub::processing(CompareData& data)
{
    if (!m_compare.compare(data.fullstr))
        return false;

    CompareRange range;
    m_compare.getRange(&range);

    CompareRange check(range);
    if (!data.find(check))
        return false;

    std::vector<MudViewStringBlock> &b = data.string->blocks;
    for (int i=check.begin; i<=check.end; ++i)
    {
        if (b[i].subs_protected)
            return false;
    }

    // generate new blocks to insert
    std::vector<MudViewStringBlock> newb;
    int vars_count = m_phelper->getSize();
    if (vars_count == 0)
    {
        // no vars
        newb.resize(1);
        newb[0].string = m_value;
        newb[0].params = b[range.begin].params;
    }
    else 
    {   
        int blocks_count = vars_count - 1;
        newb.resize(blocks_count);

        
        //newb[0].string = m_value;
        //ActionParameters ap(&m_compare); //same adapter for subs
            //std::vector<CompareRange> pr;
        //m_compare.getParametersRange(&pr);

        //todo!
        newb[0].string = m_value;
    }

    // translate vars
    InputVarsAccessor va;
    for (int i=0,e=newb.size();i<e;++i)
        va.translateVars(&newb[i].string);
    
    // insert new blocks in result string
    if (!data.cut(range))
        return false;
    int cut_blocks_count = range.end - range.begin + 1;
    int blocks = newb.size();
    int db = cut_blocks_count - blocks;
    if (db > 0)
        data.del(range.begin, db);
    else if (db < 0)
        data.insert(range.begin, -db);
    for (int i=0; i<blocks; ++i) {
        data.copy(range.begin+i, newb[i]);
    }
    data.start = range.begin+blocks;
    return true;

    /*int pos = data.fold(range);
    if (pos == -1) return false;

    ActionParameters ap(&m_compare); //same adapter for subs
    InputTranslateParameters tp;
    tstring value(m_value);
    tp.doit(&ap, &value);

    InputVarsAccessor va;
    va.translateVars(&value);
    data.string->blocks[pos].string = value;
    data.start = pos+1;
    return true;*/
}

AntiSub::AntiSub(){}
void AntiSub::init(const property_value& v)
{
    m_compare.init(v.key, false);
}

bool AntiSub::processing(CompareData& data)
{
    if (!m_compare.compare(data.fullstr))
        return false;

    CompareRange range;
    m_compare.getRange(&range);
    if (!data.cut(range))
        return false;

    for (int i=range.begin; i<=range.end; ++i)
        data.string->blocks[i].subs_protected = 1;

    data.start = range.end+1;
    return true;
}

Gag::Gag(){}
void Gag::init(const property_value& v)
{
    m_compare.init(v.key, false);
}

bool Gag::processing(CompareData& data)
{
    if (!m_compare.compare(data.fullstr))
        return false;

    CompareRange range;
    m_compare.getRange(&range);

    CompareRange check(range);
    if (!data.find(check))
        return false;

    std::vector<MudViewStringBlock> &b = data.string->blocks;
    for (int i=check.begin; i<=check.end; ++i)
    {
        if (b[i].subs_protected)
            return false;
    }

    int len = data.fullstr.length();
    if (range.begin == 0 && range.end == len)
        data.string->dropped = true;
    else
        data.del(range);
    data.start = range.end+1;
    return true;
}

Highlight::Highlight(){}
void Highlight::init(const property_value& v)
{
    m_compare.init(v.key, false);
    m_hl.convertFromString(v.value);
}

bool Highlight::processing(CompareData& data)
{
    if (!m_compare.compare(data.fullstr))
        return false;

    CompareRange range;
    m_compare.getRange(&range);
    int pos = data.fold(range);
    if (pos == -1) return false;
    MudViewStringParams &p = data.string->blocks[pos].params;
    p.use_ext_colors = 1;
    p.ext_text_color = m_hl.textcolor;
    p.ext_bkg_color = m_hl.bkgcolor;
    p.underline_status = m_hl.underlined;
    p.blink_status = m_hl.border;
    p.italic_status = m_hl.italic;
    data.start = pos + 1;
    return true;
}

Timer::Timer() : timer(0), period(0)
{
}

void Timer::init(const property_value& v, const InputTemplateParameters& p)
{
    id.assign(v.key);
    PropertiesTimer pt;
    pt.convertFromString(v.value);

    InputPlainCommands plain(pt.cmd);
    m_cmds.init(plain, p);
    m_cmds.makeTemplates();

    double t = 0;
    w2double(pt.timer, &t);
    if (t < 0)
        t = 0;
    timer = 0;
    t = t * 1000;
    period = static_cast<int>(t);
}

void Timer::makeCommands(InputCommands *cmds)
{
    m_cmds.makeCommands(cmds, NULL);
}

bool Timer::tick(int dt)
{
    if (period == 0)
        return false;

    timer += dt;
    if (timer < period)
        return false;

    timer -= period;
    return true;
}

void Timer::reset()
{
    timer = 0;
}

int Timer::left()
{
    int dt = period - timer;
    if (dt < 0) dt = 0;
    return dt;
}
