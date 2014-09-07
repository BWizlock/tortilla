#pragma once

#include "compareObject.h"
#include "propertiesPages/propertiesData.h"
#include "MudViewString.h"

class CompareData
{
public:
    CompareData(MudViewString *s);
    void reinit();
    void del(CompareRange& range);
    int  fold(CompareRange& range);
    bool cut(CompareRange& range); // distinguish in individual blocks
    bool find(CompareRange& range);

    MudViewString *string;
    tstring fullstr;
    int  start;
private:    
    int  cutpos(int pos, int d);
    int  findpos(int pos, int d);
};

class Alias
{
public:
    Alias(const property_value& v);
    bool processing(const tstring& key, tstring *newcmd);
private:
    tstring m_key;
    tstring m_cmd;
};

class Hotkey
{
public:
    Hotkey(const property_value& v);
    bool processing(const tstring& key, tstring *newcmd);
private:
    tstring m_key;
    tstring m_cmd;
};

class Action
{
public:
    Action(const property_value& v);
    bool processing(CompareData& data, tstring* newcmd);

private:
    CompareObject m_compare;
    tstring m_value;
};

class Sub
{
public:
    Sub(const property_value& v);
    bool processing(CompareData& data);

private:
    CompareObject m_compare;
    tstring m_value;
};

class AntiSub
{
public:
    AntiSub(const property_value& v);
    bool processing(CompareData& data);
private:
    CompareObject m_compare;
};

class Gag
{
public:
    Gag(const property_value& v);
    bool processing(CompareData& data);

private:
    CompareObject m_compare;
};

class Highlight
{
public:
    Highlight(const property_value& v);
    bool processing(CompareData& data);

private:
    CompareObject m_compare;
    PropertiesHighlight m_hl;
};

class Timer
{
public:
    Timer();
    void init(const property_value& v);
    bool tick(int dt);
    void reset();
    tstring id;
    tstring cmd;

private:
    int timer;
    int period;    
};