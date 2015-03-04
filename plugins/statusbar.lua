﻿-- statusbar
-- Плагин для Tortilla mud client

statusbar = {}
function statusbar.name() 
    return 'Гистограммы здоровья и энергии и др.'
end

function statusbar.description()
return 'Плагин отображает информацию о здоровье, энергии и других параметрах\r\n\z
в виде полосок на отдельной панели клиента.'
end

function statusbar.version()
    return '1.0'
end

local objs = {}
local log = system.dbglog
local r, regexp, regexp2, values, cfg

function statusbar.render()
  if not cfg then
    return statusbar.print('Нет файла настроек')
  end
  --r:select(objs.pen1)
  --r:select(objs.brush1)
  --r:rect{left = 10, right = 30, top = 10, bottom = 30}
  --r:rect{60, 10, 90, 27}
  --r:solidrect{10, 4, 160, 26}
  r:select(objs.font1)
  local msg
  if regexp2 and values.hp and values.mv then
    if values.maxhp and values.maxmv then
        msg = values.hp..'/'..values.maxhp..'HP '..values.mv..'/'..values.maxmv..'MV'
    else
        msg = "Выполните команду 'счет' для настройки плагина."
    end
  end
  if not regexp2 and values.maxhp and values.maxmv and values.hp and values.mv then
    msg = values.hp..'/'..values.maxhp..' HP '..values.mv..'/'..values.maxmv..' MV'
  end
  statusbar.print(msg)
end

function statusbar.print(msg)
  if msg then 
    r:print(10, 10, msg)
  end
end

function statusbar.before(window, v)
if window ~= 0 or not cfg then return end
for i=1,v:size() do
  v:select(i)
  if v:isprompt() then
    if regexp:findall(v:getprompt()) then
    values.hp = regexp:get(cfg.hp-1)
    values.mv = regexp:get(cfg.mv-1)
    if not regexp2 then
      values.maxhp = regexp:get(cfg.maxhp-1)
      values.maxmv = regexp:get(cfg.maxmv-1)
    end
    r:update()
    end
  end
end
if regexp2 and v:find(regexp2) then
  values.maxhp = regexp2:get(cfg.maxhp)
  values.maxmv = regexp2:get(cfg.maxmv)
  log('maxhp='..values.maxhp..',maxmv='..values.maxmv..'\r\n')  
end
end

function statusbar.init()
  local p = createPanel("bottom", 28)
  r = p:setrender(statusbar.render)
  r:setbackground(0,0,0)
  r:textcolor(192,192,192)
  --objs.pen1 = r:createpen{ style ="solid", width = 1, r = 0, g = 0, b = 120 }
  --objs.brush1 = r:createbrush{ style ="solid", r = 200, g = 0, b = 200 }
  --objs.font1 = r:createfont{ font="fixedsys", height = 11, bold = 0 }
  objs.font1 = r:defaultfont()
  regexp = createPcre("[0-9]+")
  local file = loadTable('config.xml')
  if not file then
    cfg = nil
    error("ошибка") --todo
    return
  end
  cfg = file.config
  values = {}
  regexp2 = cfg.regexp and createPcre(cfg.regexp) or nil
  log(cfg.regexp)
end
