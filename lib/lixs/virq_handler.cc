#include <lixs/domain.hh>
#include <lixs/domain_mgr.hh>
#include <lixs/iomux.hh>
#include <lixs/virq_handler.hh>
#include <lixs/xenstore.hh>

#include <list>


lixs::virq_handler::virq_handler(xenstore& xs, domain_mgr& dmgr, iomux& io)
    : xs(xs), dmgr(dmgr), io(io)
{
    xc_handle = xc_interface_open(NULL, NULL, 0);
    xce_handle = xc_evtchn_open(NULL, 0);

    virq_port = xc_evtchn_bind_virq(xce_handle, VIRQ_DOM_EXC);

    fd = xc_evtchn_fd(xce_handle);
    ev_read = true;
    ev_write = false;
    io.add(*this);
}

lixs::virq_handler::~virq_handler()
{
    io.remove(*this);
    xc_evtchn_close(xce_handle);
    xc_interface_close(xc_handle);
}

void lixs::virq_handler::operator()(bool ev_read, bool ev_write)
{
    int ret;
    xc_dominfo_t dominfo;
    std::list<domid_t> dead_list;
    std::list<domid_t> dying_list;

    domain_mgr::iterator dom_it;
    std::list<domid_t>::iterator id_it;

    for (dom_it = dmgr.begin(); dom_it != dmgr.end(); dom_it++) {
        bool active = dom_it->second->is_active();
        domid_t domid = dom_it->second->get_domid();

        ret = xc_domain_getinfo(xc_handle, domid, 1, &dominfo);
        if (ret != 1 || domid != dominfo.domid) {
            /* Domain doesn't exist already but is still in our list: remove */
            dead_list.push_back(domid);
        } else if (dominfo.dying) {
            /* Domain is dying: remove */
            dead_list.push_back(domid);
        } else if (active && (dominfo.shutdown || dominfo.crashed)) {
            dom_it->second->set_inactive();
            dying_list.push_back(domid);
        }
    }

    for (id_it = dead_list.begin(); id_it != dead_list.end(); id_it++) {
        if (dmgr.destroy(*id_it) == 0) {
            xs.domain_release(*id_it);
        }
    }

    for (id_it = dying_list.begin(); id_it != dying_list.end(); id_it++) {
        xs.domain_release(*id_it);
    }

    evtchn_port_t port = xc_evtchn_pending(xce_handle);
    xc_evtchn_unmask(xce_handle, port);
}

