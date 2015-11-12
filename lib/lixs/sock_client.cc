#include <lixs/domain_mgr.hh>
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


unsigned int lixs::sock_client::next_id = 0;

/* FIXME: What is the correct domid when running in a stub domain? */
lixs::sock_client::sock_client(std::function<void(sock_client*)> dead_cb,
        xenstore& xs, domain_mgr& dmgr, event_mgr& emgr, iomux& io, int fd)
    : client(get_id(), 0, xs, dmgr, io, fd), emgr(emgr), dead_cb(dead_cb)
{
}

lixs::sock_client::~sock_client()
{
}

void lixs::sock_client::conn_dead(void)
{
    emgr.enqueue_event(std::bind(dead_cb, this));
}

std::string lixs::sock_client::get_id(void)
{
    return "S" + std::to_string(next_id++);
}

