#pragma once
#include "mapperObjects2.h"

struct Room;
class RoomsLevel;
class Zone;

typedef tstring RoomVnum;
struct RoomData
{
    RoomVnum vnum;
    tstring name;
    tstring descr;
    tstring exits;
    tstring zonename;   // if known

    //todo! uint    hash;
    //uint    dhash;

    /*bool equal(const RoomData& rd) const
    {
        assert(hash && rd.hash);
        return (hash == rd.hash && dhash == rd.dhash) ? true : false;
    }

    bool similar(const RoomData& rd) const
    {
        assert(hash && rd.hash);
        return (hash == rd.hash) ? true : false;
    }

    bool compare(const RoomData& rd) const
    {
        if (name != rd.name)
            return false;
        if (exits != rd.exits)
            return false;
        if (descr != rd.descr)
            return false;        
        return true;
    }
    
    void calcHash()
    {
        CRC32 crc;
        crc.process(name.c_str(), name.length() * sizeof(tchar));        
        crc.process(exits.c_str(), exits.length() * sizeof(tchar));
        hash = crc.getCRC32();
        dhash = 0;
        if (!descr.empty()) 
        {
            CRC32 dcrc;
            dcrc.process(descr.c_str(), descr.length() * sizeof(WCHAR));
            dhash = dcrc.getCRC32();
        }
    }
    RoomData() : hash(0), dhash(0) {}*/
};

struct RoomExit
{
    RoomExit() : next_room(NULL), exist(false), door(false) {}
    Room *next_room;
    bool exist;
    bool door;
};

struct Room
{
    Room() : level(NULL), x(0), y(0), icon(0), use_color(0), color(0) {} //, special(0) {}
    RoomData roomdata;              // room key data
    RoomExit dirs[ROOM_DIRS_COUNT]; // room exits
    RoomsLevel *level;              // parent level (owner of room)
    int x, y;                       // position in level
    int icon;                       // icon if exist
    int use_color;                  // flag for use background color
    COLORREF color;                 // background color
    //int special;                    // special internal value for algoritms (don't save)
};

enum ViewCursorColor { RCC_NORMAL = 0, RCC_LOST };
struct ViewMapPosition
{
    ViewMapPosition() : room(NULL), cursor(RCC_NORMAL) {}
    Room* room;
    ViewCursorColor cursor;
};

struct RoomsLevelBox
{
    RoomsLevelBox() : left(0), right(0), top(0), bottom(0) {}
    int left;
    int right;
    int top;
    int bottom;
};

class RoomsLevel
{
    friend class Zone;
public:
    RoomsLevel(Zone* parent_zone, int level_floor) : zone(parent_zone), level(level_floor), m_invalidBoundingBox(true), m_changed(false)
    {
        rooms.push_back(new row);        
    }
    ~RoomsLevel() { std::for_each(rooms.begin(), rooms.end(), [](row *r){delete r;}); }
    bool  addRoom(Room* r, int x, int y);
    Room* detachRoom(int x, int y);
    void  deleteRoom(int x, int y);
    Room* getRoom(int x, int y);
    Zone* getZone() const { return zone; }
    int   getLevel() const { return level; }
    int   width() const;
    int   height() const;    
    const RoomsLevelBox& box();
    bool  isChanged() const { return m_changed; }
    bool  isEmpty() const;

private:
    void calcBoundingBox();
    void resizeLevel(int x, int y);
    bool checkCoords(int x, int y) const;    
    struct row {
    row() : left(-1), right(-1) {}
    ~row() { std::for_each(rr.begin(), rr.end(), [](Room* r) { delete r;}); }
    void recalc_leftright() {
    left = -1; right = -1;
    for (int i = 0, e = rr.size(); i < e; ++i)
    {
        if (!rr[i]) continue;
        if (left == -1) left = i;
        right = i;
    }}
    std::vector<Room*> rr;
    int left, right;
    };
    std::vector<row*> rooms;
    Zone* zone;
    int level;
    RoomsLevelBox m_box;
    bool m_invalidBoundingBox;
    bool m_changed;
};

struct ZoneParams
{
    ZoneParams() : minl(0), maxl(0), empty(true) {}
    int minl;
    int maxl;
    bool empty;
    tstring original_name;
};

class Zone
{
public:
    Zone(const tstring& zonename) : start_index(0), m_original_name(zonename) {}
    ~Zone() { std::for_each(m_levels.begin(), m_levels.end(), [](RoomsLevel* rl) {delete rl;}); }
    RoomsLevel* getLevel(int level, bool create_if_notexist);
    RoomsLevel* getDefaultLevel();
    int width() const;
    int height() const;
    void getParams(ZoneParams* params) const;
    void resizeLevels(int x, int y);
    bool isChanged() const;
private:
    std::vector<RoomsLevel*> m_levels;
    int start_index;
    tstring m_original_name;
};
