#ifndef __LIXS_CLIENT_HH__
#define __LIXS_CLIENT_HH__

#include <lixs/events.hh>
#include <lixs/event_mgr.hh>
#include <lixs/xenstore.hh>

#include <cerrno>
#include <list>
#include <map>
#include <string>
#include <utility>

extern "C" {
#include <xen/io/xs_wire.h>
}


namespace lixs {

class client_base {
protected:
    struct msg {
        msg(char* buff)
            : hdr(*((xsd_sockmsg*)buff)), abs_path(buff + sizeof(xsd_sockmsg)), body(abs_path)
        { }

        struct xsd_sockmsg& hdr;
        char* abs_path;
        char* body;
    };

    class ev_cb_k : public lixs::ev_cb_k {
    public:
        ev_cb_k(client_base& client)
            : _client(client)
        { };

        void operator()(void);

        client_base& _client;
    };

    class watch_cb_k : public lixs::watch_cb_k {
    public:
        watch_cb_k(client_base& client, char* path, char* token, bool rel)
            : lixs::watch_cb_k(path, token), _client(client), rel(rel)
        { };

        void operator()(const std::string& path);

        client_base& _client;
        bool rel;
    };

    enum state {
        p_rx,
        rx_hdr,
        rx_body,
        p_tx,
        tx_hdr,
        tx_body,
        p_watch,
    };


    client_base(xenstore& xs, event_mgr& emgr);
    virtual ~client_base();

    virtual void operator()(void) = 0;
    virtual void process(void) = 0;

    virtual bool read(char*& buff, int& bytes) = 0;
    virtual bool write(char*& buff, int& bytes) = 0;

    void handle_msg(void);

    void op_read(void);
    void op_write(void);
    void op_mkdir(void);
    void op_rm(void);
    void op_transaction_start(void);
    void op_transaction_end(void);
    void op_get_domain_path(void);
    void op_get_perms(void);
    void op_set_perms(void);
    void op_set_target(void);
    void op_restrict(void);
    void op_directory(void);
    void op_watch(void);
    void op_unwatch(void);
    void op_reset_watches(void);
    void op_introduce(void);
    void op_release(void);
    void op_is_domain_introduced(void);
    void op_debug(void);
    void op_resume(void);

    char* get_path(void);
    void build_resp(const char* resp);
    void append_resp(const char* resp);
    void append_sep(void);
    void build_watch(const char* path, const char* token);
    void build_err(int err);
    void build_ack(void);

#ifdef DEBUG
    void print_msg(char* pre);
#endif

    xenstore& xs;
    event_mgr& emgr;

    ev_cb_k ev_cb;
    client_base::state state;

    std::map<std::string, watch_cb_k> watches;
    std::list<std::pair<std::string, watch_cb_k&> > fire_lst;

    /*
     * buff: [HEADER][/local/domain/<id>][BODY][/0]
     */
    char buff[sizeof(xsd_sockmsg) + 35 + XENSTORE_PAYLOAD_MAX + 1];
    struct msg msg;

    char* cid;

    char* read_buff;
    char* write_buff;
    int read_bytes;
    int write_bytes;
};


template < typename CONNECTION >
class client : public client_base, public CONNECTION, public ev_cb_k {
public:
    template < typename... ARGS >
    client(xenstore& xs, event_mgr& emgr, ARGS&&... args);
    virtual ~client();

private:
    void operator()(void);
    void process(void);

    bool read(char*& buff, int& bytes);
    bool write(char*& buff, int& bytes);
};

template < typename CONNECTION >
template < typename... ARGS >
client<CONNECTION>::client(xenstore& xs, event_mgr& emgr, ARGS&&... args)
    : client_base(xs, emgr), CONNECTION(*this, emgr, std::forward<ARGS>(args)...)
{
}

template < typename CONNECTION >
client<CONNECTION>::~client()
{
}

template < typename CONNECTION >
void client<CONNECTION>::operator()(void)
{
    process();
    if (!CONNECTION::is_alive()) {
        delete this;
    }
}

template < typename CONNECTION >
void client<CONNECTION>::process(void)
{
    bool ret;
    bool yield = false;

    while (!yield && CONNECTION::is_alive()) {
        switch(state) {
            case p_rx:
                read_buff = reinterpret_cast<char*>(&(msg.hdr));
                read_bytes = sizeof(msg.hdr);

                state = rx_hdr;
                break;

            case rx_hdr:
                ret = CONNECTION::read(read_buff, read_bytes);
                if (ret == false) {
                    yield = true;
                    break;
                }

                read_buff = msg.body;
                read_bytes = msg.hdr.len;

                state = rx_body;
                break;

            case rx_body:
                ret = CONNECTION::read(read_buff, read_bytes);

                if (ret == false) {
                    yield = true;
                    break;
                }

#ifdef DEBUG
                print_msg((char*)"<");
#endif
                handle_msg();
#ifdef DEBUG
                print_msg((char*)">");
#endif

                state = p_tx;
                break;

            case p_tx:
                write_buff = reinterpret_cast<char*>((&msg.hdr));
                write_bytes = sizeof(msg.hdr);

                state = tx_hdr;
                break;

            case tx_hdr:
                ret = CONNECTION::write(write_buff, write_bytes);

                if (ret == false) {
                    yield = true;
                    break;
                }

                write_buff = msg.body;
                write_bytes = msg.hdr.len;

                state = tx_body;
                break;

            case tx_body:
                ret = CONNECTION::write(write_buff, write_bytes);

                if (ret == false) {
                    yield = true;
                    break;
                }

                state = p_watch;
                break;

            case p_watch:
                if (fire_lst.empty()) {
                    state = p_rx;
                } else {
                    std::pair<std::string, watch_cb_k&>& e = fire_lst.front();

                    build_watch(e.first.c_str() + (e.second.rel ? msg.body - msg.abs_path : 0), e.second.token.c_str());
#ifdef DEBUG
                    print_msg((char*)">");
#endif

                    fire_lst.pop_front();
                    state = p_tx;
                }
                break;
        }
    }
}

template < typename CONNECTION >
bool client<CONNECTION>::read(char*& buff, int& bytes)
{
    return CONNECTION::read(buff, bytes);
}

template < typename CONNECTION >
bool client<CONNECTION>::write(char*& buff, int& bytes)
{
    return CONNECTION::write(buff, bytes);
}

} /* namespace lixs */

#endif /* __LIXS_CLIENT_HH__ */

