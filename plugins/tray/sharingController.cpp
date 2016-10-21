#include "stdafx.h"
#include "sharingController.h"

const wchar_t *global_share_name = L"TortillaTray";
const int global_share_size = sizeof(SharingHeader) + 100*sizeof(SharingWindow);

class SharedMemoryInitializer : public SharedMemoryHandler
{
    void onInitSharedMemory(SharedMemoryData *d)
    {
        SharingHeader sh;
        size_t datalen = sizeof(SharingHeader);
        sh.counter_id = 1;
        sh.messages = 0;
        memcpy(d->data, &sh, datalen);
        d->data_size = datalen;
    }
};

SharingController::SharingController() : m_id(0) 
{
}

SharingController::~SharingController()
{
}

bool SharingController::init()
{
    SharedMemoryInitializer smi;
    if (!m_shared_memory.create(global_share_name, global_share_size, &smi))
        return false;
    // get id
    SharedMemoryLocker l(&m_shared_memory);
    SharedMemoryData* m = l.memory();
    SharingHeader* sh = getHeader(m);
    m_id = sh->counter_id;
    sh->counter_id = m_id+1;
    return true;
}

bool SharingController::tryAddWindow(const SharingWindow& sw)
{
    SharedMemoryLocker l(&m_shared_memory);
    SharedMemoryData* m = l.memory();
    if (m->data_size+sizeof(SharingWindow) > m->max_size)
       return false;
    SharingHeader* h = getHeader(m);
    int count = h->messages;
    SharingWindow* w = getWindow(count, m);
    *w = sw;
    h->messages = count + 1;
    return true;
}

void SharingController::deleteWindow(const SharingWindow& sw)
{
    SharedMemoryLocker l(&m_shared_memory);
    SharedMemoryData* m = l.memory();
    SharingHeader* h = getHeader(m);
    int count = h->messages;
    SharingWindow* w = getWindow(0, m);
    for (int i=0; i<count; ++i)
    {
        SharingWindow& c = w[i];
        if (c.x == sw.x && c.y == sw.y && c.w == sw.w && c.h == sw.h) {
            memcpy(&w[i], &w[i+1], sizeof(SharingWindow)*(count-i-1));
            h->messages = count - 1;
            break;
        }
    }
}

void SharingController::updateWindow(const SharingWindow& sw, int newx, int newy)
{
    SharedMemoryLocker l(&m_shared_memory);
    SharedMemoryData* m = l.memory();
    SharingHeader* h = getHeader(m);
    int count = h->messages;
    SharingWindow* w = getWindow(0, m);
    for (int i=0; i<count; ++i)
    {
        SharingWindow& c = w[i];
        if (c.x == sw.x && c.y == sw.y && c.w == sw.w && c.h == sw.h) {
            c.x = newx; c.y = newy;
            break;
        }
    }
}

SharingHeader* SharingController::getHeader(SharedMemoryData *d)
{
    return (SharingHeader*)d->data;
}

SharingWindow* SharingController::getWindow(int index, SharedMemoryData* d)
{
    SharingHeader *h = getHeader(d);
    SharingWindow* w = (SharingWindow*)(h+1);
    return &w[index];
}
