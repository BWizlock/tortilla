#include "stdafx.h"
#include "common.h"

void loadString(UINT id, tstring* string)
{
    WCHAR buffer[256];
    LoadString(GetModuleHandle(NULL), id, buffer, 256);
    string->assign(buffer);
}

int msgBox(HWND parent, UINT msg, UINT options)
{
    tstring msg_text, title_text;
    loadString(msg, &msg_text);
    loadString(IDR_MAINFRAME, &title_text);
    return MessageBox(parent, msg_text.c_str(), title_text.c_str(), options);
}

void getWindowText(HWND handle, tstring *string)
{
    int text_len = ::GetWindowTextLength(handle);
    MemoryBuffer tmp((text_len+2)*sizeof(WCHAR)); 
    WCHAR *buffer = (WCHAR*)tmp.getData();
    ::GetWindowText(handle, buffer, text_len+1);
    string->assign(buffer);
}

bool isOnlySymbolsA(const std::string& str, const std::string& symbols)
{
   int pos = strspn(str.c_str(), symbols.c_str());
   return (pos != str.length()) ? false : true;
}

bool isOnlyDigitsA(const std::string& str)
{
    return isOnlySymbolsA(str, "0123456789");
}

bool isOnlyDigits(const tstring& str)
{
   int pos = wcsspn(str.c_str(), L"0123456789");
   return (pos != str.length()) ? false : true;
}

bool isOnlySpaces(const tstring& str)
{
   int pos = wcsspn(str.c_str(), L" ");
   return (pos != str.length()) ? false : true;
}

bool a2int(const std::string& str, int *value)
{
    if (!isOnlyDigitsA(str))
        return false;
    *value = atoi(str.c_str());
    return true;
}

bool isExistSymbols(const tstring& str, const tstring& symbols)
{
   int pos = wcscspn(str.c_str(), symbols.c_str());
   return (pos != str.length()) ? true : false;
}

void tstring_trimleft(tstring *str)
{
    int pos = wcsspn(str->c_str(), L" ");
    if (pos != 0)
        str->assign(str->substr(pos));
}

void tstring_trimright(tstring *str)
{
    if (str->empty())
        return;
    int last = str->size() - 1;
    int pos = last;
    while (str->at(pos) == L' ')
        pos--;
    if (pos != last)
        str->assign(str->substr(0, pos+1));
}

void tstring_trim(tstring *str)
{
    tstring_trimleft(str);
    tstring_trimright(str);
}

void tstring_toupper(tstring *str)
{
    std::transform(str->begin(), str->end(), str->begin(), ::toupper);
}

void tstring_tolower(tstring *str)
{
    std::transform(str->begin(), str->end(), str->begin(), ::tolower);
}

void tstring_replace(tstring *str, const tstring& what, const tstring& forr)
{
    size_t pos = 0;
    while((pos = str->find(what, pos)) != std::string::npos)
    {
        str->replace(pos, what.length(), forr);
        pos += forr.length();
    }
}

bool tstring_cmpl(const tstring& str, const WCHAR* lstr)
{
    return (wcsncmp(str.c_str(), lstr, wcslen(lstr)) == 0) ? true : false;    
}

Separator::Separator(const tstring& str)
{
    if (str.empty())
        return;

    const WCHAR *b = str.c_str();
    const WCHAR *e = b + str.length();
    
    bool word = (*b == L' ') ? false : true; 
    const WCHAR *p = b + 1;
    for (;p != e; ++p)
    {
        if (*p == L' ' && word)
        {
            tstring new_part(b, p-b);
            m_parts.push_back(new_part);
            word = false;
        }
        if (*p != L' ' && !word)
        {
            b = p;
            word = true;
        }
    }
    if (word)
    {
        tstring new_part(b, e-b);
        m_parts.push_back(new_part);
    }
}

int Separator::getSize() const
{
    return m_parts.size();
}
    
const tstring& Separator::operator[](int index)
{
    return (index >= 0 && index <getSize()) ? m_parts[index] : empty;
}

COLORREF invertColor(COLORREF c)
{
    return RGB((GetRValue(c) ^ 0xff),(GetGValue(c) ^ 0xff),(GetBValue(c) ^ 0xff));
}

bool sendToClipboard(HWND owner, const tstring& text)
{  
    if (!OpenClipboard(owner))
        return false;

    if (!EmptyClipboard())
    {
        CloseClipboard();
        return false;
    }

    SIZE_T size = (text.length() + 1) * sizeof(WCHAR);
    HGLOBAL hGlob = GlobalAlloc(GMEM_FIXED, size);
    if (!hGlob)
    {
        CloseClipboard();   
        return false;
    }

    WCHAR* buffer = (WCHAR*)GlobalLock(hGlob);
    wcscpy(buffer, text.c_str());
    GlobalUnlock(hGlob);
    bool result = (SetClipboardData(CF_UNICODETEXT, hGlob) == NULL) ? false : true;        
    CloseClipboard();
    if (!result)
        GlobalFree(hGlob);
    return result;    
}

bool getFromClipboard(HWND owner, tstring* text)
{
    if (!OpenClipboard(owner))
        return false;

    bool result = false;
    HANDLE hClipboardData = GetClipboardData(CF_UNICODETEXT);
    if (hClipboardData)
    {
        WCHAR *pchData = (WCHAR*)GlobalLock(hClipboardData);
        if (pchData)
        {
            text->assign(pchData);
            GlobalUnlock(hClipboardData);
            result = true;
        }
    }
    CloseClipboard();
    return result;
}