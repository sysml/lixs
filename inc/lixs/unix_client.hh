#ifndef __LIXS_UNIX_CLIENT_HH__
#define __LIXS_UNIX_CLIENT_HH__

#include <lixs/client.hh>
#include <lixs/iomux.hh>
#include <lixs/xenstore.hh>


namespace lixs {

class unix_client : public client {
public:
    static void create(iomux& io, xenstore& xs, int fd);

private:
    unix_client(iomux& io, xenstore& xs, int fd);
    ~unix_client();

    bool read(char*& buff, int& bytes);
    bool write(char*& buff, int& bytes);

private:
    int fd;

    iokfd::ioev events;
};

} /* namespace lixs */

#endif /* __LIXS_UNIX_CLIENT_HH__ */

