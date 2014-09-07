#include "stdafx.h"
#include "base.h"

int module_test(lua_State *L)
{
    lua_pushnumber(L, 5);
    return 1;
}

// ������� �������, � ������� t.test(), ������� ���������� �������� 5
// ��� t - �������, ������� ��������� �� ����� ��� ������ �� ������
// � ����, ��� ����������� ������, ������ ���� ��� ����:
// [local] module = require("module")
// ����� ������� t ����� �������� � ��������� module.

int luaopen_module(lua_State *L)
{
    lua_newtable(L);
    lua_pushstring(L, "test");
    lua_pushcfunction(L, module_test);
    lua_settable(L, -3);    
    return 1;
}
