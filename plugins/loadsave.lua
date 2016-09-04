﻿-- loadsave
-- Плагин Tortilla mud client

local loadsave = {}
function loadsave.name() 
  return 'Тесты loadTable, saveTable'
end
function loadsave.description()
  return 'Плагин используется для автотестирования функций loadTable, saveTable.'
end
function loadsave.version()
  return '-'
end

local function assert(res, text)
  if not res then
    print("unit test faled: "..text)
  end
end

function loadsave.init()
  log("Автотесты для loadTable, saveTable")
  local t = {
    mode = true,
    a = { 'проверка' },
    b = { 'проверка2', p = 4, b = false },
    c = { 'сложная проверка', d = { 'вложенная таблица' } }
  }
  saveTable(t, 'unittests.lua')
  local t2 = loadTable('unittests.lua')
  
  assert(t.mode == t2.mode, 't.mode == t2.mode')
  assert(t.a[1] == t2.a[1], 't.a[1] == t2.a[1]')
  assert(t.a[2] == t2.a[2], 't.a[2] == t2.a[2]')
  assert(t.b[1] == t2.b[1], 't.b[1] == t2.b[1]')
  assert(t.c[1] == t2.c[1], 't.c[1] == t2.c[1]')
  assert(t2.c.d[1] == 'вложенная таблица', 't2.c.d[1] == "вложенная таблица"')
  assert(t2.b.p == 4, 't2.b.p == 4')
  assert(t2.b.b == false, 't2.b.b == false')
end

return loadsave
