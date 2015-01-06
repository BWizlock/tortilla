#include "stdafx.h"
#include "mapperKey.h"

MapperKey::MapperKey() : m_find_end_mode(false)
{
}

void MapperKey::updateProps(PropertiesMapper *props)
{
    bk.init(props->begin_key);
    ek.init(props->end_key);
}

void MapperKey::processNetworkData(MapperNetworkData &ndata)
{
    //return; //todo
    if (!m_find_end_mode)       // ����� ������ ������ �����
    {
        if (!bk.isKeyUsable())  // ������ ����� ��� �� �������
        {
            if (!bk.findData(ndata.getData(), ndata.getDataLen()))
                return;
            // ����� ������ ���� ��� ���������, �� � ����� ����� ������
            if (bk.isKeyFull() || bk.getAfterKey() == ndata.getDataLen())
            {
                int pos = bk.getKey();
                const WCHAR* key_begin = ndata.getData() + pos;
                int key_len = ndata.getDataLen() - pos;
                m_buffer.write(key_begin, key_len);
                ndata.trimLeft(pos);
                m_find_end_mode = true;
                if (bk.isKeyFull())
                    m_buffer.truncate(bk.getKeyLen());
            }
            return;
        }

        // ���� �������� � ������ ���������� ����� � ����� ����� ������
        // ������� ����������� ����
        m_buffer.write(ndata.getData(), ndata.getDataLen());
        ndata.trimLeft(0);

        int datalen = m_buffer.getDataLen();
        if (datalen < bk.getMaskLen())
            return;

        const WCHAR* data = m_buffer.getData();
        if (bk.findData(data, datalen) && bk.isKeyFull())
        {
            m_buffer.truncate(bk.getKeyLen());
            m_find_end_mode = true;
            // ���������� ��� � ������� ����� �����
        }
        else
        {
            // ���� �� ������ - ���������� ����� �� �����
            ndata.accept(m_buffer.getData(), m_buffer.getDataLen());
            m_buffer.clear();
            return;
        }
    }

    // ����� ��������� �����
    m_buffer.write(ndata.getData(), ndata.getDataLen());
    ndata.trimLeft(0);

    int datalen = m_buffer.getDataLen();
    const WCHAR* data = m_buffer.getData();

    if (ek.findData(data, datalen) && ek.isKeyFull())
    {
        // ����� ��������� �����
        int pos = ek.getAfterKey();        
        ndata.accept(data + pos, datalen - pos);
        m_buffer.clear();
        m_find_end_mode = false;
        return;
    }
}
