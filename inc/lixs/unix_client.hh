#ifndef __LIXS_UNIX_CLIENT_HH__
#define __LIXS_UNIX_CLIENT_HH__

#include <lixs/client.hh>
#include <lixs/xenstore.hh>


namespace lixs {

class unix_client : public client {
public:
    static void create(xenstore& xs, int fd);

private:
    unix_client(xenstore& xs, int fd);
    ~unix_client();

    bool read(char*& buff, int& bytes);
    bool write(char*& buff, int& bytes);

private:
    int fd;

    fd_cb::fd_ev events;
};

} /* namespace lixs */

#endif /* __LIXS_UNIX_CLIENT_HH__ */

