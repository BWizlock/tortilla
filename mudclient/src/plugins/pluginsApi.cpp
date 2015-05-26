#include "stdafx.h"
#include "pluginsApi.h"
#include "api/api.h"
#include "../MainFrm.h"
#include "pluginSupport.h"
#include "../profiles/profilesPath.h"

#define CAN_DO if (!_wndMain.IsWindow()) return 0;
extern CMainFrame _wndMain;
extern Plugin* _cp;
ToolbarEx<CMainFrame>* _tbar;
LogicProcessorMethods* _lp;
PropertiesData* _pdata;
CFont* _stdfont;
Palette256* _palette;
PropertiesManager* _pmanager;
PluginsManager* _plugins_manager;
PluginsIdTableControl m_idcontrol(PLUGING_MENUID_START, PLUGING_MENUID_END);
luaT_State L;
//--------------------------------------------------------------------
tstring _extra_plugin_name;
class CurrentPluginExtra
{  bool m_used;
public:
    CurrentPluginExtra() : m_used(false) {
       if (_cp || _extra_plugin_name.empty()) return;
       Plugin *p = _plugins_manager->findPlugin(_extra_plugin_name);
       if (p) { _cp = p; m_used = true; }
    }
    ~CurrentPluginExtra() {
        if (m_used) { _cp = NULL; _extra_plugin_name.clear(); }
    }
};
#define EXTRA_CP CurrentPluginExtra _cpgetextra;
//--------------------------------------------------------------------
void initExternPtrs()
{
    _tbar = &_wndMain.m_toolBar;
    _lp = _wndMain.m_gameview.getMethods();
    _pdata = _wndMain.m_gameview.getPropData();
    _stdfont = _wndMain.m_gameview.getStandardFont();
    _palette = _wndMain.m_gameview.getPalette();
    _pmanager = _wndMain.m_gameview.getPropManager();
    _plugins_manager = _wndMain.m_gameview.getPluginsManager();
}

void collectGarbage()
{
    if (L)
        lua_gc(L, LUA_GCSTEP, 1);
}

void closeLuaState()
{
    L.close();
}
//--------------------------------------------------------------------
UINT getId(int code, bool button) { return m_idcontrol.registerPlugin(_cp, code, button); }
UINT delCode(int code, bool button) { return m_idcontrol.unregisterByCode(_cp, code, button); }
UINT findId(int code, bool button) { return m_idcontrol.findId(_cp,code,button); }
void delId(UINT id) { m_idcontrol.unregisterById(_cp, id); }
void pluginsMenuCmd(UINT id) { m_idcontrol.runPluginCmd(id); }
void tmcLog(const tstring& msg) { _lp->tmcLog(msg); }
void pluginLog(const tstring& msg) { _lp->pluginLog(msg);  }
void pluginsUpdateActiveObjects(int type, const tstring& pattern) { _lp->updateActiveObjects(type, pattern); }
const wchar_t* lua_types_str[] = {L"nil", L"bool", L"lightud", L"number", L"string", L"table", L"function", L"userdata", L"thread",  };
const wchar_t* unknown_plugin = L"?������?";
//---------------------------------------------------------------------
MemoryBuffer pluginBuffer(16384*sizeof(wchar_t));
wchar_t* plugin_buffer() { return (wchar_t*)pluginBuffer.getData(); }
int pluginInvArgs(lua_State *L, const utf8* fname)
{
    int n = lua_gettop(L);
    Utf8ToWide f(fname);
    swprintf(plugin_buffer(), L"'%s'.%s: ������������ ���������(%d): ",
        _cp ? _cp->get(Plugin::FILE) : unknown_plugin, (const wchar_t*)f, n);
    tstring log(plugin_buffer());
    for (int i = 1; i <= n; ++i)
    {
        int t = lua_type(L, i);
        if (t >= 0 && t < LUA_NUMTAGS)
        {
            tstring type(lua_types_str[t]);
            if (t == LUA_TUSERDATA)
                type.assign(TU2W(luaT_typename(L, i)));
            log.append(type);
        }
        else
            log.append(L"unknown");
        if (i != n) log.append(L",");
    }
    pluginLog(log.c_str());
    return 0;
}

int pluginError(const utf8* fname, const utf8* error)
{
    Utf8ToWide f(fname);
    Utf8ToWide e(error);
    swprintf(plugin_buffer(), L"'%s'.%s: %s", _cp ? _cp->get(Plugin::FILE) : unknown_plugin, (const wchar_t*)f, (const wchar_t*)e);
    pluginLog(plugin_buffer());
    return 0;
}

int pluginError(const utf8* error)
{
    Utf8ToWide e(error);
    swprintf(plugin_buffer(), L"'%s': %s", _cp ? _cp->get(Plugin::FILE) : unknown_plugin, (const wchar_t*)e);
    pluginLog(plugin_buffer());
    return 0;
}

int pluginLog(const utf8* msg)
{
    Utf8ToWide e(msg);
    swprintf(plugin_buffer(), L"'%s': %s", _cp ? _cp->get(Plugin::FILE) : unknown_plugin, (const wchar_t*)e);
    pluginLog(plugin_buffer());
    return 0;
}

void pluginLoadError(const wchar_t* msg, const wchar_t *fname)
{
    swprintf(plugin_buffer(), L"'%s': %s", fname, msg);
    pluginLog(plugin_buffer());
}
//---------------------------------------------------------------------
int pluginName(lua_State *L)
{
    if (luaT_check(L, 1, LUA_TSTRING))
    {
        _extra_plugin_name.assign(TU2W(lua_tostring(L, 1))); 
        return 0;
    }
    return pluginInvArgs(L, "pluginName");
}
//---------------------------------------------------------------------
int addCommand(lua_State *L)
{
    CAN_DO;
    EXTRA_CP;
    if (_cp && luaT_check(L, 1, LUA_TSTRING))
    {
        bool result = false;
        tstring cmd(luaT_towstring(L, 1));
        if (!cmd.empty())
            result = _lp->addSystemCommand(cmd);
        if (result)
            _cp->commands.push_back(cmd);
        lua_pushboolean(L, result ? 1 : 0);
        return 1;
    }
    return pluginInvArgs(L, "addCommand");
}

int runCommand(lua_State *L)
{
    EXTRA_CP;
    if (luaT_check(L, 1, LUA_TSTRING))
    {
        tstring cmd(TU2W(lua_tostring(L, 1)));
        _lp->doGameCommand(cmd);
        return 0;
    }
    return pluginInvArgs(L, "runCommand");
}

int addMenu(lua_State *L)
{
    CAN_DO;
    EXTRA_CP;
    if (!_cp)
         return pluginInvArgs(L, "addMenu");
    int code = -1;
    bool params_ok = false;
    if (luaT_check(L, 1, LUA_TSTRING))
        params_ok = _tbar->addMenuItem(luaT_towstring(L, 1), -1, -1, NULL);
    else if (luaT_check(L, 2, LUA_TSTRING, LUA_TNUMBER))
    {
        code = lua_tointeger(L, 2);
        params_ok = _tbar->addMenuItem(luaT_towstring(L, 1), -1, getId(code, false), NULL);
    }
    else if (luaT_check(L, 3, LUA_TSTRING, LUA_TNUMBER, LUA_TNUMBER))
    {
        code = lua_tointeger(L, 2);
        params_ok = _tbar->addMenuItem(luaT_towstring(L, 1), lua_tointeger(L, 3), getId(code, false), NULL);
    }
    else if (luaT_check(L, 4, LUA_TSTRING, LUA_TNUMBER, LUA_TNUMBER, LUA_TNUMBER))
    {
        HBITMAP bmp = NULL;
        HMODULE module = _cp->getModule();
        if (module) bmp = LoadBitmap( module, MAKEINTRESOURCE(lua_tointeger(L, 4)) );
        code = lua_tointeger(L, 2);
        params_ok = _tbar->addMenuItem(luaT_towstring(L, 1), lua_tointeger(L, 3), getId(code, false), bmp);
    }
    else { return pluginInvArgs(L, "addMenu"); }
    if (!params_ok) { delCode(code, false); }
    else
    {
        tstring menu_id(luaT_towstring(L, 1));
        _cp->menus.push_back(menu_id);
    }
    return 0;
}

int checkMenu(lua_State *L)
{
    CAN_DO;
    EXTRA_CP;
    if (_cp && luaT_check(L, 1, LUA_TNUMBER))
    {
        int code = lua_tointeger(L, -1);
        _tbar->checkMenuItem(findId(code, false), TRUE);
        _tbar->checkToolbarButton(findId(code, true), TRUE);
        return 0;
    }
    return pluginInvArgs(L, "checkMenu");
}

int uncheckMenu(lua_State *L)
{
    CAN_DO;
    EXTRA_CP;
    if (_cp && luaT_check(L, 1, LUA_TNUMBER))
    {
        int code = lua_tointeger(L, -1);
        _tbar->checkMenuItem(findId(code, false), FALSE);
        _tbar->checkToolbarButton(findId(code, true), FALSE);
        return 0;
    }
    return pluginInvArgs(L, "uncheckMenu");
}

int enableMenu(lua_State *L)
{
    CAN_DO;
    EXTRA_CP;
    if (_cp && luaT_check(L, 1, LUA_TNUMBER))
    {
        int code = lua_tointeger(L, -1);
        _tbar->enableMenuItem(findId(code, false), TRUE);
        _tbar->enableToolbarButton(findId(code, true), TRUE);
        return 0;
    }
    return pluginInvArgs(L, "enableMenu");
}

int disableMenu(lua_State *L)
{
    CAN_DO;
    EXTRA_CP;
    if (_cp && luaT_check(L, 1, LUA_TNUMBER))
    {
        int code = lua_tointeger(L, -1);
        _tbar->enableMenuItem(findId(code, false), FALSE);
        _tbar->enableToolbarButton(findId(code, true), FALSE);
        return 0;
    }
    return pluginInvArgs(L, "disableMenu");
}

int addButton(lua_State *L)
{
    CAN_DO;
    EXTRA_CP;
    if (!_cp) 
        return pluginInvArgs(L, "addButton");
    int image = -1, code = -1; tstring hover;
    if (luaT_check(L, 2, LUA_TNUMBER, LUA_TNUMBER))
    {
        image = lua_tointeger(L, 1); code = lua_tointeger(L, 2);
    }
    else if (luaT_check(L, 3, LUA_TNUMBER, LUA_TNUMBER, LUA_TSTRING))
    {
        image = lua_tointeger(L, 1); code = lua_tointeger(L, 2); hover.assign(luaT_towstring(L, 3));
    }
    else { return pluginInvArgs(L, "addButton"); }

    if (image > 0 && code > 0)
    {
        HMODULE module = _cp->getModule();
        HBITMAP bmp = NULL;
        if (module && (bmp = LoadBitmap(module, MAKEINTRESOURCE(image))))
        {
            if (!_tbar->addToolbarButton(bmp, getId(code, true), hover.c_str()))
                delCode(code, true);
            else
                _cp->buttons.push_back(code);
        }
    }
    return 0;
}

int addToolbar(lua_State *L)
{
    CAN_DO;
    EXTRA_CP;
    if (!_cp)
        return pluginInvArgs(L, "addToolbar");
    if (luaT_check(L, 1, LUA_TSTRING))
        _tbar->addToolbar(luaT_towstring(L, 1), 15);
    else if (luaT_check(L, 2, LUA_TSTRING, LUA_TNUMBER))
        _tbar->addToolbar(luaT_towstring(L, 1), lua_tointeger(L, 2));
    else { return pluginInvArgs(L, "addToolbar"); }
    tstring tbar_name(luaT_towstring(L, 1));
    _cp->toolbars.push_back(tbar_name);
    return 0;
}

int hideToolbar(lua_State *L)
{
    CAN_DO;
    EXTRA_CP;
    if (_cp && luaT_check(L, 1, LUA_TSTRING))
    {
        _tbar->hideToolbar(luaT_towstring(L, 1));
        return 0;
    }
    return pluginInvArgs(L, "hideToolbar");
}

int showToolbar(lua_State *L)
{
    CAN_DO;
    EXTRA_CP;
    if (_cp && luaT_check(L, 1, LUA_TSTRING))
    {
        _tbar->showToolbar(luaT_towstring(L, 1));
        return 0;
    }
    return pluginInvArgs(L, "showToolbar");
}

int getPath(lua_State *L)
{
    EXTRA_CP;
    if (_cp && luaT_check(L, 1, LUA_TSTRING))
    {
        tstring filename(luaT_towstring(L, 1));
        ProfilePluginPath pp(_pmanager->getProfileGroup(), _cp->get(Plugin::FILENAME), filename);
        ProfileDirHelper dh;
        if (dh.makeDir(_pmanager->getProfileGroup(), _cp->get(Plugin::FILENAME)))
        {
            luaT_pushwstring(L, pp);
            return 1;
        }
        return pluginError("getPath", "������ �������� �������� ��� �������");
    }
    return pluginInvArgs(L, "getPath");
}

int getPathAll(lua_State* L)
{
    EXTRA_CP;
    if (_cp && luaT_check(L, 1, LUA_TSTRING))
    {
        tstring filename(luaT_towstring(L, 1));
        ProfilePluginPath pp(L"all", _cp->get(Plugin::FILENAME), filename);
        ProfileDirHelper dh;
        if (dh.makeDir(L"all", _cp->get(Plugin::FILENAME)))
        {
            luaT_pushwstring(L, pp);
            return 1;
        }
        return pluginError("getPathAll", "������ �������� �������� ��� �������");
    }
    return pluginInvArgs(L, "getPathAll");

}

int getProfile(lua_State *L)
{
    EXTRA_CP;
    if (luaT_check(L, 0))
    {
        luaT_pushwstring(L, _pmanager->getProfileName().c_str());
        return 1;
    }
    return pluginInvArgs(L, "getProfile");
}

int getParent(lua_State *L)
{
    EXTRA_CP;
    if (luaT_check(L, 0))
    {
        HWND hwnd = _wndMain;
        lua_pushunsigned(L, (DWORD)hwnd);
        return 1;
    }
    return pluginInvArgs(L, "getParent");
}

int loadTable(lua_State *L)
{
    EXTRA_CP;
    if (_cp && luaT_check(L, 1, LUA_TSTRING))
    {
        tstring filename(luaT_towstring(L, 1));
        ProfilePluginPath pp(_pmanager->getProfileGroup(), _cp->get(Plugin::FILENAME), filename);
        filename.assign(pp);

        DWORD fa = GetFileAttributes(filename.c_str());
        if (fa == INVALID_FILE_ATTRIBUTES || fa&FILE_ATTRIBUTE_DIRECTORY)
            return 0;
        xml::node doc;
        if (!doc.load(WideToUtf8(pp)) )
        {
            W2U f(filename);
            utf8 buffer[128];
            sprintf(buffer, "������ ������: %s", (const utf8*)f);
            pluginError("loadData", buffer);
            return 0;
        }
        lua_pop(L, 1);

        struct el { el(xml::node n, int l) : node(n), level(l) {} xml::node node; int level; };
        std::vector<el> stack;
        xml::request req(doc, "*");
        for (int i = 0, e = req.size(); i < e; ++i)
            { stack.push_back( el(req[i], 0) ); }

        lua_newtable(L);            // root (main) table 
        bool pop_stack_flag = false;
        int p = 0;
        int size = stack.size();
        while (p != size)
        {
            if (!pop_stack_flag)
                lua_newtable(L);
            pop_stack_flag = false;

            el s_el = stack[p++];
            xml::node n = s_el.node;
            std::string aname, avalue;
            for (int j = 0, atts = n.size(); j < atts; ++j)
            {
                n.getattrname(j, &aname);
                n.getattrvalue(j, &avalue);
                lua_pushstring(L, aname.c_str());
                lua_pushstring(L, avalue.c_str());
                lua_settable(L, -3);
            }

            lua_pushvalue(L, -1);
            std::string name;
            n.getname(&name);
            lua_pushstring(L, name.c_str());
            lua_insert(L, -2);
            lua_settable(L, -4);

            // insert subnodes
            {
                int new_level = s_el.level + 1;
                xml::request req(n, "*");
                std::vector<el> tmp;
                for (int i = 0, e = req.size(); i < e; ++i)
                    tmp.push_back(el(req[i], new_level));
                if (!tmp.empty())
                {
                    stack.insert(stack.begin() + p, tmp.begin(), tmp.end());
                    size = stack.size();
                }
            }

            // goto next node
            if (p == size)
            {
                lua_settop(L, 1);
                break;
            }
            el s_el2 = stack[p];
            if (s_el2.level < s_el.level)  // pop from stack
            {
                lua_pop(L, 1);
                pop_stack_flag = true;
            }
            else if (s_el2.level == s_el.level)
            {
                lua_pop(L, 1);
            }
        }
        doc.deletenode();
        return 1;
    }
    return pluginInvArgs(L, "loadData");
}

int saveTable(lua_State *L)
{
    EXTRA_CP;
    if (!_cp || !luaT_check(L, 2, LUA_TTABLE, LUA_TSTRING))
        return pluginInvArgs(L, "saveData");

    tstring filename(luaT_towstring(L, 2));
    lua_pop(L, 1);

    // recursive cycles in table
    struct saveDataNode
    {
        typedef std::pair<std::string, std::string> value;
        std::vector<value> attributes;
        std::vector<saveDataNode*> childnodes;
        std::string name;
    };

    saveDataNode *current = new saveDataNode();
    current->name = "plugindata";

    std::vector<saveDataNode*> stack;
    std::vector<saveDataNode*> list;
    list.push_back(current);

    bool incorrect_data = false;

    lua_pushnil(L);                         // first key
    while (true)
    {
        bool pushed = false;
        while (lua_next(L, -2) != 0)        // key index = -2, value index = -1
        {
            int value_type = lua_type(L, -1);
            int key_type = lua_type(L, -2);
            if (key_type != LUA_TSTRING && key_type != LUA_TNUMBER) 
            {
                lua_pop(L, 1);
                incorrect_data = true;
                continue; 
            }
            if (key_type == LUA_TNUMBER)
            {
                if (value_type != LUA_TTABLE) {
                    lua_pop(L, 1);
                    incorrect_data = true;
                    continue;
                }
            }
            if (value_type == LUA_TNUMBER || value_type == LUA_TSTRING || value_type == LUA_TBOOLEAN)
            {
                current->attributes.push_back( saveDataNode::value( lua_tostring(L, -2), lua_tostring(L, -1)) );
            }
            else if (value_type == LUA_TTABLE)
            {
                saveDataNode* new_node = new saveDataNode();
                if (key_type == LUA_TNUMBER)
                    new_node->name = current->name;
                else
                    new_node->name = lua_tostring(L, -2);
                current->childnodes.push_back(new_node);
                stack.push_back(current);
                list.push_back(new_node);
                current = new_node;
                pushed = true;
                break;
            }
            else { incorrect_data = true; }
            lua_pop(L, 1); // remove 'value', keeps 'key' for next iteration 
        }
        if (pushed) // new iteration for new table (recursivly iteration)
        {
            lua_pushnil(L); 
            continue;
        }
        if (stack.empty())
            break;
        int last = stack.size() - 1;
        current = stack[last];
        stack.pop_back();
        lua_pop(L, 1);
    }
    // sorting
    for (int k = 0, ke = list.size(); k < ke; ++k)
    {
        saveDataNode* node = list[k];
        std::vector<saveDataNode::value>&a = node->attributes;
        int size = a.size() - 1;
        for (int i = 0, e = size - 1; i <= e; ++i) {
        for (int j = i + 1; j <= size; ++j) {
            if (a[i].first > a[j].first) { saveDataNode::value t(a[i]); a[i] = a[j]; a[j] = t; }
        }}
        std::vector<saveDataNode*>&n = node->childnodes;
        size = n.size() - 1;
        for (int i = 0, e = size - 1; i <= e; ++i) {
        for (int j = i + 1; j <= size; ++j) {
            if (n[i]->name > n[j]->name) { saveDataNode *t = n[i]; n[i] = n[j]; n[j] = t; }
        }}
    }

    // make xml
    xml::node root(current->name.c_str());
    typedef std::pair<saveDataNode*, xml::node> _xmlstack;
    std::vector<_xmlstack> xmlstack;
    xmlstack.push_back(_xmlstack(current, root));

    int index = 0;
    while (index < (int)xmlstack.size())
    {
        _xmlstack &v = xmlstack[index++];
        xml::node node = v.second;
        std::vector<saveDataNode::value>&a = v.first->attributes;
        for (int i = 0, e = a.size(); i < e; ++i)
            node.set(a[i].first.c_str(), a[i].second.c_str());
        std::vector<saveDataNode*>&n = v.first->childnodes;
        for (int i = 0, e = n.size(); i < e; ++i)
        {
            xml::node new_node = node.createsubnode(n[i]->name.c_str());
            xmlstack.push_back(_xmlstack(n[i], new_node));
        }
    }
    xmlstack.clear();

    // delete temporary datalist
    for (int i = 0, e = list.size(); i < e; ++i)
       delete list[i];
    list.clear();

    // save xml
    ProfilePluginPath pp(_pmanager->getProfileGroup(), _cp->get(Plugin::FILENAME), filename);
    const wchar_t *filepath = pp;

    int result = 0;
    ProfileDirHelper dh;
    if (dh.makeDirEx(_pmanager->getProfileGroup(), _cp->get(Plugin::FILENAME), filename))
    {
        result = root.save(WideToUtf8(filepath));
    }

    if (incorrect_data)
        pluginError("saveData", "�������� ������ � �������� ������.");

    if (!result)
    {
       W2U f(filepath);
       utf8 buffer[128];
       sprintf(buffer, "������ ������: %s", (const utf8*)f);
       pluginError("saveData", buffer);
    }
    root.deletenode();
    return 0;
}
//----------------------------------------------------------------------------
void initVisible(lua_State *L, int index, OutputWindow *w)
{
    if (lua_gettop(L) == index)
      w->initVisible(lua_toboolean(L, index) ? true : false);
}

int createWindow(lua_State *L)
{
    EXTRA_CP;
    if (!_cp)
        return pluginInvArgs(L, "createWindow");
    PluginData &p = find_plugin();
    OutputWindow w;

    if (luaT_check(L, 1, LUA_TSTRING) || luaT_check(L, 2, LUA_TSTRING, LUA_TBOOLEAN))
    {
        tstring name( U2W(lua_tostring(L, 1)) );
        if (!p.findWindow(name, &w))
        {
            p.initDefaultPos(300, 300, &w);
            initVisible(L, 2, &w);
            w.name = name;
            p.windows.push_back(w);
        }
        else { initVisible(L, 2, &w); }
    }
    else if (luaT_check(L, 3, LUA_TSTRING, LUA_TNUMBER, LUA_TNUMBER )|| 
             luaT_check(L, 4, LUA_TSTRING, LUA_TNUMBER, LUA_TNUMBER, LUA_TBOOLEAN))
    {
        tstring name(U2W(lua_tostring(L, 1)));
        if (!p.findWindow(name, &w))
        {
            int height = lua_tointeger(L, 3);
            int width = lua_tointeger(L, 2);
            p.initDefaultPos(width, height, &w);
            initVisible(L, 4, &w);
            w.name = name;
            p.windows.push_back(w);
        }
        else { initVisible(L, 4, &w); }
    }
    else {
        return pluginInvArgs(L, "createWindow"); 
    }

    PluginsView *window =  _wndMain.m_gameview.createDockPane(w, _cp);
    if (window)
        _cp->dockpanes.push_back(window);
    luaT_pushobject(L, window, LUAT_WINDOW);
    return 1;
}

int pluginLog(lua_State *L)
{
    EXTRA_CP;
    int n = lua_gettop(L);
    if (n == 0)
        return pluginInvArgs(L, "log");

    u8string log;
    for (int i = 1; i <= n; ++i)
    {
        u8string el;
        pluginFormatByType(L, i, &el);
        log.append(el);
    }
    pluginLog(log.c_str());
    return 0;
}

int terminatePlugin(lua_State *L)
{
    EXTRA_CP;
    if (!_cp)
        { assert(false); return 0; }

    int n = lua_gettop(L);
    u8string log;
    for (int i = 1; i <= n; ++i)
    {
        u8string el;
        pluginFormatByType(L, i, &el);
        log.append(el);
    }
    _cp->setErrorState();

    if (log.empty())
        log.assign("TERMINATE");
    lua_settop(L, 0);
    lua_pushstring(L, log.c_str());
    lua_error(L);
    return 0;
}

int regUnloadFunction(lua_State *L)
{
    if (luaT_check(L, 1, LUA_TFUNCTION))
    {
        lua_getglobal(L, "muloadf");
        if (!lua_istable(L, -1))
        {
            if (!lua_isnil(L, -1)) 
            {
                lua_pop(L, 1);
                lua_pushboolean(L, 0);
                return 1;
            }
            lua_pop(L, 1);
            lua_newtable(L);
            lua_pushvalue(L, -1);
            lua_setglobal(L, "muloadf");
        }
        lua_len(L, -1);
        int index = lua_tointeger(L, -1) + 1;
        lua_pop(L, 1);
        lua_insert(L, -2);
        lua_pushinteger(L, index);
        lua_insert(L, -2);
        lua_settable(L, -3);
        lua_pop(L, 1);        
        lua_pushboolean(L, 1);
        return 1;
    }
    lua_pushboolean(L, 0);
    return 1;
}

void unloadModules()
{
    lua_getglobal(L, "muloadf");
    if (!lua_istable(L, -1))
    {
        lua_pop(L,1);
        return;
    }
    lua_pushnil(L);                     // first key
    while (lua_next(L, -2) != 0)        // key index = -2, value index = -1
    {
        if (lua_isfunction(L, -1))
            lua_pcall(L, 0, 0, 0);
        else
            lua_pop(L, 1);
    }
}
//---------------------------------------------------------------------
// Metatables for all types
void reg_mt_window(lua_State *L);
void reg_mt_viewdata(lua_State *L);
void reg_activeobjects(lua_State *L);
void reg_string(lua_State *L);
void reg_props(lua_State *L);
void reg_mt_panels(lua_State *L);
void reg_mt_render(lua_State *L);
void reg_mt_pcre(lua_State *L);
void reg_msdp(lua_State *L);
//---------------------------------------------------------------------
bool initPluginsSystem()
{
    initExternPtrs();
    if (!L)
        return false;

    luaopen_base(L);
    lua_pop(L, 1);
    luaopen_math(L);
    lua_setglobal(L, "math");
    luaopen_table(L);
    lua_setglobal(L, "table");
    reg_string(L);
    lua_register(L, "addCommand", addCommand);
    lua_register(L, "runCommand", runCommand);
    lua_register(L, "addMenu", addMenu);
    lua_register(L, "addButton", addButton);
    lua_register(L, "addToolbar", addToolbar);
    lua_register(L, "hideToolbar", hideToolbar);
    lua_register(L, "showToolbar", showToolbar);
    lua_register(L, "checkMenu", checkMenu);
    lua_register(L, "uncheckMenu", uncheckMenu);
    lua_register(L, "enableMenu", enableMenu);
    lua_register(L, "disableMenu", disableMenu);
    lua_register(L, "getPath", getPath);
    lua_register(L, "getPathAll", getPathAll);
    lua_register(L, "getProfile", getProfile);
    lua_register(L, "getParent", getParent);
    lua_register(L, "loadTable", loadTable);
    lua_register(L, "saveTable", saveTable);
    lua_register(L, "createWindow", createWindow);
    lua_register(L, "log", pluginLog);
    lua_register(L, "terminate", terminatePlugin);
    lua_register(L, "pluginName", pluginName);
    lua_register(L, "regUnloadFunction", regUnloadFunction);

    reg_props(L);
    reg_activeobjects(L);
    reg_mt_window(L);
    reg_mt_viewdata(L);
    reg_mt_panels(L);
    reg_mt_render(L);
    reg_mt_pcre(L);
    reg_msdp(L);
    return true;
}

void pluginDeleteResources(Plugin *plugin)
{
    // delete UI (menu, buttons, etc)
    Plugin *old = _cp;
    _cp = plugin;
    for (int i = 0, e = plugin->menus.size(); i < e; ++i)
    {
        std::vector<UINT> ids;
        _tbar->deleteMenuItem(plugin->menus[i].c_str(), &ids);
        for (int i = 0, e = ids.size(); i < e; ++i)
            delId(ids[i]);
    }
    plugin->menus.clear();
    for (int i = 0, e = plugin->buttons.size(); i < e; ++i)
        _tbar->deleteToolbarButton(delCode(plugin->buttons[i], true));
    plugin->buttons.clear();
    for (int i = 0, e = plugin->toolbars.size(); i<e; ++i)
        _tbar->deleteToolbar(plugin->toolbars[i].c_str());
    plugin->toolbars.clear();
    for (int i = 0, e = plugin->dockpanes.size(); i < e; ++i)
        _wndMain.m_gameview.deleteDockPane(plugin->dockpanes[i]);
    plugin->dockpanes.clear();
    for (int i = 0, e = plugin->panels.size(); i < e; ++i)
        _wndMain.m_gameview.deletePanel(plugin->panels[i]);
    plugin->panels.clear();

    // delete all system commands of plugin
    for (int i = 0, e = plugin->commands.size(); i < e; ++i)
        _lp->deleteSystemCommand(plugin->commands[i]);
    plugin->commands.clear();
    _cp = old;
}
//--------------------------------------------------------------------
int string_len(lua_State *L)
{
    int len = 0;
    if (luaT_check(L, 1, LUA_TSTRING))
    {
        u8string str(lua_tostring(L, 1));
        len = u8string_len(str);
    }
    lua_pushinteger(L, len);
    return 1;
}

int string_substr(lua_State *L)
{
    if (luaT_check(L, 2, LUA_TSTRING, LUA_TNUMBER))
    {
        u8string s(lua_tostring(L, 1));
        u8string_substr(&s, 0, lua_tointeger(L, 2));
        lua_pushstring(L, s.c_str());
        return 1;
    }
    if (luaT_check(L, 3, LUA_TSTRING, LUA_TNUMBER, LUA_TNUMBER))
    {
        u8string s(lua_tostring(L, 1));
        int from = lua_tointeger(L, 2);
        if (from < 1)
            s.clear();
        else
            u8string_substr(&s, from-1, lua_tointeger(L, 3));
        lua_pushstring(L, s.c_str());
        return 1;
    }
    lua_pushnil(L);
    return 1;
}

extern void regFunction(lua_State *L, const char* name, lua_CFunction f);
extern void regIndexMt(lua_State *L);
void reg_string(lua_State *L)
{
    lua_newtable(L);
    regFunction(L, "len", string_len);
    regFunction(L, "substr", string_substr);
    regIndexMt(L);

    // set metatable for lua string type
    lua_pushstring(L, "");
    lua_insert(L, -2);
    lua_setmetatable(L, -2);
    lua_pop(L, 1);
}

int props_paletteColor(lua_State *L)
{
    if (luaT_check(L, 1, LUA_TNUMBER))
    {
        int index = lua_tointeger(L, 1);
        if (index >= 0 && index <= 255)
        {
            lua_pushunsigned(L, _palette->getColor(index));
            return 1;
        }
    }
    return pluginInvArgs(L, "props.paletteColor");
}

int props_backgroundColor(lua_State *L)
{
    if (luaT_check(L, 0))
    {
        lua_pushunsigned(L, _pdata->bkgnd);
        return 1;
    }
    return pluginInvArgs(L, "props.backgroundColor");
}

int props_currentFont(lua_State *L)
{
    if (luaT_check(L, 0))
    {
        luaT_pushobject(L, _stdfont, LUAT_FONT);
        return 1;
    }
    return pluginInvArgs(L, "props.currentFont");
}

int props_currentFontHandle(lua_State *L)
{
    if (luaT_check(L, 0))
    {
        HFONT handle = *_stdfont;
        lua_pushunsigned(L, (DWORD)handle);
        return 1;
    }
    return pluginInvArgs(L, "props.currentFontHandle");
}

int props_cmdPrefix(lua_State *L)
{
    if (luaT_check(L, 0))
    {
        tchar prefix[2] = { _pdata->cmd_prefix, 0 };
        lua_pushstring(L, TW2U(prefix));
        return 1;
    }
    return pluginInvArgs(L, "props.cmdPrefix");
}

int props_cmdSeparator(lua_State *L)
{
    if (luaT_check(L, 0))
    {
        tchar prefix[2] = { _pdata->cmd_separator, 0 };
        lua_pushstring(L, TW2U(prefix));
        return 1;
    }
    return pluginInvArgs(L, "props.cmdSeparator");
}

int props_serverHost(lua_State *L)
{
    if (luaT_check(L, 0))
    {
        if (_lp->getConnectionState())
        {
            const NetworkConnectData *cdata = _wndMain.m_gameview.getConnectData();
            lua_pushstring(L, cdata->address.c_str());
        }
        else
            lua_pushnil(L);
        return 1;
    }
    return pluginInvArgs(L, "props.serverHost");
}

int props_serverPort(lua_State *L)
{
    if (luaT_check(L, 0))
    {
        if (_lp->getConnectionState())
        {
            const NetworkConnectData *cdata = _wndMain.m_gameview.getConnectData();
            WCHAR buffer[8];
            _itow(cdata->port, buffer, 10);
            lua_pushstring(L, TW2U(buffer));
        }
        else
            lua_pushnil(L);
        return 1;
    }
    return pluginInvArgs(L, "props.serverPort");
}

int props_connected(lua_State *L)
{
    if (luaT_check(L, 0))
    {
        lua_pushboolean(L, _lp->getConnectionState() ? 1 : 0);
        return 1;
    }
    return pluginInvArgs(L, "props.connected");
}

int props_activated(lua_State *L)
{
    if (luaT_check(L, 0))
    {
        int state = _wndMain.m_gameview.activated() ? 1 : 0;
        lua_pushboolean(L, state);
        return 1;
    }
    return pluginInvArgs(L, "props.activated");
}

int props_settingsWnd(lua_State *L)
{
    if (luaT_check(L, 0))
    {
        int state = _wndMain.m_gameview.isSettingsMode() ? 1 : 0;
        lua_pushboolean(L, state);
        return 1;
    }
    return pluginInvArgs(L, "props.settingsWnd");
}

void reg_props(lua_State *L)
{
    lua_newtable(L);
    regFunction(L, "paletteColor", props_paletteColor);
    regFunction(L, "backgroundColor", props_backgroundColor);
    regFunction(L, "currentFont", props_currentFont);
    regFunction(L, "currentFontHandle", props_currentFontHandle);
    regFunction(L, "cmdPrefix", props_cmdPrefix);
    regFunction(L, "cmdSeparator", props_cmdSeparator);
    regFunction(L, "serverHost", props_serverHost);
    regFunction(L, "serverPort", props_serverPort);
    regFunction(L, "connected", props_connected);
    regFunction(L, "activated", props_activated);
    regFunction(L, "settingsWnd", props_settingsWnd);
    lua_setglobal(L, "props");
}
