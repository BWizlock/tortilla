#pragma once

class PropertyTabwords :  public CDialogImpl<PropertyTabwords>
{
    PropertiesList *propValues;
    PropertiesList m_list_values;
    PropertyListCtrl m_list;
    CBevelLine m_bl1;
    CBevelLine m_bl2;
    CEdit m_pattern;
    CButton m_add;
    CButton m_del;
    bool m_update_mode;

public:
     enum { IDD = IDD_PROPERTY_TABWORDS };
     PropertyTabwords() : m_update_mode(false) {}
     void setParams(PropertiesList *values)
     {
         propValues = values;
     }

private:
    BEGIN_MSG_MAP(PropertyTabwords)
       MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
       MESSAGE_HANDLER(WM_DESTROY, OnCloseDialog)
       MESSAGE_HANDLER(WM_SHOWWINDOW, OnShowWindow)
       COMMAND_ID_HANDLER(IDC_BUTTON_ADD, OnAddElement)
       COMMAND_ID_HANDLER(IDC_BUTTON_DEL, OnDeleteElement)
       COMMAND_HANDLER(IDC_EDIT_PATTERN, EN_CHANGE, OnPatternEditChanged)
       NOTIFY_HANDLER(IDC_LIST, LVN_ITEMCHANGED, OnListItemChanged)
       NOTIFY_HANDLER(IDC_LIST, NM_SETFOCUS, OnListItemChanged)
       NOTIFY_HANDLER(IDC_LIST, NM_KILLFOCUS, OnListKillFocus)
       REFLECT_NOTIFICATIONS()
    END_MSG_MAP()

    LRESULT OnAddElement(WORD, WORD, HWND, BOOL&)
    {
        tstring pattern;
        getWindowText(m_pattern, &pattern);

        int index = m_list_values.find(pattern);
        m_list_values.add(index, pattern);

        if (index == -1)
        {
            int pos = m_list.GetItemCount();
            m_list.addItem(pos, 0, pattern);
        }
        else
        {
            m_list.setItem(index, 0, pattern);
        }

        if (index == -1)
            index = m_list.GetItemCount()-1;
        m_list.SelectItem(index);
        m_list.SetFocus();
        return 0;
    }

    LRESULT OnDeleteElement(WORD, WORD, HWND, BOOL&)
    {
        std::vector<int> selected;
        m_list.getSelected(&selected);
        int items = selected.size();
        for (int i = 0; i < items; ++i)
        {
            int index = selected[i];
            m_list.DeleteItem(index);        
            m_list_values.del(index);
        }
        return 0;
    }

    LRESULT OnPatternEditChanged(WORD, WORD, HWND, BOOL&)
    {
        if (!m_update_mode)
        {
            int len = m_pattern.GetWindowTextLength();
            m_add.EnableWindow(len == 0 ? FALSE : TRUE);
            if (len > 0)
            {
                tstring pattern;
                getWindowText(m_pattern, &pattern);
                int index = m_list_values.find(pattern);
                if (index != -1)
                {
                    m_list.SelectItem(index);
                    m_pattern.SetSel(len, len);
                }
            }
        }
        return 0;
    }
    
    LRESULT OnListItemChanged(int , LPNMHDR , BOOL&)
    {
        m_update_mode = true;
        int items_selected = m_list.GetSelectedCount();
        if (items_selected == 0)
        {
            m_del.EnableWindow(FALSE);
            m_pattern.SetWindowText(L"");
        }
        else if (items_selected == 1)
        {
            m_del.EnableWindow(TRUE);
            int item = m_list.getOnlySingleSelection();
            const tstring &v = m_list_values.get(item);
            m_pattern.SetWindowText( v.c_str() );
        }
        else
        {
            m_del.EnableWindow(TRUE);
            m_add.EnableWindow(FALSE);
            m_pattern.SetWindowText( L"" );
        }
        m_update_mode = false;
        return 0;
    }    

    LRESULT OnListKillFocus(int , LPNMHDR , BOOL&)
    {
        if (GetFocus() != m_del && m_list.GetSelectedCount() > 1)
            m_list.SelectItem(-1);
        return 0;
    }    

    LRESULT OnShowWindow(UINT, WPARAM wparam, LPARAM, BOOL&)
    {
        if (wparam)        
        {
            loadValues();
            m_pattern.SetWindowText(L"");
            update();
        }
        else
        {
            m_del.EnableWindow(FALSE);
            saveValues();
        }
        return 0;
    } 
    
    LRESULT OnInitDialog(UINT, WPARAM, LPARAM, BOOL&)
	{
        m_pattern.Attach(GetDlgItem(IDC_EDIT_PATTERN));
        m_add.Attach(GetDlgItem(IDC_BUTTON_ADD));
        m_del.Attach(GetDlgItem(IDC_BUTTON_DEL));
        m_list.Attach(GetDlgItem(IDC_LIST));
        m_list.addColumn(L"�������� �����", 90);        
        m_list.SetExtendedListViewStyle( m_list.GetExtendedListViewStyle() | LVS_EX_GRIDLINES | LVS_EX_FULLROWSELECT);       
        m_bl1.SubclassWindow(GetDlgItem(IDC_STATIC_BL1));
        m_bl2.SubclassWindow(GetDlgItem(IDC_STATIC_BL2));
        m_add.EnableWindow(FALSE);
        m_del.EnableWindow(FALSE);
        loadValues();
        return 0;
    }

    LRESULT OnCloseDialog(UINT, WPARAM, LPARAM, BOOL&)
    {
        saveValues();
        return 0;
    }

    void update()
    {
        m_list.DeleteAllItems();
        for (int i=0,e=m_list_values.size(); i<e; ++i)
        {
            const tstring& v = m_list_values.get(i);
            m_list.addItem(i, 0, v);
        }

        tstring pattern;
        getWindowText(m_pattern, &pattern);
        if (!pattern.empty())
        {
            int index = m_list_values.find(pattern);
            if (index != -1)
                m_list.SelectItem(index);
        }
    }

    void loadValues()
    {
        m_list_values.clear();
        for (int i=0,e=propValues->size(); i<e; ++i) {
            m_list_values.add(-1, propValues->get(i));
        }
    }

    void saveValues()
    {
        propValues->clear();
        for (int i=0,e=m_list_values.size(); i<e; ++i) {
            propValues->add(-1, m_list_values.get(i));
        }        
    }
};