#include "stdafx.h"
#include "phrase.h"
#include <memory>

const DWORD max_db_filesize = 8 * 1024 * 1024; // 8mb
std::map<lua_State*, int> m_dict_types;
typedef std::map<lua_State*, int>::iterator iterator;
int get_dict(lua_State *L)
{
    iterator it = m_dict_types.find(L);
    if (it == m_dict_types.end())
        return -1;
    return it->second;
}
void regtype_dict(lua_State *L, int type)
{
    m_dict_types[L] = type;
}
int dict_invalidargs(lua_State *L, const char* function_name)
{
    luaT_push_args(L, function_name);
    return lua_error(L);
}

class filereader
{
    HANDLE hfile;
    DWORD  fsize;
public:
   filereader() : hfile(INVALID_HANDLE_VALUE), fsize(0) {}
   ~filereader() { if (hfile != INVALID_HANDLE_VALUE) CloseHandle(hfile); }
   bool open(const tstring& path)
   {
       if (hfile != INVALID_HANDLE_VALUE)
       {
           return true;
       }
       hfile = CreateFile(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
       if (hfile == INVALID_HANDLE_VALUE)
            return false;
       DWORD hsize = 0;
       DWORD lsize = GetFileSize(hfile, &hsize);
       if (hsize > 0)
       {
           CloseHandle(hfile);
           hfile = INVALID_HANDLE_VALUE;
           return false;
       }
       fsize = lsize;
       return true;
   }
   DWORD size() const { return fsize; }
   bool read(DWORD pos_begin, DWORD len, MemoryBuffer* buffer)
   {
       if (hfile == INVALID_HANDLE_VALUE)
            return false;
       if (pos_begin > fsize || pos_begin+len > fsize)
           return false;
       DWORD toread = len;
       if (SetFilePointer(hfile, pos_begin, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER)
           return false;
       buffer->alloc(toread);
       DWORD readed = 0;
       if (!ReadFile(hfile, buffer->getData(), toread, &readed, NULL) || readed != toread)
       {
           buffer->alloc(0);
           return false;
       }
       return true;
   }
};

class filewriter
{
    HANDLE hfile;
public:
    filewriter() : hfile(INVALID_HANDLE_VALUE), start_name(0), start_data(0), written(0) {}
    ~filewriter() { if (hfile!=INVALID_HANDLE_VALUE) CloseHandle(hfile); }
    DWORD start_name;
    DWORD start_data;
    DWORD written;

    bool write(const tstring &path, const tstring& name, const tstring& data, const std::vector<tstring>& tegs)
    {
        hfile = CreateFile(path.c_str(), GENERIC_WRITE, 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hfile == INVALID_HANDLE_VALUE)
            return false;
        DWORD hsize = 0;
        DWORD size = GetFileSize(hfile, &hsize);
        if (hsize > 0)
            return false;
        if (SetFilePointer(hfile, size, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER)
            return false;
        DWORD written1 = 0;
        tstring tmp(name);
        tmp.append(L";");
        for (int i=0,e=tegs.size();i<e;++i) 
        {
            tmp.append(tegs[i]);
            tmp.append(L";");
        }
        tmp.append(L"\n");
        if (!write_tofile(hfile, tmp, &written1))
            return error(size);
        DWORD written2 = 0;
        if (!write_tofile(hfile, data, &written2))
            return error(size);
        DWORD written3 = 0;
        if (!write_tofile(hfile, L"\n\n", &written3))
            return error(size);
        start_name = size;
        start_data = size + written1;
        written = written1 + written2 + written3;
        CloseHandle(hfile);
        hfile = INVALID_HANDLE_VALUE;
        return true;
    }

private:
  bool error(DWORD pos) 
  {
      SetFilePointer(hfile,pos,NULL,FILE_BEGIN);
      SetEndOfFile(hfile);
      CloseHandle(hfile);
      hfile = INVALID_HANDLE_VALUE;
      return false;
  }
  bool write_tofile(HANDLE hfile, const tstring& t, DWORD *written)
  {
      *written = 0;
      u8string tmp(TW2U(t.c_str()));
      DWORD towrite = tmp.length();
      if (!WriteFile(hfile, tmp.c_str(), towrite, written, NULL) || *written!=towrite)
          return false;
      return true;
  }
};

class new_filewriter
{
    HANDLE hfile;
protected:
    tstring m_path;
    DWORD start_data;
    DWORD written;
public:
    new_filewriter() : hfile(INVALID_HANDLE_VALUE), start_data(0), written(0) {}
    ~new_filewriter() { close(); }
    bool open(const tstring &path)
    {
        hfile = CreateFile(path.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hfile == INVALID_HANDLE_VALUE)
            return false;
        m_path = true;
        return true;
    }
    void close()
    {
        if (hfile!=INVALID_HANDLE_VALUE)
            CloseHandle(hfile);
        hfile = INVALID_HANDLE_VALUE;
        start_data = 0;
        written = 0;
    }
    void remove()
    {
        error();
    }
protected:
    bool error() 
    {
       close();
       DeleteFile(m_path.c_str());
       return false;
    }
    bool write_tofile(const tstring& t, DWORD *written)
    {
        *written = 0;
        u8string tmp(TW2U(t.c_str()));
        DWORD towrite = tmp.length();
        if (!WriteFile(hfile, tmp.c_str(), towrite, written, NULL) || *written!=towrite)
            return false;
        return true;
    }
    bool write_tofile(const MemoryBuffer& t, DWORD *written)
    {
        *written = 0;
        DWORD towrite = t.getSize();
        if (!WriteFile(hfile, t.getData(), towrite, written, NULL) || *written!=towrite)
            return false;
        return true;
    }
};

class database_file_writer : public new_filewriter
{
public:
    struct WriteResult
    {
        DWORD start;
        DWORD title_len;
        DWORD data_len;
    };
    bool write(const tstring& name, const MemoryBuffer& data, const std::vector<tstring>& tegs, WriteResult* r)
    {
        tstring tmp(name);
        tmp.append(L";");
        for (int i=0,e=tegs.size();i<e;++i) 
        {
            tmp.append(tegs[i]);
            tmp.append(L";");
        }
        tmp.append(L"\n");

        start_data += written;
        DWORD written1 = 0;
        if (!write_tofile(tmp, &written1))
            return error();
        DWORD written2 = 0;
        if (!write_tofile(data, &written2))
            return error();
        written = written1 + written2;
        if (r) 
        {
          r->start = start_data;
          r->title_len = written1;
          r->data_len = written2;
        }
        return true;
    }
};

struct MapDictonaryData
{
    std::string data;
    std::vector<tstring> auto_tegs;
    std::vector<tstring> manual_tegs;
};
typedef std::shared_ptr<MapDictonaryData> data_ptr;
typedef std::map<tstring,data_ptr> MapDictonaryMap;

class MapDictonary
{
    struct fileinfo
    {
        tstring path;
        DWORD size;
    };
    std::vector<fileinfo> m_files;
    struct index
    {
        index() : file(-1), pos_in_file(0), name_tegs_len(0), data_len(0), manual(false) {}
        tstring name;
        int file;
        DWORD pos_in_file;
        DWORD name_tegs_len;
        DWORD data_len;
        bool  manual;
    };
    PhrasesList m_phrases;
    typedef std::shared_ptr<index> index_ptr;
    typedef std::vector<index_ptr> indexes;
    std::unordered_map<tstring, indexes> m_indexes;
    typedef std::unordered_map<tstring, indexes>::iterator iterator;
    std::unordered_map<tstring, index_ptr> m_objects;
    typedef std::unordered_map<tstring, index_ptr>::iterator objects_iterator;    
    typedef std::vector<tstring> manual_tegs;
    typedef std::shared_ptr<manual_tegs> manual_tegs_ptr;
    std::map<tstring, manual_tegs_ptr> m_manual_tegs;
    typedef std::map<tstring, manual_tegs_ptr>::iterator manual_tegs_iterator;
    bool m_manual_tegs_changed;

    int m_current_file;
    tstring m_base_dir;
    lua_State *L;
    MemoryBuffer buffer;
    void fileerror(const tstring& file) 
    {
        tstring e(L"������ ������ �����: ");
        e.append(file);
        base::log(L, e.c_str());
    }
    void filesave_error(const tstring& file)
    {
        tstring e(L"������ ������ �����: ");
        e.append(file);
        base::log(L, e.c_str());
    }

public:
    MapDictonary(const tstring& dir, lua_State *pl) : m_manual_tegs_changed(false), m_current_file(-1), m_base_dir(dir), L(pl)
    {
        buffer.alloc(4096);
        load_db();
        load_manual_tegs();
    }
    ~MapDictonary() 
    {
        save_manual_tegs();
    }
    enum { MD_OK = 0, MD_EXIST, MD_ERROR };

    bool update(const lua_ref& r, tstring *error)
    {
        typedef std::set<index_ptr> indexes_set;
        std::map<int, indexes_set> cat;
        indexes_set empty;
        for (int i=0,e=m_files.size(); i<e; ++i) {
            cat[i] = empty;
        }
        iterator it = m_indexes.begin(), it_end = m_indexes.end();
        for (; it!=it_end; ++it)
        {
            indexes &x = it->second;
            for (int i=0,e=x.size();i<e;++i)
            {
                if (x[i]->manual) continue;
                int file = x[i]->file;
                indexes_set &c = cat[file];
                c.insert(x[i]);
            }
        }

        MemoryBuffer mb;
        std::vector<tstring> tegs;
        struct new_index { DWORD pos; DWORD title_len; DWORD data_len; std::vector<tstring> tegs; };
        std::unordered_map<index_ptr, new_index*> new_indexes;
        std::vector<DWORD> new_files_size(m_files.size(), 0);

        bool result = true;
        std::map<int, indexes_set>::iterator ft = cat.begin(), ft_end = cat.end();
        for (; ft!=ft_end; ++ft)
        {
            fileinfo& fi = m_files[ ft->first ];
            const indexes_set &s = ft->second;
            filereader fr;
            if (!fr.open(fi.path))
            {
                error->assign(L"������ ��� �������� �����: ");
                error->append(fi.path);
                result = false;
                break;
            }

            if (fi.size != fr.size())
            {
                error->assign(L"���� ������� ������ ����������: ");
                error->append(fi.path);
                error->append(L". ������������� ������.");
                result = false;
                break;            
            }

            tstring newfile_path(fi.path);
            newfile_path.append(L".new");
            database_file_writer fw;
            if (!fw.open(newfile_path))
            {
                error->assign(L"������ ��� �������� ����� �� ������: ");
                error->append(newfile_path);
                result = false;
                break;
            }

            indexes_set::iterator si = s.begin(), si_end = s.end();
            for (;si != si_end; ++si)
            {
                index_ptr i = (*si);
                DWORD pos = i->pos_in_file+i->name_tegs_len;
                DWORD len = i->data_len;
                if (!fr.read(pos, len, &mb))
                {
                    error->assign(L"������ ��� ������ �����: ");
                    error->append(fi.path);
                    result = false;
                    break;
                }

                u8string data(mb.getData(), mb.getSize());
                r.pushValue(L);
                lua_pushstring(L, data.c_str());
                if (lua_pcall(L, 1, 1, 0))
                {
                    error->assign(lua_toerror(L));
                    result = false;
                    break;
                }

                tegs.clear();
                if (lua_istable(L, -1))
                {
                    lua_pushnil(L);                     // first key
                    while (lua_next(L, -2) != 0)        // key index = -2, value index = -1
                    {
                        if (lua_isstring(L, -1))
                        {
                            tstring teg(luaT_towstring(L, -1));
                            tegs.push_back(teg);
                        }
                        lua_pop(L, 1);
                    }
                }
                lua_pop(L, 1);
                database_file_writer::WriteResult r;
                if (!fw.write(i->name, mb, tegs, &r))
                {
                    error->assign(L"������ ��� ������ �����: ");
                    error->append(newfile_path);
                    result = false;
                    break;
                }
                new_index *ni = new new_index;
                ni->pos = r.start; ni->title_len=r.title_len; ni->data_len = r.data_len;
                tegs.push_back(i->name);
                ni->tegs.swap(tegs);
                new_indexes[i] = ni;
                new_files_size[ft->first] = r.start + (r.data_len+r.title_len);
            }
            if (!result) 
            {
                fw.remove();
                break;
            }
        }

        if (result)
        {
            // �������������� ������ ������
            for (int i=0,e=m_files.size(); i<e; ++i)
            {
                tstring name(m_files[i].path);
                tstring oldname(name);
                oldname.append(L".old");
                if (!MoveFile(name.c_str(), oldname.c_str()))
                {
                    error->assign(L"������ ��� �������������� ������. ������������ �� �������.");
                    result = false;
                    break;
                }
            }
        }

        if (result)
        {
            // �������������� ����� ������
            for (int i=0,e=m_files.size(); i<e; ++i)
            {
                tstring name(m_files[i].path);
                tstring newname(name);
                newname.append(L".new");
                if (!MoveFile(newname.c_str(), name.c_str()))
                {
                    error->assign(L"������ ��� �������������� ������. ������������ �� �������.");
                    result = false;
                    break;
                }
            }
        }

        if (result)
        {
             // �������� ������ ������
            for (int i=0,e=m_files.size(); i<e; ++i)
            {
                tstring name(m_files[i].path);
                name.append(L".old");
                DeleteFile(name.c_str());
                m_files[i].size = new_files_size[i];
            }

            // ������������� ������� � ����
            m_indexes.clear();
            m_phrases.clear();
            m_objects.clear();
            std::map<tstring, manual_tegs_ptr> new_manual_tegs;
            std::unordered_map<index_ptr, new_index*>::iterator it = new_indexes.begin(), it_end = new_indexes.end();
            for (; it!=it_end; ++it)
            {
                index_ptr i = it->first;
                new_index* ni = it->second;
                i->pos_in_file = ni->pos;
                i->name_tegs_len = ni->title_len;
                i->data_len = ni->data_len;
                std::vector<tstring>& new_tegs = ni->tegs;
                for (int k=0,e=new_tegs.size();k<e;++k)
                    add_index(new_tegs[k], i);
                m_objects[i->name] = i;

                manual_tegs_iterator mt = m_manual_tegs.find(i->name);
                if (mt != m_manual_tegs.end())
                {
                    manual_tegs_ptr newmt = std::make_shared<manual_tegs>();
                    manual_tegs_ptr p = mt->second;
                    for (int j=0,e=p->size(); j<e; ++j) 
                    {
                       const tstring& manual_teg = p->at(j);
                       if (std::find(new_tegs.begin(), new_tegs.end(), manual_teg) == new_tegs.end())
                           newmt->push_back(manual_teg);
                    }
                    if (!newmt->empty())
                    {
                        index_ptr mi = std::make_shared<index>(*i);
                        mi->manual = true;
                        for (int j=0,e=newmt->size(); j<e; ++j)
                        {
                            const tstring& manual_teg = newmt->at(j);
                            add_index(manual_teg, mi);
                        }
                        new_manual_tegs[i->name] = newmt;
                    }
                }
                delete ni;
            }
            new_indexes.clear();
            m_manual_tegs.swap(new_manual_tegs);
            m_manual_tegs_changed = true;
            save_manual_tegs();
        }
        return result;
    }

    void wipe()
    {
        m_current_file = -1;
        for (int i=0,e=m_files.size(); i<e; ++i)
        {
            DeleteFile(m_files[i].path.c_str());
        }
        m_files.clear();
    }

    enum TegResult { ERR = 0, ABSENT, EXIST, ADDED, REMOVED };
    TegResult teg(const tstring& name, const tstring& teg)
    {
        if (teg.find(L";") != teg.npos)
            return ERR;

        objects_iterator ot = m_objects.find(name);
        if (ot == m_objects.end())
            return ABSENT;

        bool manual_indexes_exists = false;
        iterator it = m_indexes.find(teg);
        if (it != m_indexes.end())
        {
            indexes& x = it->second;
            for (int i=0,e=x.size();i<e;++i)
            {
               if (x[i]->name == name)
               {
                   if (!x[i]->manual)
                      return EXIST;
                   manual_indexes_exists = true;
                   break;
               }
            }
        }

        if (manual_indexes_exists)
        {
            struct pred { tstring name;
               bool operator() (index_ptr i) { return (i->manual && i->name == name); }
            };
            indexes& x = it->second;
            pred p; p.name = name;
            x.erase( std::remove_if(x.begin(), x.end(), p), x.end() );
            manual_tegs_iterator mt = m_manual_tegs.find(name);
            if (mt != m_manual_tegs.end())
            {
                struct pred2 { tstring name;
                    bool operator() (const tstring& n) { return (n == name); }
                };
                manual_tegs_ptr pt = mt->second;
                pred2 p2; p2.name = teg;
                pt->erase( std::remove_if(pt->begin(), pt->end(), p2), pt->end());
                if (pt->empty())
                    m_manual_tegs.erase(mt);
            }
            m_manual_tegs_changed = true;
            return REMOVED;
        }

        index_ptr ix = ot->second;
        index_ptr manual_teg_ix = std::make_shared<index>(*ix);
        manual_teg_ix->manual = true;
        add_index(teg, manual_teg_ix);

        manual_tegs_iterator mt = m_manual_tegs.find(ix->name);
        if (mt != m_manual_tegs.end())
        {
            manual_tegs_ptr p = mt->second;
            p->push_back(teg);
        }
        else
        {
            manual_tegs_ptr p = std::make_shared<manual_tegs>();
            p->push_back(teg);
            m_manual_tegs[ix->name] = p;
        }
        m_manual_tegs_changed = true;
        return ADDED;
    }

    int add(const tstring& name, const tstring& data, const std::vector<tstring>& tegs)
    {
        tstring n(name);
        tstring_tolower(&n);
        iterator it = m_indexes.find(n);
        if (it != m_indexes.end())
        {
            return MD_EXIST;
        }
        index_ptr ix = add_tofile(name, data, tegs);
        if (ix->file == -1)
        {
            return MD_ERROR;
        }
        ix->name = name;
        add_index(name, ix);
        for (int i=0,e=tegs.size();i<e;++i)
            add_index(tegs[i], ix);
        m_objects[name] = ix;
        return MD_OK;
    }

    bool find(const tstring& name, MapDictonaryMap* values)
    {
        tstring n(name);
        tstring_tolower(&n);
        Phrase p(n);
        std::vector<tstring> result;
        if (!m_phrases.findPhrase(p, true, &result))
            return false;
        for (int k=0,ke=result.size();k<ke;++k )
        {
            iterator it = m_indexes.find(result[k]);
            if (it == m_indexes.end())
                continue;

            std::vector<filereader> open_files(m_files.size());
            indexes &ix = it->second;
            for (int i=0,e=ix.size()-1;i<=e;++i)
            {
                int fileid = ix[i]->file;
                fileinfo& fi = m_files[fileid];
                filereader& fr = open_files[fileid];
                if (!fr.open(fi.path) || fi.size != fr.size())
                {
                    fileerror(fi.path);
                    continue;
                }

                index_ptr p = ix[i];
                bool result = fr.read(p->pos_in_file, p->name_tegs_len, &buffer);
                if (!result)
                {
                    fileerror(fi.path);
                    continue;
                }

                data_ptr d = std::make_shared<MapDictonaryData>();
                {
                    int size = buffer.getSize();
                    buffer.keepalloc(size+1);
                    char *b = buffer.getData();
                    b[size] = 0;
                    Tokenizer tk(TU2W(b), L";\r\n");
                    tk.trimempty();
                    for (int i=1,e=tk.size();i<e;++i)
                        d->auto_tegs.push_back(tk[i]);
                }
                result = fr.read(p->pos_in_file+p->name_tegs_len, p->data_len, &buffer);
                if (!result)
                {
                    fileerror(fi.path);
                    continue;
                }
                // load data
                {
                   int size = buffer.getSize();
                   buffer.keepalloc(size+1);
                   char *b = buffer.getData();
                   b[size] = 0;
                   d->data.assign(b);
                }
                const tstring& name = ix[i]->name;

                // manual tegs
                manual_tegs_iterator mt = m_manual_tegs.find(name);
                if (mt != m_manual_tegs.end())
                {
                    manual_tegs_ptr p = mt->second;
                    std::copy(p->begin(), p->end(), std::back_inserter(d->manual_tegs));
                }
                values->operator[](name) = d;
            }
        }
        return true;
    }

private:
    void add_index(const tstring& name, index_ptr ix)
    {
        tstring n(name);
        tstring_tolower(&n);
        Phrase p(n);
        int count = p.len();
        if (count > 1)
        {
            for (int i=0; i<count; ++i)
            {
              tstring part(p.get(i));
              m_phrases.addPhrase(new Phrase(part));
              add_toindex(part, ix);
              for (int j=i+1; j<count; ++j) 
              {
                 part.assign(p.get(i));
                 for (int k=j; k<count; ++k)
                 {
                    part.append(L" ");
                    part.append(p.get(k));
                    m_phrases.addPhrase(new Phrase(part));
                    add_toindex(part, ix);
                 }
              }
            }
        }
        else 
        {
          m_phrases.addPhrase(new Phrase(n));
          add_toindex(n, ix);
        }
    }
    void add_toindex(const tstring& t, index_ptr ix)
    {
        iterator it = m_indexes.find(t);
        if (it == m_indexes.end())
        {
            indexes empty;
            m_indexes[t] = empty;
            it = m_indexes.find(t);
        }
        it->second.push_back(ix);
    }

    index_ptr add_tofile(const tstring& name, const tstring& data, const std::vector<tstring>& tegs)
    {
        if (m_current_file != -1) {
           fileinfo &f = m_files[m_current_file];
           if (f.size > max_db_filesize) {
               m_current_file = -1;
           }
        }
        if (m_current_file == -1) {
            for (int i=0,e=m_files.size();i<e;++i) 
            {
                if (m_files[i].size < max_db_filesize)
                    { m_current_file = i; break; }
            }
        }

        index_ptr ix = std::make_shared<index>();
        if (m_current_file == -1)
        {
            int idx = m_files.size();
            tchar buffer[16];
            tstring filename;
            while(true) {
                swprintf(buffer,L"%d.db", idx);
                filename.assign(m_base_dir);
                filename.append(buffer);
                if (GetFileAttributes(filename.c_str()) == INVALID_FILE_ATTRIBUTES)
                    break;
                idx++;
            }
            m_current_file = m_files.size();
            fileinfo f;
            f.path = filename;
            f.size = 0;
            m_files.push_back(f);
        }
        fileinfo &f = m_files[m_current_file];
        filewriter fw;
        if (!fw.write(f.path, name, data, tegs))
           return ix;
        f.size += fw.written;
        ix->file = m_current_file;
        ix->pos_in_file = fw.start_name;
        ix->name_tegs_len = fw.start_data - fw.start_name;
        ix->data_len = fw.written - (fw.start_data - fw.start_name);
        return ix;
    }

    void load_db()
    {
        tstring mask(m_base_dir);
        mask.append(L"*.db");
        WIN32_FIND_DATA fd;
        memset(&fd, 0, sizeof(WIN32_FIND_DATA));
        HANDLE file = FindFirstFile(mask.c_str(), &fd);
        if (file != INVALID_HANDLE_VALUE)
        {
            do {
                if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
                {
                    if (fd.nFileSizeHigh > 0)
                        continue;
                    DWORD max_size = max_db_filesize + 4096;
                    if (fd.nFileSizeLow >= max_size)
                        continue;
                    fileinfo f;
                    tstring path(m_base_dir);
                    path.append(fd.cFileName);
                    f.path = path;
                    f.size = fd.nFileSizeLow;
                    m_files.push_back(f);
                }
            } while (::FindNextFile(file, &fd));
            ::FindClose(file);
        }
        std::sort(m_files.begin(),m_files.end(),[](const fileinfo&a, const fileinfo&b) {
            return a.path < b.path;
        });
        for (int i=0,e=m_files.size();i<e;++i)
        {
            // read files in to catalog
            load_file lf(m_files[i].path);
            if (!lf.result) {
                fileerror(m_files[i].path);
                continue;
            }

            index_ptr ix = std::make_shared<index>();
            ix->file = i;

            DWORD start_pos = 0;
            u8string str, name;
            bool find_name_mode = true;
            while (lf.readNextString(&str, &start_pos))
            {
                if (str.empty())
                {
                    find_name_mode = true;
                    if (!name.empty())
                    {
                        ix->data_len = lf.getPosition()-ix->pos_in_file-ix->name_tegs_len;
                        Tokenizer tk(TU2W(name.c_str()), L";");
                        ix->name = tk[0];
                        {
                           for (int i=0,e=tk.size();i<e;++i)
                             add_index(tk[i], ix);
                        }
                        m_objects[ix->name] = ix;
                        ix = std::make_shared<index>();
                        ix->file = i;
                        name.clear();
                    }
                    continue;
                }
                if (find_name_mode) 
                {
                    name.assign(str);
                    find_name_mode = false;
                    ix->pos_in_file = start_pos;
                    ix->name_tegs_len = lf.getPosition() - start_pos;
                }
            }

        }
    }

    void save_manual_tegs()
    {
        if (!m_manual_tegs_changed)
            return;

        class tegs_file_writer : public new_filewriter  {
        public:
        bool write(const tstring& name, const std::vector<tstring>& tegs)
        {
            tstring s(name);
            for (int i=0,e=tegs.size();i<e;++i)
            {
                s.append(L";");
                s.append(tegs[i]);
            }
            DWORD written = 0;
            return write_tofile(s, &written);
        }} file;

        tstring path(m_base_dir);
        path.append(L"usertegs.new");
        if (!file.open(path.c_str()))
        {
            filesave_error(path);
            return;
        }
        manual_tegs_iterator mt = m_manual_tegs.begin(), mt_end = m_manual_tegs.end();
        for(; mt!=mt_end;++mt)
        {
            manual_tegs_ptr p = mt->second;
            if (p->empty())
                continue;
            if (!file.write(mt->first,*p))
            {
                filesave_error(path);
                return;
            }
        }
        file.close();
        tstring newpath(m_base_dir);
        newpath.append(L"usertegs.db");
        if (!::MoveFileEx(path.c_str(), newpath.c_str(), MOVEFILE_REPLACE_EXISTING))
            return;
        m_manual_tegs_changed = false;
    }

    void load_manual_tegs()
    {
        tstring path(m_base_dir);
        path.append(L"usertegs.db");
        load_file fr(path);
        if (fr.file_missed)
            return;
        if (!fr.result)
        {
            tstring error(L"������ ��� �������� �����: ");
            error.append(path.c_str());
            fileerror( error.c_str() );
            return;
        }
        std::string s;
        while (fr.readNextString(&s))
        {
            tstring ws(TU2W(s.c_str()));
            Tokenizer tk(ws.c_str(), L";");
            if (tk.size() < 2) continue;
            const tstring& name = tk[0];
            for (int i=1,e=tk.size();i<e;++i)
                teg(name, tk[i]);
        }
        fr.close();
    }
};

int dict_add(lua_State *L)
{
    if (luaT_check(L, 3, get_dict(L), LUA_TSTRING, LUA_TSTRING) || 
        luaT_check(L, 4, get_dict(L), LUA_TSTRING, LUA_TSTRING, LUA_TTABLE))
    {
        MapDictonary *d = (MapDictonary*)luaT_toobject(L, 1);
        tstring id(luaT_towstring(L, 2));
        tstring info(luaT_towstring(L, 3));
        std::vector<tstring> tegs;
        if (lua_gettop(L) == 4)
        {
            lua_pushnil(L);
            while (lua_next(L, -2) != 0)        // key index = -2, value index = -1
            {
                tstring teg(luaT_towstring(L, -1));
                tegs.push_back(teg);
                lua_pop(L, 1);
            }
        }
        int result = d->add(id, info, tegs);
        if (result == MapDictonary::MD_OK)
        {
            lua_pushboolean(L, 1);
            return 1;
        }
        lua_pushboolean(L, 0);
        lua_pushstring(L, result == MapDictonary::MD_EXIST ? "exist" : "error");
        return 2;
    }
    return dict_invalidargs(L, "add");
}

int dict_find(lua_State *L)
{
    if (luaT_check(L, 2, get_dict(L), LUA_TSTRING))
    {
        MapDictonary *d = (MapDictonary*)luaT_toobject(L, 1);
        tstring id(luaT_towstring(L, 2));
        MapDictonaryMap result;
        typedef MapDictonaryMap::iterator iterator;
        if (d->find(id, &result) && !result.empty())
        {
            lua_newtable(L);
            iterator it = result.begin(), it_end = result.end();
            for (;it!=it_end;++it)
            {
                luaT_pushwstring(L, it->first.c_str());
                lua_newtable(L);

                data_ptr p = it->second;
                lua_pushstring(L, "data");
                lua_pushstring(L, p->data.c_str());
                lua_settable(L, -3);

                const std::vector<tstring>& at = p->auto_tegs;
                lua_pushstring(L, "auto");
                lua_newtable(L);
                for (int i=0,e=at.size();i<e;++i)
                {
                    lua_pushinteger(L, i+1);
                    luaT_pushwstring(L, at[i].c_str());
                    lua_settable(L, -3);
                }                
                lua_settable(L, -3);

                const std::vector<tstring>& mt = p->manual_tegs;
                lua_pushstring(L, "tegs");
                lua_newtable(L);
                for (int i=0,e=mt.size();i<e;++i)
                {
                    lua_pushinteger(L, i+1);
                    luaT_pushwstring(L, mt[i].c_str());
                    lua_settable(L, -3);
                }
                lua_settable(L, -3);
                lua_settable(L, -3);
            }
        }
        else
            lua_pushnil(L);
        return 1;
    }
    return dict_invalidargs(L, "find");
}

int dict_update(lua_State *L)
{
   if (luaT_check(L, 2, get_dict(L), LUA_TFUNCTION))
   {
       MapDictonary *d = (MapDictonary*)luaT_toobject(L, 1);
       lua_ref r;
       r.createRef(L);
       tstring error;
       bool result = d->update(r, &error);
       r.unref(L);
       if (result) { lua_pushboolean(L, 1); return 1; }
       lua_pushboolean(L, 0);
       luaT_pushwstring(L, error.c_str());
       return 2;
   }
   return dict_invalidargs(L, "update");
}

int dict_wipe(lua_State *L)
{
   if (luaT_check(L, 1, get_dict(L)))
   {
       MapDictonary *d = (MapDictonary*)luaT_toobject(L, 1);
       d->wipe();
       return 0;
   }
   return dict_invalidargs(L, "wipe");
}

int dict_teg(lua_State *L)
{
    if (luaT_check(L, 3, get_dict(L), LUA_TSTRING, LUA_TSTRING))
    {
        MapDictonary *d = (MapDictonary*)luaT_toobject(L, 1);
        tstring name(luaT_towstring(L, 2));
        tstring teg(luaT_towstring(L, 3));
        MapDictonary::TegResult result = d->teg(name, teg);
        switch (result) {
        case MapDictonary::ABSENT:
            lua_pushstring(L, "absent");
            break;
        case MapDictonary::EXIST:
            lua_pushstring(L, "exist");
            break;
        case MapDictonary::ADDED:
            lua_pushstring(L, "added");
            break;
        case MapDictonary::REMOVED:
            lua_pushstring(L, "removed");
            break;
        default:
            lua_pushnil(L);
        }
        return 1;
    }
    return dict_invalidargs(L, "teg");
}

int dict_gc(lua_State *L)
{
    if (luaT_check(L, 1, get_dict(L)))
    {
        MapDictonary *d = (MapDictonary *)luaT_toobject(L, 1);
        delete d;
    }
    return 0;
}

/*int dict_remove(lua_State *L)
{
    if (luaT_check(L, 2, get_dict(L), LUA_TSTRING))
    {
        MapDictonary *d = (MapDictonary*)luaT_toobject(L, 1);
        tstring id(luaT_towstring(L, 2));
        bool result = d->remove(id);
        lua_pushboolean(L, result ? 1:0);
        return 1;
    }
    return dict_invalidargs(L, "remove");
}*/

int dict_new(lua_State *L)
{
    if (!luaT_check(L, 1, LUA_TSTRING))
    {
        luaT_push_args(L, "dictonary");
        return lua_error(L);
    }
    tstring path(luaT_towstring(L, 1));

    if (get_dict(L) == -1)
    {
        int type = luaT_regtype(L, "dictonary");
        if (!type)
            return 0;
        regtype_dict(L, type);
        luaL_newmetatable(L, "dictonary");
        regFunction(L, "add", dict_add);
        regFunction(L, "find", dict_find);
        regFunction(L, "update", dict_update);
        regFunction(L, "wipe", dict_wipe);
        regFunction(L, "teg", dict_teg);
        //regFunction(L, "remove", dict_remove);
        regFunction(L, "__gc", dict_gc);
        lua_pushstring(L, "__index");
        lua_pushvalue(L, -2);
        lua_settable(L, -3);
        lua_pushstring(L, "__metatable");
        lua_pushstring(L, "access denied");
        lua_settable(L, -3);
        lua_pop(L, 1);
    }
    MapDictonary* nd = new MapDictonary(path, L);
    luaT_pushobject(L, nd, get_dict(L));
    return 1;
}
