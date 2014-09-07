#pragma once

class HighlightHelper
{
    Pcre16 pcre_colors;
    Pcre16 pcre_rgb;
    Pcre16 pcre_prefix;
    tstring colors;

public:
    HighlightHelper()
    {
        colors.assign(L"(black|red|green|brown|blue|magenta|cyan|gray|coal|light red|light green|yellow|light blue|purple|light cyan|white|light magenta|light brown|grey)");
        pcre_colors.setRegExp(colors);
        pcre_rgb.setRegExp(L"[0-9]+,[0-9]+,[0-9]+");
        pcre_prefix.setRegExp(L"border|line|italic|b");
    }

    bool checkText(tstring* param)
    {
        tstring tmp(*param);
        tstring_tolower(&tmp);
        preprocessColors(tmp);

        Separator s(tmp);
        int size = s.getSize();
        if (!size)
            return false;

        std::vector<tstring> parts(size);
        for (int i=0; i<size; ++i)
        {
            if (checkPrefix(s[i]) || checkColors(s[i]))
                parts[i] = s[i];
            else
                return false;
        }

        bool border = false; bool line = false; bool italic = false;
        tstring text_color, bkg_color;

        for (int i=0; i<size; ++i)
        {
            const tstring& s = parts[i];
            if (s == L"border")
                border = true;
            if (s == L"line")
                line = true;
            if (s == L"italic")
                italic = true;

            if (checkPrefix(s))
                continue;

            if (i != 0 && parts[i-1] == L"b")
            {
                if (!bkg_color.empty())
                    return false;
                bkg_color = s;
                continue;
            }
            if (!text_color.empty())
                return false;
            text_color = s;
        }

        param->clear();
        if (!text_color.empty())
        {
            param->append(L"txt[");
            param->append(text_color);
            param->append(L"],");
        }
        if (!bkg_color.empty())
        {
            param->append(L"bkg[");
            param->append(bkg_color);
            param->append(L"],");
        }
        param->append(L"ubi[");
        param->append(line ? L"1" : L"0");
        param->append(L",");
        param->append(border ? L"1" : L"0");
        param->append(L",");
        param->append(italic ? L"1" : L"0");
        param->append(L"]");
        return true;
    }
private:
    void preprocessColors(tstring& str)
    {
        Pcre16 &p = pcre_colors;
        p.findAllMatches(str);
        if (p.getSize() == 0)
           return;
        WCHAR* table[16] = { L"0,0,0", L"128,0,0", L"0,128,0", L"128,128,0", L"0,0,128", L"128,0,128", L"0,128,128", L"192,192,192",
           L"128,128,128", L"255,0,0", L"0,255,0", L"255,255,0", L"0,0,255", L"255,0,255", L"0,255,255", L"255,255,255" };
        tstring name;
        for (int i=p.getSize()-1; i>=0; --i)
        {
            p.getString(i, &name);
            int pos = colors.find(name);
            int colorid = 0;
            for (int j = 0; j < pos; ++j)
            {
               if (colors.at(j) == L'|') 
                   colorid++;
            }
            if (colorid == 16) colorid = 13; // light magenta -> purple
            if (colorid == 17) colorid = 11; // light brown -> yellow
            if (colorid == 18) colorid = 7;  // grey -> gray

            tstring tmp(str.substr(0,p.getFirst(i)));
            tmp.append(table[colorid]);
            tmp.append(str.substr(p.getLast(i)));
            str = tmp;
        }
     }

     bool checkColors(const tstring& str)
     {
        pcre_rgb.find(str);
        return (pcre_rgb.getSize() != 0) ? true : false;
     }

     bool checkPrefix(const tstring& str)
     {
         Pcre16 &p = pcre_prefix;
         p.find(str);
         if (p.getSize() != 1)
             return false;
         int len = str.length();
         return (p.getFirst(0) == 0 && p.getLast(0) == len) ? true : false;
     }
};