#include "stdafx.h"
#include "mapperObjects.h"
#include "mapperObjects2.h"

const wchar_t* RoomDirName[] = { L"north", L"south", L"west", L"east", L"up", L"down" };
RoomDir RoomDirHelper::revertDir(RoomDir dir)
{
    if (dir == RD_NORTH)
        return RD_SOUTH;
    if (dir == RD_SOUTH)
        return RD_NORTH;
    if (dir == RD_WEST)
        return RD_EAST;
    if (dir == RD_EAST)
        return RD_WEST;
    if (dir == RD_UP)
        return RD_DOWN;
    if (dir == RD_DOWN)
        return RD_UP;
    assert(false);
    return RD_ERROR;
}
const wchar_t* RoomDirHelper::getDirName(RoomDir dir)
{
    int index = static_cast<int>(dir);
    if (index >= 0 && index <= 5)
        return RoomDirName[index];
    return NULL;
}
RoomDir RoomDirHelper::getDirByName(const wchar_t* dirname)
{
    tstring name(dirname);
    for (int index=0;index<=5;++index)
    {
        if (!name.compare(RoomDirName[index]))
            return static_cast<RoomDir>(index);    
    }
    return RD_ERROR;
}

RoomCursor::RoomCursor(Room* current_room) : 
m_current_room(current_room), x(0), y(0), level(0)
{
}

Room* RoomCursor::getRoom(RoomDir dir)
{
    if (!move(dir))
        return NULL;
    Zone *zone = m_current_room->level->getZone();
    RoomsLevel *rl = zone->getLevel(level, false);
    if (!rl)
        return NULL;
    return rl->getRoom(x, y);
}

bool RoomCursor::addRoom(RoomDir dir, Room* room)
{
    if (!move(dir))
        return false;
    Zone *zone = m_current_room->level->getZone();
    RoomsLevel *rl = zone->getLevel(level, true);
    return rl->addRoom(room, x, y);
}

bool RoomCursor::move(RoomDir dir)
{
    x = m_current_room->x;
    y = m_current_room->y;
    level = m_current_room->level->getLevel();
    switch (dir)
    {
    case RD_NORTH: y -= 1; break;
    case RD_SOUTH: y += 1; break;
    case RD_WEST:  x -= 1; break;
    case RD_EAST:  x += 1; break;
    case RD_UP: level += 1; break;
    case RD_DOWN: level -= 1; break;
    default:
        assert(false);
        return false;
    }
    return true;
}