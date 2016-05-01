#include <nba/core/intrinsic.hh>
#include <nba/engines/knapp/defs.hh>
#include <nba/engines/knapp/types.hh>
#include <nba/engines/knapp/utils.hh>
#include <nba/engines/knapp/computecontext.hh>
#include <rte_memzone.h>
#include <unistd.h>
#include <scif.h>

using namespace std;
using namespace nba;

struct cuda_event_context {
    ComputeContext *computectx;
    void (*callback)(ComputeContext *ctx, void *user_arg);
    void *user_arg;
};

#define IO_BASE_SIZE (16 * 1024 * 1024)
#define IO_MEMPOOL_ALIGN (8lu)
#undef USE_PHYS_CONT_MEMORY // performance degraded :(

KnappComputeContext::KnappComputeContext(unsigned ctx_id, ComputeDevice *mother)
 : ComputeContext(ctx_id, mother), checkbits_d(NULL), checkbits_h(NULL),
   mz(reserve_memory(mother)), num_kernel_args(0)
   /* NOTE: Write-combined memory degrades performance to half... */
{
    type_name = "knapp";
    size_t io_base_size = ALIGN_CEIL(IO_BASE_SIZE, getpagesize());
    int rc;

    /* Initialize Knapp vDev Parameters. */
    vdev.device_id = ctx_id;
    vdev.ht_per_core = 4;  // FIXME: retrieve from scif
    vdev.pipeline_depth = 32;
    vdev.next_poll = 0;

    vdev.data_epd = scif_open();
    if (vdev.data_epd == SCIF_OPEN_FAILED)
        rte_exit(EXIT_FAILURE, "scif_open() for data_epd failed.");
    vdev.ctrl_epd = scif_open();
    if (vdev.ctrl_epd == SCIF_OPEN_FAILED)
        rte_exit(EXIT_FAILURE, "scif_open() for ctrl_epd failed.");
    vdev.local_dataport.node = knapp::local_node;
    vdev.local_dataport.port = knapp::get_host_dataport(ctx_id);
    vdev.local_ctrlport.node = knapp::local_node;
    vdev.local_ctrlport.port = knapp::get_host_ctrlport(ctx_id);
    vdev.remote_ctrlport.node = knapp::remote_scif_nodes[0];
    vdev.remote_ctrlport.port = knapp::get_mic_dataport(ctx_id);
    vdev.remote_ctrlport.node = knapp::remote_scif_nodes[0];
    vdev.remote_ctrlport.port = knapp::get_mic_ctrlport(ctx_id);
    rc = scif_bind(vdev.data_epd, vdev.local_dataport.port);
    assert(rc == vdev.local_dataport.port);
    rc = scif_bind(vdev.ctrl_epd, vdev.local_ctrlport.port);
    assert(rc == vdev.local_ctrlport.port);
    knapp::connect_with_retry(&vdev);

    vdev.ctrlbuf = (uint8_t *) rte_zmalloc_socket(nullptr,
            KNAPP_OFFLOAD_CTRLBUF_SIZE,
            CACHE_LINE_SIZE, node_id);
    assert(vdev.ctrlbuf != nullptr);
    vdev.tasks_in_flight = (struct knapp::offload_task *) rte_zmalloc_socket(nullptr,
            sizeof(struct knapp::offload_task) * vdev.pipeline_depth,
            CACHE_LINE_SIZE, node_id);
    assert(vdev.tasks_in_flight != nullptr);

    NEW(node_id, io_base_ring, FixedRing<unsigned>,
        NBA_MAX_IO_BASES, node_id);
    for (unsigned i = 0; i < NBA_MAX_IO_BASES; i++) {
        io_base_ring->push_back(i);
        //NEW(node_id, _cuda_mempool_in[i], KnappMemoryPool, io_base_size, IO_MEMPOOL_ALIGN);
        //NEW(node_id, _cuda_mempool_out[i], KnappMemoryPool, io_base_size, IO_MEMPOOL_ALIGN);
        //_cuda_mempool_in[i]->init();
        //_cuda_mempool_out[i]->init();
        NEW(node_id, _cpu_mempool_in[i], CPUMemoryPool, io_base_size, IO_MEMPOOL_ALIGN, 0);
        NEW(node_id, _cpu_mempool_out[i], CPUMemoryPool, io_base_size, IO_MEMPOOL_ALIGN, 0);
        #ifdef USE_PHYS_CONT_MEMORY
        void *base;
        base = (void *) ((uintptr_t) mz->addr + i * io_base_size);
        _cpu_mempool_in[i]->init_with_flags(base, 0);
        base = (void *) ((uintptr_t) mz->addr + i * io_base_size + NBA_MAX_IO_BASES * io_base_size);
        _cpu_mempool_out[i]->init_with_flags(base, 0);
        #else
        //_cpu_mempool_in[i]->init_with_flags(nullptr, cudaHostAllocPortable);
        //_cpu_mempool_out[i]->init_with_flags(nullptr, cudaHostAllocPortable);
        #endif
    }
    // TODO: replace wtih scif_register() & scif_mmap()
    //cutilSafeCall(cudaHostAlloc((void **) &checkbits_h, MAX_BLOCKS, cudaHostAllocMapped));
    //cutilSafeCall(cudaHostGetDevicePointer((void **) &checkbits_d, checkbits_h, 0));
    assert(checkbits_h != NULL);
    assert(checkbits_d != NULL);
    memset(checkbits_h, 0, MAX_BLOCKS);
}

const struct rte_memzone *KnappComputeContext::reserve_memory(ComputeDevice *mother)
{
#ifdef USE_PHYS_CONT_MEMORY
    char namebuf[RTE_MEMZONE_NAMESIZE];
    size_t io_base_size = ALIGN_CEIL(IO_BASE_SIZE, getpagesize());
    snprintf(namebuf, RTE_MEMZONE_NAMESIZE, "knapp.io.%d:%d", mother->device_id, ctx_id);
    const struct rte_memzone *_mz = rte_memzone_reserve(namebuf, 2 * io_base_size * NBA_MAX_IO_BASES,
                                                        mother->node_id,
                                                        RTE_MEMZONE_2MB | RTE_MEMZONE_SIZE_HINT_ONLY);
    assert(_mz != nullptr);
    return _mz;
#else
    return nullptr;
#endif
}

KnappComputeContext::~KnappComputeContext()
{
    //cutilSafeCall(cudaStreamDestroy(_stream));
    for (unsigned i = 0; i < NBA_MAX_IO_BASES; i++) {
        //_cuda_mempool_in[i]->destroy();
        //_cuda_mempool_out[i]->destroy();
        _cpu_mempool_in[i]->destroy();
        _cpu_mempool_out[i]->destroy();
    }
    if (mz != nullptr)
        rte_memzone_free(mz);
    scif_close(vdev.data_epd);
    scif_close(vdev.ctrl_epd);
    rte_free(vdev.ctrlbuf);
    rte_free(vdev.tasks_in_flight);
    //cutilSafeCall(cudaFreeHost(checkbits_h));
}

io_base_t KnappComputeContext::alloc_io_base()
{
    if (io_base_ring->empty()) return INVALID_IO_BASE;
    unsigned i = io_base_ring->front();
    io_base_ring->pop_front();
    return (io_base_t) i;
}

int KnappComputeContext::alloc_input_buffer(io_base_t io_base, size_t size,
                                           host_mem_t &host_mem, dev_mem_t &dev_mem)
{
    unsigned i = io_base;
    assert(0 == _cpu_mempool_in[i]->alloc(size, host_mem));
    //assert(0 == _cuda_mempool_in[i]->alloc(size, dev_mem));
    // for debugging
    //assert(((uintptr_t)host_mem.ptr & 0xffff) == ((uintptr_t)dev_mem.ptr & 0xffff));
    return 0;
}

int KnappComputeContext::alloc_output_buffer(io_base_t io_base, size_t size,
                                            host_mem_t &host_mem, dev_mem_t &dev_mem)
{
    unsigned i = io_base;
    assert(0 == _cpu_mempool_out[i]->alloc(size, host_mem));
    //assert(0 == _cuda_mempool_out[i]->alloc(size, dev_mem));
    // for debugging
    //assert(((uintptr_t)host_mem.ptr & 0xffff) == ((uintptr_t)dev_mem.ptr & 0xffff));
    return 0;
}

void KnappComputeContext::map_input_buffer(io_base_t io_base, size_t offset, size_t len,
                                          host_mem_t &hbuf, dev_mem_t &dbuf) const
{
    unsigned i = io_base;
    hbuf.ptr = (void *) ((uintptr_t) _cpu_mempool_in[i]->get_base_ptr().ptr + offset);
    //dbuf.ptr = (void *) ((uintptr_t) _cuda_mempool_in[i]->get_base_ptr().ptr + offset);
    // len is ignored.
}

void KnappComputeContext::map_output_buffer(io_base_t io_base, size_t offset, size_t len,
                                           host_mem_t &hbuf, dev_mem_t &dbuf) const
{
    unsigned i = io_base;
    hbuf.ptr = (void *) ((uintptr_t) _cpu_mempool_out[i]->get_base_ptr().ptr + offset);
    //dbuf.ptr = (void *) ((uintptr_t) _cuda_mempool_out[i]->get_base_ptr().ptr + offset);
    // len is ignored.
}

void *KnappComputeContext::unwrap_host_buffer(const host_mem_t hbuf) const
{
    return hbuf.ptr;
}

void *KnappComputeContext::unwrap_device_buffer(const dev_mem_t dbuf) const
{
    return dbuf.ptr;
}

size_t KnappComputeContext::get_input_size(io_base_t io_base) const
{
    unsigned i = io_base;
    return _cpu_mempool_in[i]->get_alloc_size();
}

size_t KnappComputeContext::get_output_size(io_base_t io_base) const
{
    unsigned i = io_base;
    return _cpu_mempool_out[i]->get_alloc_size();
}

void KnappComputeContext::clear_io_buffers(io_base_t io_base)
{
    unsigned i = io_base;
    _cpu_mempool_in[i]->reset();
    _cpu_mempool_out[i]->reset();
    //_cuda_mempool_in[i]->reset();
    //_cuda_mempool_out[i]->reset();
    io_base_ring->push_back(i);
}

int KnappComputeContext::enqueue_memwrite_op(const host_mem_t host_buf,
                                            const dev_mem_t dev_buf,
                                            size_t offset, size_t size)
{
    //cutilSafeCall(cudaMemcpyAsync(dev_buf.ptr, host_buf.ptr, size,
    //                              cudaMemcpyHostToDevice, _stream));
    return 0;
}

int KnappComputeContext::enqueue_memread_op(const host_mem_t host_buf,
                                           const dev_mem_t dev_buf,
                                           size_t offset, size_t size)
{
    //cutilSafeCall(cudaMemcpyAsync(host_buf.ptr, dev_buf.ptr, size,
    //                              cudaMemcpyDeviceToHost, _stream));
    return 0;
}

void KnappComputeContext::clear_kernel_args()
{
    num_kernel_args = 0;
}

void KnappComputeContext::push_kernel_arg(struct kernel_arg &arg)
{
    assert(num_kernel_args < KNAPP_MAX_KERNEL_ARGS);
    kernel_args[num_kernel_args ++] = arg;  /* Copied to the array. */
}

int KnappComputeContext::enqueue_kernel_launch(dev_kernel_t kernel, struct resource_param *res)
{
    assert(checkbits_d != nullptr);
    // TODO: considerations for cudaFuncSetCacheConfig() and
    //       cudaSetDoubleFor*()?
    //cudaFuncAttributes attr;
    //cudaFuncGetAttributes(&attr, kernel.ptr);
    if (unlikely(res->num_workgroups == 0))
        res->num_workgroups = 1;
    void *raw_args[num_kernel_args];
    for (unsigned i = 0; i < num_kernel_args; i++) {
        raw_args[i] = kernel_args[i].ptr;
    }
    state = ComputeContext::RUNNING;
    //cutilSafeCall(cudaLaunchKernel(kernel.ptr, dim3(res->num_workgroups),
    //                               dim3(res->num_threads_per_workgroup),
    //                               (void **) &raw_args[0], 1024, _stream));
    return 0;
}

int KnappComputeContext::enqueue_event_callback(
        void (*func_ptr)(ComputeContext *ctx, void *user_arg),
        void *user_arg)
{
    //auto cb = [](cudaStream_t stream, cudaError_t status, void *user_data)
    {
        //assert(status == cudaSuccess);
        //struct cuda_event_context *cectx = (struct cuda_event_context *) user_data;
        //cectx->callback(cectx->computectx, cectx->user_arg);
        //delete cectx;
    };
    // TODO: how to avoid using new/delete?
    //struct cuda_event_context *cectx = new struct cuda_event_context;
    //cectx->computectx = this;
    //cectx->callback = func_ptr;
    //cectx->user_arg = user_arg;
    //cutilSafeCall(cudaStreamAddCallback(_stream, cb, cectx, 0));
    return 0;
}


// vim: ts=8 sts=4 sw=4 et
