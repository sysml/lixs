#ifndef __LIXS_VIRQ_HANDLER_HH__
#define __LIXS_VIRQ_HANDLER_HH__

#include <lixs/domain_mgr.hh>
#include <lixs/iomux.hh>
#include <lixs/xenstore.hh>

#include <cerrno>

extern "C" {
#include <xenctrl.h>
}


namespace lixs {

class virq_handler {
public:
    virq_handler(xenstore& xs, domain_mgr& dmgr, iomux& io);
    virtual ~virq_handler();

    void callback(bool read, bool write);

private:
    xenstore& xs;
    domain_mgr& dmgr;
    iomux& io;

    int fd;

    xc_interface* xc_handle;
    xc_evtchn *xce_handle;
    evtchn_port_t virq_port;
};

} /* namespace lixs */

#endif /* __LIXS_VIRQ_HANDLER_HH__ */

