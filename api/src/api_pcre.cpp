#include "stdafx.h"
#include "../api.h"
#include <vector>

#ifdef _DEBUG
#pragma comment(lib, "pcred.lib")
#else
#pragma comment(lib, "pcre.lib")
#endif

extern "C" {
#define PCRE_STATIC
#define SUPPORT_PCRE8
#define SUPPORT_UTF8
#include "pcre.h"
}

struct h_pcre
{
    h_pcre() : regexp(NULL), extra(NULL) {}
    pcre* regexp;
    pcre_extra *extra;
    std::vector<int> indexes;
    u8string str;
    std::vector<u8string> data;

    int size() { return indexes.size() / 2; }
    int last(int index) { return check_index(index) ? indexes[index * 2 + 1] : -1; }
    int first(int index) { return check_index(index) ? indexes[index * 2] : -1; }    
    const utf8* get(int index)
    {
        if (!check_index(index)) return "";
        int b = indexes[index * 2];
        int e = indexes[index * 2 + 1];
        data.push_back(str.substr(b, e - b));
        int last = data.size() - 1;
        return data[last].c_str();
    }
    bool check_index(int index) { return (index >= 0 && index < size()) ? true : false; }
};

pcre8 pcre_create(const utf8* regexp)
{
    if (!regexp || !strlen(regexp))
        return NULL;
    h_pcre *hpcre = NULL;

    const char *error = NULL;
    int error_offset = 0;
    pcre *p = pcre_compile(regexp, PCRE_UTF8, &error, &error_offset, NULL);
    if (p)
    {
        hpcre = new h_pcre;
        hpcre->regexp = p;
        const char *error = NULL;
        hpcre->extra = pcre_study(p, 0, &error);        
    }
    return hpcre;
}

void pcre_delete(pcre8 handle)
{
    if (!handle) return;
    h_pcre* hpcre = (h_pcre*)handle;
    if (hpcre->extra)
        pcre_free_study(hpcre->extra);
    if (hpcre->regexp)
        pcre_free(hpcre->regexp);
    delete hpcre;
}

bool pcre_find(pcre8 handle, const utf8* string)
{
    if (!handle || !string) return false;
    h_pcre* hpcre = (h_pcre*)handle;
    if (!hpcre->regexp) return false;
        
    hpcre->str.assign(string);
    hpcre->indexes.clear();
    int params[16];
    int count = pcre_exec(hpcre->regexp, hpcre->extra, string, strlen(string), 0, 0, params, 48);
    for (int i = 0; i<count; i++)
    {
        hpcre->indexes.push_back(params[2 * i]);
        hpcre->indexes.push_back(params[2 * i + 1]);
    }
    return true;
}

bool pcre_findall(pcre8 handle, const utf8* string)
{
    if (!handle || !string) return false;
    h_pcre* hpcre = (h_pcre*)handle;
    if (!hpcre->regexp) return false;

    hpcre->str.assign(string);
    hpcre->indexes.clear();
    int params[16];
    int pos = 0;
    int len = strlen(string);
    while (1)
    {
        int count = pcre_exec(hpcre->regexp, hpcre->extra, string, len, pos, 0, params, 48);
        if (count <= 0)
            break;
        hpcre->indexes.push_back(params[0]);
        hpcre->indexes.push_back(params[1]);
        pos = params[1];
    }
    return true;
}

int pcre_size(pcre8 handle)
{
    if (!handle) return 0;
    h_pcre* hpcre = (h_pcre*)handle;
    return hpcre->size();
}

int pcre_first(pcre8 handle, int index)
{
    h_pcre* hpcre = (h_pcre*)handle;
    return hpcre->first(index);
}

int pcre_last(pcre8 handle, int index)
{
    h_pcre* hpcre = (h_pcre*)handle;
    return hpcre->last(index);
}

const utf8* pcre_string(pcre8 handle, int index)
{
    h_pcre* hpcre = (h_pcre*)handle;
    return hpcre->get(index);
}