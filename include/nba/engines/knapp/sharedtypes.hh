#ifndef __NBA_KNAPP_SHAREDTYPES_HH__
#define __NBA_KNAPP_SHAREDTYPES_HH__

#include <cstdint>
#include <scif.h>
#include <nba/engines/knapp/defs.hh>
#ifdef __MIC__
#include <nba/engines/knapp/micintrinsic.hh>
#else
#include <nba/core/intrinsic.hh>
#endif

namespace nba { namespace knapp {

struct taskitem {
    uint32_t task_id;
    int32_t kernel_id;
    uint32_t num_items;
    uint32_t num_kernel_args;
    void *args[KNAPP_MAX_KERNEL_ARGS];
} __cache_aligned;

struct d2hcopy {
    uint32_t buffer_id;
    uint32_t offset;
    uint32_t size;
} __cache_aligned;

}} // endns(nba::knapp)

#endif // __NBA_KNAPP_SHAREDTYPES_HH__

// vim: ts=8 sts=4 sw=4 et
