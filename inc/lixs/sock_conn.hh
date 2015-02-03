#ifndef __LIXS_SOCK_CONN_HH__
#define __LIXS_SOCK_CONN_HH__

#include <lixs/events.hh>
#include <lixs/event_mgr.hh>


namespace lixs {

class sock_conn : public fd_cb_k {
protected:
    sock_conn(event_mgr& emgr, int fd);
    virtual ~sock_conn();

    bool read(char*& buff, int& bytes);
    bool write(char*& buff, int& bytes);

    virtual void process(void) = 0;
    virtual void conn_dead(void) = 0;

private:
    void operator()(bool read, bool write);

private:
    bool alive;
    event_mgr& emgr;
};

} /* namespace lixs */

#endif /* __LIXS_SOCK_CONN_HH__ */

