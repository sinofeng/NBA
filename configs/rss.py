#! /usr/bin/env python3
import nshader, os
import sys

for netdev in nshader.get_netdevices():
    print(netdev)
for coproc in nshader.get_coprocessors():
    print(coproc)
node_cpus = nshader.get_cpu_node_mapping()
for node_id, cpus in enumerate(node_cpus):
    print('Cores in NUMA node {0}: [{1}]'.format(node_id, ', '.join(map(str, cpus))))

# The values read by the framework are:
# - system_params
# - io_threads
# - comp_threads
# - coproc_threads
# - queues
# - thread_connections

# 64, 64, 192 is optimal parameter for ipv4-router

system_params = {
    'IO_BATCH_SIZE': int(os.environ.get('NSHADER_IO_BATCH_SIZE', 64)),
    'COMP_BATCH_SIZE': int(os.environ.get('NSHADER_COMP_BATCH_SIZE', 64)),
    'COPROC_PPDEPTH': int(os.environ.get('NSHADER_COPROC_PPDEPTH', 32)),
    'COPROC_CTX_PER_COMPTHREAD': 1,
}
print("# logical cores: {0}, # physical cores {1} (hyperthreading {2})".format(
    nshader.num_logical_cores, nshader.num_physical_cores,
    "enabled" if nshader.ht_enabled else "disabled"
))
_ht_diff = nshader.num_physical_cores if nshader.ht_enabled else 0

io_threads = [
    # core_id, list of (port_id, rxq_idx)
    nshader.IOThread(core_id=node_cpus[0][0], attached_rxqs=[(0, 0), (1, 0), (2, 0), (3, 0)], mode='normal'),
    nshader.IOThread(core_id=node_cpus[0][1], attached_rxqs=[(0, 1), (1, 1), (2, 1), (3, 1)], mode='normal'),
    nshader.IOThread(core_id=node_cpus[0][2], attached_rxqs=[(0, 2), (1, 2), (2, 2), (3, 2)], mode='normal'),
    nshader.IOThread(core_id=node_cpus[0][3], attached_rxqs=[(0, 3), (1, 3), (2, 3), (3, 3)], mode='normal'),
    nshader.IOThread(core_id=node_cpus[0][4], attached_rxqs=[(0, 4), (1, 4), (2, 4), (3, 4)], mode='normal'),
    nshader.IOThread(core_id=node_cpus[0][5], attached_rxqs=[(0, 5), (1, 5), (2, 5), (3, 5)], mode='normal'),
    nshader.IOThread(core_id=node_cpus[0][6], attached_rxqs=[(0, 6), (1, 6), (2, 6), (3, 6)], mode='normal'),
    nshader.IOThread(core_id=node_cpus[1][0], attached_rxqs=[(4, 0), (5, 0), (6, 0), (7, 0)], mode='normal'),
    nshader.IOThread(core_id=node_cpus[1][1], attached_rxqs=[(4, 1), (5, 1), (6, 1), (7, 1)], mode='normal'),
    nshader.IOThread(core_id=node_cpus[1][2], attached_rxqs=[(4, 2), (5, 2), (6, 2), (7, 2)], mode='normal'),
    nshader.IOThread(core_id=node_cpus[1][3], attached_rxqs=[(4, 3), (5, 3), (6, 3), (7, 3)], mode='normal'),
    nshader.IOThread(core_id=node_cpus[1][4], attached_rxqs=[(4, 4), (5, 4), (6, 4), (7, 4)], mode='normal'),
    nshader.IOThread(core_id=node_cpus[1][5], attached_rxqs=[(4, 5), (5, 5), (6, 5), (7, 5)], mode='normal'),
    nshader.IOThread(core_id=node_cpus[1][6], attached_rxqs=[(4, 6), (5, 6), (6, 6), (7, 6)], mode='normal'),
]
comp_threads = [
    # core_id
    nshader.CompThread(core_id=node_cpus[0][0] + _ht_diff),
    nshader.CompThread(core_id=node_cpus[0][1] + _ht_diff),
    nshader.CompThread(core_id=node_cpus[0][2] + _ht_diff),
    nshader.CompThread(core_id=node_cpus[0][3] + _ht_diff),
    nshader.CompThread(core_id=node_cpus[0][4] + _ht_diff),
    nshader.CompThread(core_id=node_cpus[0][5] + _ht_diff),
    nshader.CompThread(core_id=node_cpus[0][6] + _ht_diff),
    nshader.CompThread(core_id=node_cpus[1][0] + _ht_diff),
    nshader.CompThread(core_id=node_cpus[1][1] + _ht_diff),
    nshader.CompThread(core_id=node_cpus[1][2] + _ht_diff),
    nshader.CompThread(core_id=node_cpus[1][3] + _ht_diff),
    nshader.CompThread(core_id=node_cpus[1][4] + _ht_diff),
    nshader.CompThread(core_id=node_cpus[1][5] + _ht_diff),
    nshader.CompThread(core_id=node_cpus[1][6] + _ht_diff),
]

coproc_threads = [
    # core_id, device_id
    nshader.CoprocThread(core_id=node_cpus[0][7] + _ht_diff, device_id=0),
    nshader.CoprocThread(core_id=node_cpus[1][7] + _ht_diff, device_id=1),
]

comp_input_queues = [
    # node_id, template
    nshader.Queue(node_id=0, template='swrx'),
    nshader.Queue(node_id=0, template='swrx'),
    nshader.Queue(node_id=0, template='swrx'),
    nshader.Queue(node_id=0, template='swrx'),
    nshader.Queue(node_id=0, template='swrx'),
    nshader.Queue(node_id=0, template='swrx'),
    nshader.Queue(node_id=0, template='swrx'),
    nshader.Queue(node_id=1, template='swrx'),
    nshader.Queue(node_id=1, template='swrx'),
    nshader.Queue(node_id=1, template='swrx'),
    nshader.Queue(node_id=1, template='swrx'),
    nshader.Queue(node_id=1, template='swrx'),
    nshader.Queue(node_id=1, template='swrx'),
    nshader.Queue(node_id=1, template='swrx'),
]

coproc_input_queues = [
    # node_id, template
    nshader.Queue(node_id=0, template='taskin'),
    nshader.Queue(node_id=1, template='taskin'),
]

coproc_completion_queues = [
    # node_id, template
    nshader.Queue(node_id=0, template='taskout'),
    nshader.Queue(node_id=0, template='taskout'),
    nshader.Queue(node_id=0, template='taskout'),
    nshader.Queue(node_id=0, template='taskout'),
    nshader.Queue(node_id=0, template='taskout'),
    nshader.Queue(node_id=0, template='taskout'),
    nshader.Queue(node_id=0, template='taskout'),
    nshader.Queue(node_id=1, template='taskout'),
    nshader.Queue(node_id=1, template='taskout'),
    nshader.Queue(node_id=1, template='taskout'),
    nshader.Queue(node_id=1, template='taskout'),
    nshader.Queue(node_id=1, template='taskout'),
    nshader.Queue(node_id=1, template='taskout'),
    nshader.Queue(node_id=1, template='taskout'),
]

queues = comp_input_queues + coproc_input_queues + coproc_completion_queues

thread_connections = [
    # from-thread, to-thread, queue-instance
    (io_threads[0], comp_threads[0], comp_input_queues[0]),
    (io_threads[1], comp_threads[1], comp_input_queues[1]),
    (io_threads[2], comp_threads[2], comp_input_queues[2]),
    (io_threads[3], comp_threads[3], comp_input_queues[3]),
    (io_threads[4], comp_threads[4], comp_input_queues[4]),
    (io_threads[5], comp_threads[5], comp_input_queues[5]),
    (io_threads[6], comp_threads[6], comp_input_queues[6]),
    (io_threads[7], comp_threads[7], comp_input_queues[7]),
    (io_threads[8], comp_threads[8], comp_input_queues[8]),
    (io_threads[9], comp_threads[9], comp_input_queues[9]),
    (io_threads[10], comp_threads[10], comp_input_queues[10]),
    (io_threads[11], comp_threads[11], comp_input_queues[11]),
    (io_threads[12], comp_threads[12], comp_input_queues[12]),
    (io_threads[13], comp_threads[13], comp_input_queues[13]),
    (comp_threads[0], coproc_threads[0], coproc_input_queues[0]),
    (comp_threads[1], coproc_threads[0], coproc_input_queues[0]),
    (comp_threads[2], coproc_threads[0], coproc_input_queues[0]),
    (comp_threads[3], coproc_threads[0], coproc_input_queues[0]),
    (comp_threads[4], coproc_threads[0], coproc_input_queues[0]),
    (comp_threads[5], coproc_threads[0], coproc_input_queues[0]),
    (comp_threads[6], coproc_threads[0], coproc_input_queues[0]),
    (comp_threads[7], coproc_threads[1], coproc_input_queues[1]),
    (comp_threads[8], coproc_threads[1], coproc_input_queues[1]),
    (comp_threads[9], coproc_threads[1], coproc_input_queues[1]),
    (comp_threads[10], coproc_threads[1], coproc_input_queues[1]),
    (comp_threads[11], coproc_threads[1], coproc_input_queues[1]),
    (comp_threads[12], coproc_threads[1], coproc_input_queues[1]),
    (comp_threads[13], coproc_threads[1], coproc_input_queues[1]),
    (coproc_threads[0], comp_threads[0], coproc_completion_queues[0]),
    (coproc_threads[0], comp_threads[1], coproc_completion_queues[1]),
    (coproc_threads[0], comp_threads[2], coproc_completion_queues[2]),
    (coproc_threads[0], comp_threads[3], coproc_completion_queues[3]),
    (coproc_threads[0], comp_threads[4], coproc_completion_queues[4]),
    (coproc_threads[0], comp_threads[5], coproc_completion_queues[5]),
    (coproc_threads[0], comp_threads[6], coproc_completion_queues[6]),
    (coproc_threads[1], comp_threads[7], coproc_completion_queues[7]),
    (coproc_threads[1], comp_threads[8], coproc_completion_queues[8]),
    (coproc_threads[1], comp_threads[9], coproc_completion_queues[9]),
    (coproc_threads[1], comp_threads[10], coproc_completion_queues[10]),
    (coproc_threads[1], comp_threads[11], coproc_completion_queues[11]),
    (coproc_threads[1], comp_threads[12], coproc_completion_queues[12]),
    (coproc_threads[1], comp_threads[13], coproc_completion_queues[13]),
]
