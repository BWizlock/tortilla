#include "stdafx.h"
#include "mudGameView.h"

bool MudGameView::initialize()
{
    if (!initPluginsSystem())
    {
        msgBox(m_hWnd, IDS_ERROR_SCRIPTENGINE_FAILED, MB_OK | MB_ICONSTOP);
        return false;
    }

#ifdef _DEBUG
    InputCommandTemplateUnitTests::run();
    ParamsHelperUnitTests::run();
    CompareObjectUnitTests::run();
#endif

    if (!m_processor.init())
    {
        msgBox(m_hWnd, IDS_ERROR_SCRIPTENGINE_FAILED, MB_OK|MB_ICONSTOP);
        return false;
    }

    if (!m_manager.init())
    {
        msgBox(m_hWnd, IDS_ERROR_INITPROFILES_FAILED, MB_OK|MB_ICONSTOP);
        return false;
    }

    if (!m_manager.loadProfile())
    {
        if (msgBox(m_hWnd, IDS_ERROR_LASTLOAD_FAILED, MB_YESNO|MB_ICONSTOP) != IDYES)
            return false;
        Profile p(m_manager.getProfile());
        p.name = default_profile_name;
        if (!m_manager.loadProfile(p))
        {
            if (!m_manager.createEmptyProfile(p))
            {
                msgBox(m_hWnd, IDS_ERROR_NEWPROFILE_FAILED, MB_OK|MB_ICONSTOP);
                return false;
            }
        }
    }
    return true;
}

void MudGameView::onStart()
{
    if (!loadModules())
        msgBox(m_hWnd, IDS_ERROR_MODULES, MB_OK | MB_ICONSTOP);
   updateProps();
   loadPlugins();
}

void MudGameView::onClose()
{
    m_manager.saveProfile();
}

void MudGameView::onNewProfile()
{
    NewProfileDlg dlg;
    dlg.loadProfiles(m_manager.getProfileGroup());
    if (dlg.DoModal() == IDOK)
    {
        CopyProfileData data;
        dlg.getProfile(&data);

        unloadPlugins();

        Profile current(m_manager.getProfile());
        bool successed = true;
        if (data.src.name.empty())
        {
            if (!m_manager.createEmptyProfile(data.dst)) {
                msgBox(m_hWnd, IDS_ERROR_NEWPROFILE_FAILED, MB_OK|MB_ICONSTOP); successed = false;
            }
        }
        else
        {
            if (!m_manager.copyProfile(data.src, data.dst)) {
                msgBox(m_hWnd, IDS_ERROR_COPYPROFILE_FAILED, MB_OK|MB_ICONSTOP); successed = false;
            }
        }
        if (!successed)
            m_manager.loadProfile(current);

        updateProps();
        loadClientWindowPos();
        loadPlugins();
        m_bar.reset();
    }
}

void MudGameView::loadProfile(const tstring& name, const tstring& group, tstring* error)
{
    assert(error);
    Profile current(m_manager.getProfile());
    if ((current.group == group || group.empty()) && current.name == name)
    {
        error->assign(L"������� ��������� ������� �������.");
        return;
    }
    Profile profile;
    profile.name = name;
    profile.group = current.group;
    if (!group.empty())
        profile.group = group;
    if (!m_manager.checkProfile(profile))
    {
        error->assign(L"��� ������ �������.");
        return;
    }
    saveClientWindowPos();
    savePluginWindowPos();
    unloadPlugins();
    if (!m_manager.saveProfile())
    {
        error->assign(L"�� ���������� ��������� ������� �������, ����� ��������� �����.");
        loadClientWindowPos();
        loadPlugins();
        return;
    }
    if (!m_manager.loadProfile(profile))
    {
        error->assign(L"�� ���������� ��������� �������.");
        m_manager.loadProfile(current);
    }
    updateProps();
    loadClientWindowPos();
    loadPlugins();
    m_bar.reset();
}

void MudGameView::onLoadProfile()
{
    LoadProfileDlg dlg;
    if (dlg.DoModal() == IDOK)
    {
        Profile profile;
        dlg.getProfile(&profile);
        if (profile.name.empty())
            return;

        Profile current(m_manager.getProfile());
        if (current.group == profile.group && current.name == profile.name)
            return;

        saveClientWindowPos();
        savePluginWindowPos();
        unloadPlugins();
        if (!m_manager.saveProfile())
        {
            msgBox(m_hWnd, IDS_ERROR_CURRENTSAVEPROFILE_FAILED, MB_OK|MB_ICONSTOP);
            loadClientWindowPos();
            loadPlugins();
            return;
        }
        if (!m_manager.loadProfile(profile))
        {
            msgBox(m_hWnd, IDS_ERROR_LOADPROFILE_FAILED, MB_OK|MB_ICONSTOP);
            m_manager.loadProfile(current);
        }
        updateProps();
        loadClientWindowPos();
        loadPlugins();
        m_bar.reset();
    }
}

void MudGameView::onNewWorld()
{
    NewWorldDlg dlg;
    if (dlg.DoModal() == IDOK)
    {
        CopyProfileData data;
        dlg.getData(&data);

        saveClientWindowPos();
        savePluginWindowPos();
        unloadPlugins();
        if (!m_manager.saveProfile())
        {
            msgBox(m_hWnd, IDS_ERROR_CURRENTSAVEPROFILE_FAILED, MB_OK | MB_ICONSTOP);
            loadPlugins();
            loadClientWindowPos();
            return;
        }

        Profile current(m_manager.getProfile());
        bool successed = true;
        if (!data.src.group.empty())
        {
            if (!m_manager.copyProfile(data.src, data.dst)) {
                msgBox(m_hWnd, IDS_ERROR_COPYPROFILE_FAILED, MB_OK | MB_ICONSTOP); successed = false;
            }
        }
        else
        {
            if (!m_manager.createEmptyProfile(data.dst)) {
                msgBox(m_hWnd, IDS_ERROR_NEWPROFILE_FAILED, MB_OK | MB_ICONSTOP); successed = false;
            }
        }
        if (!successed)
            m_manager.loadProfile(current);

        updateProps();
        loadClientWindowPos();
        loadPlugins();
        m_bar.reset();
    }
}

void MudGameView::loadPlugins()
{
    m_plugins.loadPlugins(m_manager.getProfileGroup(), m_manager.getProfileName());
}

void MudGameView::unloadPlugins()
{
    m_plugins.unloadPlugins();
}

void MudGameView::preprocessCommand(InputCommand cmd)
{
    m_plugins.processGameCmd(cmd);
}

void MudGameView::setOscColor(int index, COLORREF color)
{
    if (m_propData->disable_osc)
        return;
    m_propData->osc_colors[index] = color;
    m_propData->osc_flags[index] = 1;
    m_propElements.palette.setColor(index, color);
}

void MudGameView::resetOscColors()
{
    if (m_propData->disable_osc)
        return;
    m_propData->resetOSCColors();
    m_propElements.palette.updateProps(m_propData);
}

MudViewHandler* MudGameView::getHandler(int view)
{
    if (view >= 0 && view <= OUTPUT_WINDOWS)
        return m_handlers[view];
    return NULL;
}

void MudGameView::findText()
{
    tstring text;
    m_find_dlg.getTextToSearch(&text);
    if (text.empty())
        return;
    int view = m_find_dlg.getSelectedWindow();
    bool shift = (GetKeyState(VK_SHIFT) < 0);
    int find_direction = m_find_dlg.getDirection() * ((shift) ? -1 : 1);

    MudView *v = (view == 0) ? &m_history : m_views[view - 1];
    int current_find = v->getCurrentFindString();
    int new_find = v->findAndSelectText(current_find, find_direction, text);
    if (new_find == -1)
    {
       // not found
       if (current_find == -1)
          return;
    }
    // found / not found with last found
    // clear find in last find window (if it another window)
    if (m_last_find_view != view && m_last_find_view != -1)
    {
        MudView *lf = (m_last_find_view == 0) ? &m_history : m_views[m_last_find_view - 1];
        lf->clearFind();
        m_last_find_view = -1;
        if (new_find == -1)
            return;
    }
    if (new_find == -1)
        new_find = current_find;

    if (view == 0 && !m_history.IsWindowVisible())
        showHistory(new_find, 0);

    int count = v->getStringsCount();
    int delta = v->getStringsOnDisplay() / 2;  // center on the screen
    int center_vs = new_find + delta;          // ������� ��������� ������ ��������� �� ������
    if (center_vs < count)
        new_find = center_vs;
    else
        new_find = count-1;
    v->setViewString(new_find);
    m_last_find_view = view;
}
