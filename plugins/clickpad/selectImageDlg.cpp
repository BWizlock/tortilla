#include "stdafx.h"
#include "clickpad.h"
#include "selectImageDlg.h"

void error(const tchar* msg, const tchar* p1 = NULL)
{
    tstring errtext(msg);
    if (p1) errtext.append(p1);
    TW2U text(errtext.c_str());
    luaT_log(getLuaState(), text);
}

ImageCollection::~ImageCollection()
{
    for (int i=0,e=m_files.size();i<e;++i)
        delete m_files[i].image;
}

int ImageCollection::getImagesCount() const
{
    return m_files.size();
}

const ImageCollection::imdata& ImageCollection::getImage(int index) const
{
    assert(index >= 0 && index < getImagesCount());
    return m_files[index];
}

void ImageCollection::scanImages()
{
    u8string sdir;
    base::getResource(getLuaState(), "", &sdir);
    tstring dir(TU2W(sdir.c_str()));
    if (!(GetFileAttributes(dir.c_str()) & FILE_ATTRIBUTE_DIRECTORY))
        return error(L"���������� ��������� ������� � ��������: ", dir.c_str());

    // 1. get current files list
    std::vector<imdata> current_files;
    tchar current_path[MAX_PATH + 1];
    GetCurrentDirectory(MAX_PATH, current_path);
    std::vector<tstring> dirs;
    dirs.push_back(dir);
    for (int index=0; index != dirs.size(); ++index)
    {
        tstring fullpath(dirs[index]);
        if (!SetCurrentDirectory(fullpath.c_str()))
            continue;
        WIN32_FIND_DATA fd;
        memset(&fd, 0, sizeof(WIN32_FIND_DATA));
        HANDLE file = FindFirstFile(L"*.*", &fd);
        if (file != INVALID_HANDLE_VALUE)
        {
            do
            {
                tstring filename(fd.cFileName);
                if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                {
                    if (filename == L"." || filename == L"..")
                        continue;
                    tstring newdir(fullpath);
                    newdir.append(filename);
                    newdir.append(L"\\");
                    dirs.insert(dirs.begin()+index+1, newdir);
                }
                else
                {
                    size_t e = filename.find_last_of(L'.');
                    if (e == -1) continue;
                    tstring ext(filename.substr(e + 1));
                    bool ok = (ext == L"png" || ext == L"bmp" || ext == L"gif" || ext == L"ico" || ext == L"jpg") ? true : false;
                    if (!ok)
                        continue;
                    imdata f;
                    f.name = filename;
                    tstring &p = f.file_path;
                    p.append(fullpath);
                    p.append(filename);
                    f.image_size = 0;
                    size_t end = fullpath.find_last_of(L'\\');
                    if (end != -1)
                    {
                        size_t from = fullpath.find_last_of(L'\\', end-1);
                        if (from != -1) {
                        tstring lastdir(fullpath.substr(from+1, end-from-1));
                        bool only_numbers = (wcsspn(lastdir.c_str(), L"0123456789") != lastdir.length()) ? false : true;
                        int size = (only_numbers) ? _wtoi(lastdir.c_str()) : 0;
                        if (size > 0)
                            f.image_size = size;
                        }
                    }
                    current_files.push_back(f);
                }
            } while (::FindNextFile(file, &fd));
            ::FindClose(file);
        }
    }
    SetCurrentDirectory(current_path);

    // check diffs
    std::vector<imdata> new_files;
    for (int i=0,e=m_files.size(); i<e; ++i)
    {
        bool exist = false;
        for (int j=0,je=current_files.size();j<je;++j) {
            if (m_files[i].file_path == current_files[j].file_path) { exist = true; break; }
        }
        if (!exist) { delete m_files[i].image; }
        else { new_files.push_back(m_files[i]); }    
    }
    m_files.clear();
    m_files.swap(new_files);

    for (int j=0,je=current_files.size();j<je;++j)
    {
        bool exist = false;
        for (int i=0,e=m_files.size(); i<e; ++i) {
            if (current_files[j].file_path == m_files[i].file_path) { exist = true; break; }
        }
        if (!exist) 
        {
            const tchar* filepath = current_files[j].file_path.c_str();
            Image *img = new Image();
            if (!img->load(filepath, 0))
            {
                delete img; img = NULL;
                error(L"���������� ��������� ����: ", filepath);
            }
            else
            {
                current_files[j].image = img;
                new_files.push_back(current_files[j]);
            }
        }
    }
    m_files.insert(m_files.end(), new_files.begin(), new_files.end());
}

void SelectImage::setImage(Image* image, int size)
{
    if (!image || image->width() == 0 || image->height() == 0 || size < 0) {
        m_pimg = NULL; m_size = 0; 
        Invalidate(FALSE);
        return;
    }
    m_pimg = image;
    m_size = size;
    Invalidate(FALSE);
}

void SelectImage::renderImage(HDC hdc, int width, int height)
{
    CDCHandle dc(hdc);
    dc.FillSolidRect(0, 0, width, height, GetSysColor(COLOR_WINDOWFRAME));
    if (!m_pimg) return;
    m_pimg->render(dc, 0, 0);
}

LRESULT SelectImageDlg::OnUser(UINT, WPARAM, LPARAM, BOOL&)
{
    tstring item;
    m_category.getSelectedItem(&item);
    for (int i = 0, e = m_images.getImagesCount(); i < e; ++i)
    {
        const ImageCollection::imdata &image = m_images.getImage(i);
        if (image.name == item)
        {
            m_atlas.setImage(image.image, image.image_size);
            return 0;
        }
    }
    m_atlas.setImage(NULL, 0);
    return 0;
}

LRESULT SelectImageDlg::OnCreate(UINT, WPARAM, LPARAM, BOOL&)
{
    RECT rc;
    GetClientRect(&rc);
    m_vSplitter.Create(m_hWnd, rc);
    m_vSplitter.m_cxySplitBar = 3;
    m_vSplitter.SetSplitterRect();
    m_vSplitter.SetDefaultSplitterPos();

    RECT pane_left, pane_right;
    m_vSplitter.GetSplitterPaneRect(0, &pane_left);
    pane_left.right -= 3;
    m_vSplitter.GetSplitterPaneRect(1, &pane_right);

    DWORD style = WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_VISIBLE;
    m_category.Create(m_vSplitter, pane_left); // NULL, style, WS_EX_CLIENTEDGE);
    m_category.setNotyfy(m_hWnd, WM_USER);

    m_atlas.Create(m_vSplitter, pane_right, NULL, style); // | WS_VSCROLL | WS_HSCROLL);
    m_vSplitter.SetSplitterPanes(m_category, m_atlas);

    m_images.scanImages();
    for (int i = 0, e = m_images.getImagesCount(); i < e; ++i)
    {
        const ImageCollection::imdata &image = m_images.getImage(i);
        m_category.addItem(image.name);
    }
    return 0;
}

LRESULT SelectImageDlg::OnSize(UINT, WPARAM, LPARAM, BOOL&)
{
    RECT rc;
    GetClientRect(&rc);
    m_vSplitter.MoveWindow(&rc, FALSE);
    m_vSplitter.SetSplitterRect();
    return 0;
}
