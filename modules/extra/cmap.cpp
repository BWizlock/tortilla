#include "stdafx.h"
#include "phrase.h"

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
   bool open(const tstring& path, DWORD csize)
   {
       if (hfile != INVALID_HANDLE_VALUE)
       {
           assert(fsize == csize);
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
       assert(fsize == csize);
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

    bool write(const tstring &path, const tstring& name, const tstring& data)
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
        tstring tmp(name); tmp.append(L"\n");
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

class MapDictonary
{
    PhrasesList m_phrases;
    struct fileinfo
    {
        tstring path;
        DWORD size;
    };
    std::vector<fileinfo> m_files;
    struct index
    {
        index() : file(-1), data_in_file(0), datalen_in_file(0) {}
        tstring name;
        int file;
        DWORD data_in_file;
        DWORD datalen_in_file;
    };
    typedef std::vector<index> indexes;
    std::unordered_map<tstring, indexes> m_indexes;
    typedef std::unordered_map<tstring, indexes>::iterator iterator;
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

public:
    MapDictonary(const tstring& dir, lua_State *pl) : m_current_file(-1), m_base_dir(dir), L(pl) {
        buffer.alloc(4096);
        load_db();
    }
    ~MapDictonary() {}
    enum { MD_OK = 0, MD_EXIST, MD_ERROR };
    void wipe()
    {
        m_current_file = -1;
        m_indexes.clear();
        for (int i=0,e=m_files.size(); i<e; ++i)
        {
            DeleteFile(m_files[i].path.c_str());
        }
        m_files.clear();
    }

    int add(const tstring& name, const tstring& data)
    {
        tstring n(name);
        tstring_tolower(&n);
        iterator it = m_indexes.find(n);
        if (it != m_indexes.end())
        {
            return MD_EXIST;
        }
        index ix = add_tofile(name, data);
        if (ix.file == -1)
            return MD_ERROR;
        ix.name = name;
        add_index(ix);
        return MD_OK;
    }

    bool find(const tstring& name, std::map<tstring,tstring>* values)
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
                int fileid = ix[i].file;
                fileinfo& fi = m_files[fileid];
                filereader& fr = open_files[fileid];
                if (!fr.open(fi.path, fi.size))
                {
                    fileerror(fi.path);
                    continue;
                }

                bool result = fr.read(ix[i].data_in_file, ix[i].datalen_in_file, &buffer);
                if (!result)
                {
                    fileerror(fi.path);
                    continue;
                }
                int size = buffer.getSize();
                buffer.keepalloc(size+1);
                char *p = buffer.getData();
                p[size] = 0;
                const tstring& name = ix[i].name;
                values->operator[](name) = tstring(TU2W(p));
            }
        }
        return true;
    }

private:
    void add_index(index ix)
    {
        tstring n(ix.name);
        tstring_tolower(&n);
        Phrase p(n);
        int count = p.len();
        if (count > 1)
        {
            for (int i=0; i<count; ++i)
            {
              tstring part(p.get(i));
              if (part.length() > 2)
              {
                 m_phrases.addPhrase(new Phrase(part));
                 add_toindex(part, ix);
              }
              for (int j=i+1; j<count; ++j) {
                 part.append(L" ");
                 part.append(p.get(j));
                 m_phrases.addPhrase(new Phrase(part));
                 add_toindex(part, ix);
              }
            }
        }
        else 
        {
          m_phrases.addPhrase(new Phrase(n));
          add_toindex(n, ix);
        }
    }
    void add_toindex(const tstring& t, index ix)
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

    index add_tofile(const tstring& name, const tstring& data)
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

        index ix;
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
        if (!fw.write(f.path, name, data))
           return ix;
        f.size += fw.written;
        ix.file = m_current_file;
        ix.data_in_file = fw.start_data;
        ix.datalen_in_file = fw.written - (fw.start_data - fw.start_name);
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

            index ix;
            ix.file = i;
            ix.data_in_file = 0;
            ix.datalen_in_file = 0;

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
                        if (ix.data_in_file != 0)
                        {
                            ix.datalen_in_file = lf.getPosition()-ix.data_in_file;
                            ix.name = TU2W(name.c_str());
                            add_index(ix);
                        }
                        ix.data_in_file = 0;
                        ix.datalen_in_file = 0;
                        name.clear();
                    }
                    continue;
                }
                if (find_name_mode) 
                {
                    name.assign(str);
                    find_name_mode = false;
                    ix.data_in_file = lf.getPosition();
                }
            }

        }
    }
};

int dict_add(lua_State *L)
{
    if (luaT_check(L, 3, get_dict(L), LUA_TSTRING, LUA_TSTRING))
    {
        MapDictonary *d = (MapDictonary*)luaT_toobject(L, 1);
        tstring id(luaT_towstring(L, 2));
        tstring info(luaT_towstring(L, 3));
        int result = d->add(id, info);
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
        std::map<tstring,tstring> result;
        typedef std::map<tstring,tstring>::iterator iterator;
        if (d->find(id, &result))
        {
            lua_newtable(L);
            iterator it = result.begin(), it_end = result.end();
            for (;it!=it_end;++it)
            {
                luaT_pushwstring(L, it->first.c_str());
                luaT_pushwstring(L, it->second.c_str());
                lua_settable(L, -3);
            }
        }
        else
            lua_pushnil(L);
        return 1;
    }
    return dict_invalidargs(L, "find");
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

int dict_gc(lua_State *L)
{
    if (luaT_check(L, 1, get_dict(L)))
    {
        MapDictonary *d = (MapDictonary *)luaT_toobject(L, 1);
        delete d;
    }
    return 0;
}

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
        regFunction(L, "wipe", dict_wipe);

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
