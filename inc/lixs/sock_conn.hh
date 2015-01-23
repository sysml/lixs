#ifndef __LIXS_SOCK_CONN_HH__
#define __LIXS_SOCK_CONN_HH__

#include <lixs/events.hh>
#include <lixs/event_mgr.hh>


namespace lixs {

class sock_conn : public fd_cb_k {
public:
    sock_conn(ev_cb_k& cb, event_mgr& emgr, int fd);
    virtual ~sock_conn();

    bool read(char*& buff, int& bytes);
    bool write(char*& buff, int& bytes);
    bool is_alive(void);

    void operator()(bool read, bool write);

private:
    bool alive;
    ev_cb_k& cb;
    event_mgr& emgr;
};

} /* namespace lixs */

#endif /* __LIXS_SOCK_CONN_HH__ */

