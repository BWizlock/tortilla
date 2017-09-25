#include "stdafx.h"
#include "mapper.h"
#include "roomObjects.h"
#include "debugHelpers.h"
//#include "roomMergeTool.h"
#include "newZoneNameDlg.h"
#include "mapTools.h"
#include "mapSmartTools.h"

Mapper::Mapper(PropertiesMapper *props, const tstring& mapsFolder) : m_propsData(props), 
m_lastDir(RD_UNKNOWN), m_pCurrentRoom(NULL), m_mapsFolder(mapsFolder)
{
}

Mapper::~Mapper()
{
}

void Mapper::processNetworkData(const tchar* text, int text_len)
{
    bool in_dark = false;
    RoomData room;
    if (!m_processor.processNetworkData(text, text_len, &room))
    {
        if (m_prompt.processNetworkData(text, text_len))
        {
            popDir();
            if (m_dark.processNetworkData(text, text_len) && m_pCurrentRoom)
            {
                // move in dark to direction
                Room *next = m_pCurrentRoom->dirs[m_lastDir].next_room;
                //if (next)
                {
                    m_pCurrentRoom = next;
                    MapTools t(&m_map);
                    MapCursor cursor = t.createCursor(m_pCurrentRoom, RCC_LOST);
                    redrawPosition(cursor);
                }
            }            
        }
        return;
    }
    else
    {
        if (m_dark.processNetworkData(text, text_len))
            in_dark = true;
    }
    popDir();

    DEBUGOUT(L"------");
    DEBUGOUT(room.name);
    DEBUGOUT(room.vnum);
    DEBUGOUT(room.exits);
    DEBUGOUT2(L"lastdir: ", RoomDirHelper().getDirName(m_lastDir));

    MapTools t(&m_map);
    Room *new_room = t.findRoom(room.vnum);
    if (!new_room)
    {
        new_room = new Room();
        new_room->roomdata = room;
        setExits(new_room);
        if (!m_pCurrentRoom)
        {
            if (!t.createNewZone(new_room))
            {
                delete new_room;
                new_room = NULL;
            }
        }
        else
        {
            if (!t.addNewRoom(m_pCurrentRoom, new_room, m_lastDir))
            {
                delete new_room;
                new_room = NULL;
            }
        }
    }
    else
    {
         setExits(new_room);
         if (m_lastDir != RD_UNKNOWN && m_pCurrentRoom) {
            MapSmartTools st;
            st.addLink(m_pCurrentRoom, new_room, m_lastDir);
         }
    }
    m_pCurrentRoom = new_room;
    MapCursor cursor = t.createCursor(m_pCurrentRoom, RCC_NORMAL);
    redrawPosition(cursor);
}

void Mapper::processCmd(const tstring& cmd)
{
    RoomDir dir = RD_UNKNOWN;
    for (int i = 0, e = m_dirs.size(); i < e; ++i)
    {
        dir = m_dirs[i].check(cmd);
        if (dir != RD_UNKNOWN) { m_path.push_back(dir); break; }
    }

#ifdef _DEBUG
    tstring d;
    switch(dir) {
    case RD_NORTH: d.append(L"�"); break;
    case RD_SOUTH: d.append(L"�"); break;
    case RD_WEST:  d.append(L"�"); break;
    case RD_EAST:  d.append(L"�"); break;
    case RD_UP:    d.append(L"��"); break;
    case RD_DOWN:  d.append(L"��"); break;
    }
    if (!d.empty())
    {
        tstring t(L"dir:");
        t.append(d);
        DEBUGOUT(t);
    }
#endif
}

void Mapper::popDir()
{
    if (m_path.empty())
        m_lastDir = RD_UNKNOWN;
    else {
        m_lastDir = *m_path.begin();
        m_path.pop_front();
    }
}

void Mapper::checkExit(Room *room, RoomDir dir, const tstring& exit)
{
    const tstring& e = room->roomdata.exits; 
    if (e.find(exit) != -1) 
    {
        room->dirs[dir].exist = true;
        tstring door(L"(");
        door.append(exit);
        door.append(L")");
        if (e.find(door) != -1)
            room->dirs[dir].door = true;
    }
}

void Mapper::setExits(Room *room)
{
    // parse room->roomdata.exits to room->dirs
    checkExit(room,  RD_NORTH, m_propsData->north_exit);
    checkExit(room,  RD_SOUTH, m_propsData->south_exit);
    checkExit(room,  RD_WEST, m_propsData->west_exit);
    checkExit(room,  RD_EAST, m_propsData->east_exit);
    checkExit(room,  RD_UP, m_propsData->up_exit);
    checkExit(room,  RD_DOWN, m_propsData->down_exit);
}

void Mapper::updateProps()
{
    m_processor.updateProps(m_propsData);
    m_prompt.updateProps(m_propsData);
    m_dark.updateProps(m_propsData);
    InitDirVector h;
    h.make(RD_NORTH, m_propsData->north_cmd, m_dirs);
    h.make(RD_SOUTH, m_propsData->south_cmd, m_dirs);
    h.make(RD_WEST, m_propsData->west_cmd, m_dirs);
    h.make(RD_EAST, m_propsData->east_cmd, m_dirs);
    h.make(RD_UP, m_propsData->up_cmd, m_dirs);
    h.make(RD_DOWN, m_propsData->down_cmd, m_dirs);
}

void Mapper::saveProps()
{
    m_propsData->zoneslist_width = m_vSplitter.GetSplitterPos();
	int zone = m_zones_control.getCurrentZone();
	m_propsData->current_zone.clear();
	if (zone >= 0) {
		m_propsData->current_zone = m_zones_control.getZoneName(zone);
	}
	else {
		m_propsData->current_zone = L"";
	}
}

void Mapper::saveMaps()
{
    const tstring&dir = m_mapsFolder;
	m_map.saveMaps(dir);
}

void Mapper::loadMaps()
{
    m_view.clear();
    m_zones_control.deleteAllZones();

    const tstring&dir = m_mapsFolder;
    bool last_found = false;
    tstring &last = m_propsData->current_zone;
    
    m_map.loadMaps(dir);

    Rooms3dCubeList zones;
	m_map.getZones(&zones);
    m_zones_control.updateList(zones);

	for (Rooms3dCube* zone : zones)
	{
        if (!last.empty() && last == zone->name()) {
            MapTools t(&m_map);
            MapCursor c = t.createZoneCursor(zone);
            redrawPosition(c);
            last_found = true;
            break;
        }
	}
    if (!last_found && !zones.empty()) {
        MapTools t(&m_map);
        MapCursor c = t.createZoneCursor(zones[0]);
        redrawPosition(c);
    }   
}

void Mapper::redrawPosition(MapCursor cursor)
{
    m_view.showPosition(cursor);
    const Rooms3dCube* zone = cursor->zone();
    m_zones_control.setCurrentZone(zone);
}

void Mapper::onCreate()
{
    DWORD style = WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;

    RECT rc;
    GetClientRect(&rc);
    m_vSplitter.Create(m_hWnd, rc);
    m_vSplitter.m_cxySplitBar = 3;

    RECT pane_left, pane_right;
    m_vSplitter.GetSplitterPaneRect(0, &pane_left); pane_left.right -= 3;
    m_vSplitter.GetSplitterPaneRect(1, &pane_right);

    m_zones_control.Create(m_vSplitter, pane_left, style);
    m_container.Create(m_vSplitter, pane_right, L"", style);

    m_toolbar.Create(m_container, rcDefault, style);
    m_toolbar.setControlWindow(m_hWnd, WM_USER+1);
    m_view.Create(m_container, rcDefault, NULL, style | WS_VSCROLL | WS_HSCROLL, WS_EX_STATICEDGE);
    m_view.setMenuHandler(m_hWnd);

    m_container.attach(30, m_toolbar, m_view);
    m_vSplitter.SetSplitterPanes(m_zones_control, m_container);

    m_zones_control.setNotifications(m_hWnd, WM_USER);
    m_vSplitter.SetSplitterRect();

    if (m_propsData->zoneslist_width > 0)
        m_vSplitter.SetSplitterPos(m_propsData->zoneslist_width);
    else
        m_vSplitter.SetDefaultSplitterPos();
    updateProps();
}

void Mapper::onSize()
{
    RECT rc;
    GetClientRect(&rc);
    m_vSplitter.MoveWindow(&rc, FALSE);
    m_vSplitter.SetSplitterRect();
}

void Mapper::onZoneChanged()
{
    MapCursor current = m_view.getCurrentPosition();
    int zone = m_zones_control.getCurrentZone();
    if (current->valid() && current->pos().zid == zone)
    {
        return redrawPosition(current);
    }
    MapTools t(&m_map);
    Rooms3dCube *ptr = m_map.findZone(zone);
    MapCursor cursor = t.createZoneCursor(ptr);
    redrawPosition(cursor);
}

void Mapper::onRenderContextMenu(int id)
{
    const Room* room = m_view.menuTrackedRoom();
    if (!room) {
        assert(false);
        return;
    }
    RoomDirHelper dh;
    if (id >= MENU_NEWZONE_NORTH && id <= MENU_NEWZONE_DOWN)
    {
        MapNewZoneTool t(&m_map);
        RoomDir dir = dh.cast(id - MENU_NEWZONE_NORTH);
        bool result = t.tryMakeNewZone(room, dir);
        if (!result)
        {
            MessageBox(L"���������� ������� ����� ���� ��-�� ����������� ��������� �� ������ �������!", L"������", MB_OK | MB_ICONERROR);
            return;
        }
#ifdef _DEBUG
        m_view.Invalidate();
#endif
        NewZoneNameDlg dlg;
        if (dlg.DoModal() == IDCANCEL)
            return;    	
        result = t.applyMakeNewZone(dlg.getName());
        if (!result) {
            assert(false);
            return;
        }
        Rooms3dCubeList zones;
        m_map.getZones(&zones);
        m_zones_control.updateList(zones);
        m_view.Invalidate();
    }

    if (id >= MENU_JOINZONE_NORTH && id <= MENU_JOINZONE_DOWN)
    {
       /* RoomMergeTool t(m_map.getZone(room));
        RoomDir dir = dh.cast(id - MENU_JOINZONE_NORTH);
        bool result = t.tryMergeZones(room, dir);
        if (!result)
        {
            MessageBox(L"������������ ���������� ���� � ����!", L"������", MB_OK | MB_ICONERROR);
            return;
        }*/
    }

    if (id >= MENU_MOVEROOM_NORTH && id <= MENU_MOVEROOM_DOWN)
    {
        /*RoomMergeTool t(m_map.getZone(room));
        RoomDir dir = dh.cast(id - MENU_MOVEROOM_NORTH);
        bool result = t.tryJoinRoom(room, dir);
        if (!result)
        {
            MessageBox(L"������������ ����������� ������� � ������� ����!", L"������", MB_OK | MB_ICONERROR);
            return;
        }*/

    }
}

void Mapper::onToolbar(int id)
{
    if (id == IDC_BUTTON_SAVEZONES) {        
        saveMaps();
    }

    if (id == IDC_BUTTON_LOADZONES) {
        loadMaps();
    }

    if (id == IDC_BUTTON_CLEARZONES) {
        m_view.clear();
        m_map.clearMaps();
        m_zones_control.deleteAllZones();
        MapCursor cursor = m_view.getCurrentPosition();
        redrawPosition(cursor);
    }

    if (id == IDC_BUTTON_LEVEL_DOWN) {        
        MapCursor c = m_view.getCurrentPosition();
        if (!c->valid())
            return;
        if (c->move(RD_DOWN))
            redrawPosition(c);
    }

    if (id == IDC_BUTTON_LEVEL_UP) {        
        MapCursor c = m_view.getCurrentPosition();
        if (!c->valid())
            return;
        if (c->move(RD_UP))
            redrawPosition(c);
    }
}