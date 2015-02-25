#pragma once

class RoomHelper
{
public:
    RoomHelper(Room *room) : m_room(room) {}
   /* bool isExplored(RoomDir dir)
    {
        RoomExit &re = m_room->dirs[dir];

        /*Room *r = m_room->dirs[dir].next_room;
        if (r && samezone(m_room, r))
            return true;
        return false;
    }*/

    /*bool isCycled()
    {
        bool result = false;

        std::vector<Room*> rooms;
        m_room->special = 1;
        rooms.push_back(m_room);
        for (size_t i = 0; i < rooms.size(); ++i)
        {
            Room *r = rooms[i];
            for (int j = RD_NORTH; j <= RD_DOWN; ++j)
            {
               Room *next = r->dirs[j].next_room;
               if (!next || !samezone(r, next))
                   continue;

               if (next->special == 0)
               {
                   next->special = r->special + 1;
                   rooms.push_back(next);
                   continue;
               }
               if (r->special != next->special + 1)
               {
                   result = true;
                   break;
               }
            }
            if (result) break;
        }
        for (int i = 0, e = rooms.size(); i < e; ++i)
            rooms[i]->special = 0;
        return result;
    }

    // check isCycle first !!!
    void getSubZone(RoomDir dir, std::vector<Room*> *subzone)
    {
        Room *next = m_room->dirs[dir].next_room;
        if (!next) 
            return;
        m_room->special = 1;
        next->special = 2;

        std::vector<Room*> rooms;
        rooms.push_back(next);
        for (size_t i = 0; i < rooms.size(); ++i)
        {
            Room *r = rooms[i];
            for (int j = RD_NORTH; j <= RD_DOWN; ++j)
            {
                Room *next = r->dirs[j].next_room;
                if (!next || !samezone(r, next))
                    continue;

                if (next->special == 0)
                {
                    next->special = r->special + 1;
                    rooms.push_back(next);
                    continue;
                }
            }
        } 

        for (int i = 0, e = rooms.size(); i < e; ++i)
            rooms[i]->special = 0;
        m_room->special = 0;
        subzone->swap(rooms);
    }

    bool isSameZone(Room *another_room) const
    {
        return samezone(m_room, another_room);
    }

private:
    bool samezone(Room *r1, Room *r2) const
    {
        RoomsArea *a1 = r1->level->getArea();
        RoomsArea *a2 = r2->level->getArea();
        return (a1->getZone() == a2->getZone()) ? true : false;
    }*/
    Room* m_room;
};
