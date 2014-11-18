#include <lixs/domainU.hh>
#include <lixs/event_mgr.hh>
#include <lixs/xen_server.hh>

#include <map>
#include <utility>


lixs::xen_server::xen_server(xenstore& xs, event_mgr& emgr)
    : xs(xs), emgr(emgr)
{
    xs.set_xen_server(this);
}

lixs::xen_server::~xen_server(void)
{
}

void lixs::xen_server::create_domain(int domid, int port)
{
    domlist.insert(std::pair<int, lixs::domainU*>(domid, new domainU(xs, emgr, domid, port)));
}

void lixs::xen_server::destroy_domain(int domid)
{
    std::map<int, lixs::domainU*>::iterator it;

    it = domlist.find(domid);

    if (it != domlist.end()) {
        delete it->second;
        domlist.erase(it);
    }
}

