#include <lixs/sock_client.hh>
#include <lixs/xenstore.hh>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <memory>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>


lixs::sock_client::sock_client(std::function<void(sock_client*)> dead_cb,
        xenstore& xs, event_mgr& emgr, iomux& io, int fd)
    : client(get_id(fd), xs, emgr, io, fd), emgr(emgr), dead_cb(dead_cb)
{
#ifdef DEBUG
    printf("%4s = new conn\n", id.c_str());
#endif

    std::string path = "/local/domain/0/";

    memcpy(msg.abs_path, path.c_str(), path.length());
    msg.body = msg.abs_path + path.length();
}

lixs::sock_client::~sock_client()
{
#ifdef DEBUG
    printf("%4s = closed conn\n", id.c_str());
#endif
}

void lixs::sock_client::conn_dead(void)
{
    emgr.enqueue_event(std::bind(dead_cb, this));
}

std::string lixs::sock_client::get_id(int id)
{
    return "S" + std::to_string(id);
}

