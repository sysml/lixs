#include <lixs/xenbus.hh>

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

extern "C" {
#include <xenctrl.h>
#include <xen/grant_table.h>
}


const std::string lixs::xenbus_mapper::xsd_kva_path = "/proc/xen/xsd_kva";

lixs::xenbus_mapper::xenbus_mapper(domid_t domid)
{
    int fd;

    fd = open(xsd_kva_path.c_str(), O_RDWR);

    interface = (xenstore_domain_interface*) mmap(
            NULL, getpagesize(), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);

    close(fd);
}

lixs::xenbus_mapper::~xenbus_mapper(void)
{
    munmap(interface, getpagesize());
    interface = NULL;
}


const std::string lixs::xenbus::xsd_port_path = "/proc/xen/xsd_port";

lixs::xenbus::xenbus(xenstore& xs, event_mgr& emgr)
    : client(xs, emgr, 0, xenbus_evtchn())
{
#ifdef DEBUG
    asprintf(&cid, "D%d", 0);
    printf("%4s = new conn\n", cid);
#endif

    xs.get_domain_path(0, abs_path);
    body = abs_path + strlen(abs_path);
    *body++ = '/';
}

lixs::xenbus::~xenbus()
{
#ifdef DEBUG
    printf("%4s = closed conn\n", cid);
    free(cid);
#endif
}

evtchn_port_t lixs::xenbus::xenbus_evtchn(void)
{
    evtchn_port_t port;

    std::ifstream file(xsd_port_path.c_str());
    file >> port;

    return port;
}
