#include "CheckIPHeader.hh"
#include <nba/core/checksum.hh>
#include <nba/framework/logging.hh>
#include <rte_ether.h>
#include <netinet/in.h>
#include <netinet/ip.h>

using namespace std;
using namespace nba;

int CheckIPHeader::initialize()
{
    // TODO: Depending on configuration,
    //       make a rx-to-tx address mapping or perform echo-back.
    //       The current default is echo-back.
    return 0;
}

int CheckIPHeader::configure(comp_thread_context *ctx, std::vector<std::string> &args)
{
    Element::configure(ctx, args);
    return 0;
}

int CheckIPHeader::process(int input_port, Packet *pkt)
{
    struct ether_hdr *ethh = (struct ether_hdr *) pkt->data();
    struct iphdr *iph = (struct iphdr *)(ethh + 1);

    if (ntohs(ethh->ether_type) != ETHER_TYPE_IPv4) {
        RTE_LOG(DEBUG, ELEM, "CheckIPHeader: invalid packet type - %x\n", ntohs(ethh->ether_type));
        pkt->kill();
        return 0;
    }

    if ( (iph->version != 4) || (iph->ihl < 5) ) {
        RTE_LOG(DEBUG, ELEM, "CheckIPHeader: invalid packet - ver %d, ihl %d\n", iph->version, iph->ihl);
        pkt->kill();
        return 0;
        // return SLOWPATH;
    }

    if ( (iph->ihl * 4) > ntohs(iph->tot_len)) {
        RTE_LOG(DEBUG, ELEM, "CheckIPHeader: invalid packet - total len %d, ihl %d\n", iph->tot_len, iph->ihl);
        pkt->kill();
        return 0;
        // return SLOWPATH;
    }

    // TODO: Discard illegal source addresses.

    if (ip_fast_csum(iph, iph->ihl) != 0) {
        pkt->kill();
        return 0;
    }

    output(0).push(pkt);
    return 0;
}

// vim: ts=8 sts=4 sw=4 et
