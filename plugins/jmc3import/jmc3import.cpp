#include "stdafx.h"
#include "jmc3ImportImpl.h"

int get_name(lua_State *L)
{
    lua_pushstring(L, "������ �� JMC3");
    return 1;
}

int get_description(lua_State *L)
{
    lua_pushstring(L, "��������� ������������� ��������, ������ � ������ ������� ���������\r\n�� ���������������� ������ Jaba Mud Client 3.x");
    return 1;
}

int get_version(lua_State *L)
{
    lua_pushstring(L, "1.0");
    return 1;
}

int init(lua_State *L)
{
    luaT_run(L, "addMenu", "sdd", "�������/������ �� JMC3...", 1, 2);
    return 0;
}

int menucmd(lua_State *L)
{
    if (!luaT_check(L, 1, LUA_TNUMBER))
        return 0;
    int menuid = lua_tointeger(L, 1);
    lua_pop(L, 1);
    if (menuid == 1)
    {
        luaT_run(L, "getParent", "");
        HWND parent = (HWND)lua_tounsigned(L, -1);
        lua_pop(L, 1);

        std::vector<u8string> errors;
        Jmc3Import jmc3(L);
        if (jmc3.import(parent, L, &errors))
        {
            if (!errors.empty())
            {
                luaT_log(L, "������ ������� �� JMC3 (��������� / ��� ���� ����� �������):");
                for (int i=0,e=errors.size(); i<e; ++i)
                    luaT_log(L, errors[i].c_str() );
            }
            else
            {
                luaT_log(L, "������ �� JMC3 ������ �������.");
            }
        }        
    }
    return 0;
}

static const luaL_Reg jmc3_methods[] =
{
    { "name", get_name },
    { "description", get_description },
    { "version", get_version },
    { "init", init },
    { "menucmd", menucmd },
    { NULL, NULL }
};

int WINAPI plugin_open(lua_State *L)
{
    luaL_newlib(L, jmc3_methods);
    lua_setglobal(L, "jmc3import");
    return 0;
}