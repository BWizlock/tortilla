﻿-- inveq
-- Плагин для Tortilla mud client

local text_color = 6

inveq = {}
function inveq.name() 
    return 'Инвентарь и экипировка'
end

function inveq.description()
return 'Плагин отображает экипировку, которая одета на персонаже и инвентарь\r\n\z
персонажа в отдельном окне.'
end

function inveq.version()
    return '1.0'
end

--[[function statusbar.before(window, v)
if window ~= 0 then return end
end]]

local r
function inveq.render()
  r:print(4, 4, 'Экипировка:')
end

function inveq.init()
  --local p = createWindow("right", 256, 400)
  --p:dock("right")
  --[[local p = createPanel("right", 300)
  r = p:setRender(inveq.render)
  r:setBackground(props.backgroundColor())
  r:textColor(props.paletteColor(text_color))
  r:select(props.currentFont())]]


  createTrigger("Вы хотите %1", inveq.event)
end

function inveq.event(s)
  log("Сработал триггер")
end
