#pragma once

#include "base.h"

#ifndef API_EXPORTS
#pragma comment(lib, "api.lib")
#endif

#include <string>
typedef char utf8;
typedef std::string u8string;

//lua api
#define LUAT_WINDOW     100
#define LUAT_VIEWDATA   101
#define LUAT_ACTIVEOBJS 102

bool  luaT_check(lua_State *L, int n, ...);
bool  luaT_run(lua_State *L, const utf8* func, const utf8* op, ...);
int   luaT_error(lua_State *L, const utf8* error_message);
void  luaT_log(lua_State *L, const utf8* log_message);
void* luaT_toobject(lua_State* L, int index);
void  luaT_pushobject(lua_State* L, void *object, int type);
bool  luaT_isobject(lua_State* L, int type, int index);
void  luaT_showLuaStack(lua_State* L, const char* label);
void  luaT_showTableOnTop(lua_State* L, const char* label);
#define SS(L,n) luaT_showLuaStack(L,n)
#define ST(L,n) luaT_showTableOnTop(L,n)

//lua window wrapper
class luaT_window
{
    lua_State *L;
    void *window;
public:
    luaT_window() : L(NULL), window(NULL) {}
    void init(lua_State *pL, void *window_object) { L = pL; window = window_object; }

    HWND hwnd()
    {
        luaT_pushobject(L, window, LUAT_WINDOW);
        luaT_run(L, "hwnd", "o");
        HWND hwnd = (HWND)lua_tointeger(L, -1);
        lua_pop(L, 1);
        return hwnd;
    }
    void hide()
    {
        luaT_pushobject(L, window, LUAT_WINDOW);
        luaT_run(L, "hide", "o");
    }
    void show()
    {
        luaT_pushobject(L, window, LUAT_WINDOW);
        luaT_run(L, "show", "o");
    }
    bool isvisible()
    {
        luaT_pushobject(L, window, LUAT_WINDOW);
        luaT_run(L, "isvisible", "o");
        bool result = (lua_toboolean(L, -1) == 0) ? false : true;
        lua_pop(L, 1);
        return result;
    }
    void attach(HWND child)
    {
        luaT_pushobject(L, window, LUAT_WINDOW);
        luaT_run(L, "attach", "od", child);
    }
};

class luaT_ViewData
{
    lua_State *L;
    void *view_data;
public:
    enum { TEXTCOLOR = 0, BKGCOLOR, UNDERLINE, ITALIC, BLINK, REVERSE, EXTTEXTCOLOR, EXTBKGCOLOR };

    luaT_ViewData() : L(NULL), view_data(NULL) {}
    void init(lua_State *pL, void *viewdata) { L = pL; view_data = viewdata; }
    int size()             // count of all strings
    { 
        runcmd("size");
        return intresult(); 
    }
    bool select(int string) // select string for operations
    {
        runcmdint("select", string);
        return boolresult(); 
    }
    bool isfirst()
    {
        runcmd("isfirst");
        return boolresult(); 
    }
    bool isgamecmd()
    {
        runcmd("isgamecmd");
        return boolresult();
    }
    void gettext(u8string* str) 
    {
        runcmd("gettext");
        strresult(str); 
    }
    void gethash(u8string* str)
    {
        runcmd("gethash");
        strresult(str);
    }
    int blocks()            // count of blocks for selected string
    { 
        runcmd("blocks");
        return intresult(); 
    }
    void getblock(int block, u8string* str)
    {
        runcmdint("getblock", block);
        strresult(str);
    }
    bool get(int block, int param, unsigned int *value)
    {
        luaT_pushobject(L, view_data, LUAT_VIEWDATA);
        luaT_run(L, "get", "odd", block, param);
        bool result = false;
        if (lua_isnumber(L, -1)) { *value = lua_tounsigned(L, -1); result = true; }
        lua_pop(L, 1);
        return result;
    }
    bool set(int block, int param, unsigned int value)
    {
        luaT_pushobject(L, view_data, LUAT_VIEWDATA);
        luaT_run(L, "set", "oddu", block, param, value);
        return boolresult();
    }
    bool setblocktext(int block, const utf8* text)
    {
        luaT_pushobject(L, view_data, LUAT_VIEWDATA);
        luaT_run(L, "setblocktext", "ods", block, text);
        return boolresult();
    }
    bool deleteblock(int block)
    {
        runcmdint("deleteblock", block);
        return boolresult();
    }
    bool deleteallblocks()
    {
        runcmd("deleteallblocks");
        return boolresult();
    }
    bool deletestring()
    {
        runcmd("deletestring");
        return boolresult();
    }

private:
    void runcmd(const utf8* cmd)
    {
        luaT_pushobject(L, view_data, LUAT_VIEWDATA);
        luaT_run(L, cmd, "o");
    }
    void runcmdint(const utf8* cmd, int p)
    {
        luaT_pushobject(L, view_data, LUAT_VIEWDATA);
        luaT_run(L, cmd, "od", p);
    }
    bool boolresult()
    {
        int result = (lua_isboolean(L, -1)) ? lua_toboolean(L, -1) : 0;
        lua_pop(L, 1);
        return result ? true : false;
    }
    int intresult()
    {
        int result = (lua_isnumber(L, -1)) ? lua_tointeger(L, -1) : 0;
        lua_pop(L, 1);
        return result;
    }
    void strresult(u8string *res)
    {
        if (lua_isstring(L, -1)) res->assign(lua_tostring(L, -1));
        else res->clear();
        lua_pop(L, 1);
    }    
};

// active objects api
// supported types: aliases,actions,subs,antisubs,highlights,hotkeys,gags,vars,groups,timers,tabs
class luaT_ActiveObjects
{
    lua_State *L;
    u8string aotype;
public:
    enum { KEY = 0, VALUE, GROUP };
    luaT_ActiveObjects(lua_State *pL, const char* type) : L(pL), aotype(type)
    {
    }
    int size()
    {
        if (!getao()) return 0;
        luaT_run(L, "size", "o");
        return intresult();
    }
    bool select(int index)
    {
        if (!getao()) return false;
        luaT_run(L, "select", "od", index);
        return boolresult();
    }
    bool add(const utf8* key, const utf8* value, const utf8* group)
    {
        if (!getao()) return false;
        luaT_run(L, "add", "osss", key, value, group);
        return boolresult();
    }
    bool del()
    {
        if (!getao()) return false;
        luaT_run(L, "delete", "o");
        return boolresult();
    }
    int getindex()
    {
        if (!getao()) return false;
        luaT_run(L, "getindex", "o");
        return intresult();
    }
    bool setindex(int index)
    {
        if (!getao()) return false;
        luaT_run(L, "setindex", "od", index);
        return boolresult();
    }
    bool get(int param, u8string* value)
    {
        if (!param || !value || !getao()) return false;
        luaT_run(L, "get", "od", param);
        return strresult(value);
    }
    bool set(int param, const utf8* value)
    {
        if (!param || !value || !getao()) return false;
        luaT_run(L, "set", "ods", param, value);
        return boolresult();
    }
    bool update()
    {
        if (!getao()) return false;
        luaT_run(L, "update", "o");
        return true;
    }
private:
    bool getao()
    {
        lua_getglobal(L, aotype.c_str());
        if (!luaT_isobject(L, LUAT_ACTIVEOBJS, -1))
        {
            lua_pop(L, 1);
            return false;
        }
        return true;
    }
    bool boolresult()
    {
        int result = (lua_isboolean(L, -1)) ? lua_toboolean(L, -1) : 0;
        lua_pop(L, 1);
        return result ? true : false;
    }
    int intresult()
    {
        int result = (lua_isnumber(L, -1)) ? lua_tointeger(L, -1) : 0;
        lua_pop(L, 1);
        return result;
    }
    bool strresult(u8string *res)
    {
        bool result = false;
        if (lua_isstring(L, -1)) 
            { res->assign(lua_tostring(L, -1)); result = true; } 
        else res->clear();
        lua_pop(L, 1);
        return result;
    }
};

// utf8 <-> wide <-> ansi
const wchar_t* convert_utf8_to_wide(const utf8* string);
const utf8* convert_wide_to_utf8(const wchar_t* string);
const wchar_t* convert_ansi_to_wide(const char* string);
const char* convert_wide_to_ansi(const wchar_t* string);

// xml api
typedef void* xnode;
typedef void* xlist;
typedef const utf8* xstring;

xnode xml_load(const utf8* filename);
int   xml_save(xnode node, const utf8* filename);
xnode xml_open(const utf8* name);
void  xml_delete(xnode node);
void  xml_set(xnode node, const utf8* name, const utf8* value);
xstring xml_get_name(xnode node);
xstring xml_get_attr(xnode node, const utf8* name);
void  xml_set_text(xnode node, const utf8* text);
xstring xml_get_text(xnode node);
xnode xml_create_child(xnode node, const utf8* childname);
xlist xml_request(xnode node, const utf8* request);
int   xml_get_attr_count(xnode node);
xstring xml_get_attr_name(xnode node, int index);
xstring xml_get_attr_value(xnode node, int index);
xnode xml_move(xnode node, const utf8* path, int create);
void  xml_list_delete(xlist list);
int   xml_list_size(xlist list);
xnode xml_list_getnode(xlist list, int index);

namespace xml
{
    // xml node helper
    class node 
    {
    public:
        node() : m_Node(NULL) {}
        node(const utf8* rootnode) { m_Node = xml_open(rootnode); }
        node(xnode xml_node) : m_Node(xml_node) {}
        ~node() {}
        operator xnode() { return m_Node; }
        operator bool() const { return (m_Node) ? true : false; }
        bool load(const utf8 *filename) { deletenode();  m_Node = xml_load(filename); return m_Node ? true : false; }
        bool save(const utf8 *filename) { int result = xml_save(m_Node, filename); return result ? true : false; }
        void deletenode() { xml_delete(m_Node); m_Node = NULL; }
        void getname(u8string *name) { name->assign(xml_get_name(m_Node)); }
        bool get(const utf8* attribute, u8string* value)  { return _getp(xml_get_attr(m_Node, attribute), value); }
        bool get(const utf8* attribute, int* value)
        {
            u8string result;
            if (!get(attribute, &result)) return false;
            const utf8* num = result.c_str();
            if (strspn(num, "-0123456789") != strlen(num)) return false;
            *value = atoi(num);
            return true;
        }
        bool get(const utf8* attribute, std::wstring* value)
        {
            u8string result;
            if (!get(attribute, &result)) return false;
            value->assign(convert_utf8_to_wide(result.c_str()));
            return true;
        }
        void set(const utf8* attribute, const utf8* value) { xml_set(m_Node, attribute, value); }
        void set(const utf8* attribute, int value) { char buffer[16]; _itoa_s(value, buffer, 10); xml_set(m_Node, attribute, buffer); }
        void set(const utf8* attribute, const std::wstring& value) { xml_set(m_Node, attribute, convert_wide_to_utf8(value.c_str())); }
        void gettext(u8string *text) {  text->assign(xml_get_text(m_Node)); }
        void settext(const utf8* text) { xml_set_text(m_Node, text); }
        xml::node createsubnode(const utf8* name) { return xml::node(xml_create_child(m_Node, name)); }
        int  size() { return xml_get_attr_count(m_Node); }
        bool getattrname(int index, u8string* value) { return _getp(xml_get_attr_name(m_Node, index), value); }
        bool getattrvalue(int index, u8string* value) { return _getp(xml_get_attr_value(m_Node, index), value); }
        bool move(const utf8* path) { return _nmove(path, false); }
        bool create(const utf8* path) { return _nmove(path, true); }

    private:
        bool _getp(xstring str, u8string* value)
        {
            if (!str) { value->clear(); return false; }
            value->assign(str);
            return true;
        }

        bool _nmove(const utf8* path, bool create)
        {
            xnode newnode = xml_move(m_Node, path, create ? 1 : 0);
            if (!newnode) return false;
            m_Node = newnode;
            return true;
        }
        xnode m_Node;
    };

    // xml XPath request helper
    class request
    {
    public:
        request(xnode node, const utf8 *request_string)
        {
            m_NodeList = xml_request(node, request_string);
            m_ListSize = xml_list_size(m_NodeList);
        }
        request(xml::node& node, const utf8 *request_string)
        {
            m_NodeList = xml_request(node, request_string); 
            m_ListSize = xml_list_size(m_NodeList); 
        }
        ~request() { xml_list_delete(m_NodeList);  }
        int   size() const { return m_ListSize; }
        bool  empty() const { return (m_ListSize==0) ? true : false; }
        xml::node operator[](int node_index) { return xml::node(xml_list_getnode(m_NodeList, node_index)); }
    private:
        xlist m_NodeList;
        int   m_ListSize;
    };
}

//pcre api
typedef void* pcre8;
pcre8 pcre_create(const utf8* regexp);
void  pcre_delete(pcre8 handle);
bool  pcre_find(pcre8 handle, const utf8* string);
bool  pcre_findall(pcre8 handle, const utf8* string);
int   pcre_size(pcre8 handle);
int   pcre_first(pcre8 handle, int index);
int   pcre_last(pcre8 handle, int index);
const utf8* pcre_string(pcre8 handle, int index);

// pcre helper
class Pcre
{
public:
    Pcre() : regexp(NULL) {}
    ~Pcre() { pcre_delete(regexp); }
    bool init(const utf8* _rgxp)
    {
        pcre_delete(regexp);
        regexp = pcre_create(_rgxp);
        return (regexp) ? true : false;
    }
    bool find(const utf8* string) { return pcre_find(regexp, string); }
    bool findall(const utf8* string) { return pcre_findall(regexp, string); }
    int  size() { return pcre_size(regexp); }
    int  first(int index) { return pcre_first(regexp, index); }
    int  last(int index) { return pcre_last(regexp, index); }
    void getstring(int index, u8string *str) { str->assign(pcre_string(regexp, index)); }
private:
    pcre8 regexp;
};
