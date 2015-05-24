#include "stdafx.h"
#pragma comment(lib, "lua.lib")

bool paramsok(lua_State *L, int n)
{
    if (lua_gettop(L) != n)
        return false;
    for (int i=1; i<=n; ++i)
    {
        if (!lua_isnumber(L, i) && !lua_isstring(L, i) && !lua_isboolean(L, i) && !lua_isnil(L, i))
            return false;
    }
    return true;
}

const char* tostring(lua_State *L, int index)
{
    if (lua_isnil(L, index))
        return "nil";
    if (lua_isboolean(L, index))
        return lua_toboolean(L, index) ? "true" : "false";
    return lua_tostring(L, index);
}

int system_messagebox(lua_State *L)
{
    HWND parent = base::getParent(L);
    if (!::IsWindow(parent))
        parent = NULL;

    bool params_ok = false;

    u8string text;
    u8string caption("Tortilla Mud Client");
    UINT buttons = MB_OK;
    if (paramsok(L, 1))
    {
        text.assign(tostring(L, 1));
        params_ok = true;
    }
    if (paramsok(L, 2) || paramsok(L, 3))
    {
        if (lua_gettop(L) == 3)
            caption.assign(tostring(L, 3));
        text.assign(tostring(L, 1));
        u8string b(tostring(L, 2));
        if (b == "ok,cancel") buttons = MB_OKCANCEL;
        else if (b == "cancel,ok") buttons = MB_OKCANCEL|MB_DEFBUTTON2;
        else if (b == "yes,no") buttons = MB_YESNO;
        else if (b == "no,yes") buttons = MB_YESNO|MB_DEFBUTTON2;
        if (strstr(b.c_str(), "error")) buttons |= MB_ICONERROR;
        else if (strstr(b.c_str(), "stop")) buttons |= MB_ICONERROR;
        else if (strstr(b.c_str(), "info")) buttons |= MB_ICONINFORMATION;
        else if (strstr(b.c_str(), "information")) buttons |= MB_ICONINFORMATION;
        else if (strstr(b.c_str(), "warning")) buttons |= MB_ICONWARNING;
        params_ok = true;
    }
    UINT result = 0;
    if (params_ok)
        result = MessageBox(parent, TU2W(text.c_str()), TU2W(caption.c_str()), buttons);

    lua_pushinteger(L, result);
    return 1;
}

int system_debugstack(lua_State *L)
{
    if (luaT_check(L, 1, LUA_TSTRING))
    {
        std::string label(lua_tostring(L, -1));
        lua_pop(L, 1);
        luaT_showLuaStack(L, label.c_str());
        return 0;
    }
    luaT_showLuaStack(L, NULL);
    return 0;
}

int system_dbgtable(lua_State *L)
{
    if (luaT_check(L, 2, LUA_TTABLE, LUA_TSTRING))
    {
        std::string label(lua_tostring(L, -1));
        lua_pop(L, 1);
        luaT_showTableOnTop(L, label.c_str());
        return 0;
    }
    if (luaT_check(L, 1, LUA_TTABLE))
        luaT_showTableOnTop(L, NULL);
    return 0;
}

void formatByType(lua_State* L, int index, u8string *buf)
{
    int i = index;
    int type = lua_type(L, i);
    utf8 dbuf[32];
    buf->clear();
    switch (type)
    {
    case LUA_TNIL:
        buf->append("nil");
        break;
    case LUA_TNUMBER:
        sprintf(dbuf, "%d", lua_tointeger(L, i));
        buf->append(dbuf);
        break;
    case LUA_TBOOLEAN:
        sprintf(dbuf, "%s", (lua_toboolean(L, i) == 0) ? "false" : "true");
        buf->append(dbuf);
        break;
    case LUA_TSTRING:
        buf->append(lua_tostring(L, i));
        break;
    default:
        buf->append("[?]");
        break;
    }
}

int system_dbglog(lua_State *L)
{
    u8string msg;
    for (int i=1,e=lua_gettop(L); i<=e; ++i)
    {
        formatByType(L, i, &msg);
    }
    OutputDebugString(TU2W(msg.c_str()));    
    return 0;
}

static const luaL_Reg system_methods[] =
{
    { "dbgstack", system_debugstack},
    { "dbgtable", system_dbgtable },
    { "dbglog", system_dbglog },
    { "msgbox", system_messagebox },
    { NULL, NULL }
};

int luaopen_system(lua_State *L)
{
    luaL_newlib(L, system_methods);
    return 1;
}
