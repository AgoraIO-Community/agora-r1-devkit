armino_build_get_property(armino_soc ARMINO_SOC)
if(NOT armino_soc STREQUAL "bk7258_ap")
    armino_component_register()
    return()
endif()

# Keep the exact source selection and compile configuration of the BK7258
# integration captured in snapshots.json.
set(aosl_sources
    aosl/kernel/file.c
    aosl/kernel/fileobj.c
    aosl/kernel/iofd.c
    aosl/kernel/log.c
    aosl/kernel/mpq.c
    aosl/kernel/mpqp.c
    aosl/kernel/osmp.c
    aosl/kernel/panic.c
    aosl/kernel/refobj.c
    aosl/kernel/thread.c
    aosl/kernel/time.c
    aosl/kernel/timer.c
    aosl/kernel/version.c
    aosl/kernel/select_mp.c
    aosl/kernel/poll_mp.c
    aosl/kernel/et_mp.c
    aosl/kernel/rt_monitor.c
    aosl/lib/atomic.c
    aosl/lib/bitmap.c
    aosl/lib/byteswap.c
    aosl/lib/cond.c
    aosl/lib/errno.c
    aosl/lib/marshalling-base-obj.c
    aosl/lib/marshalling.c
    aosl/lib/psbuff.c
    aosl/lib/rbtree.c
    aosl/lib/rwlock.c
    aosl/lib/rwlockimpl.c
    aosl/lib/thread_api.c
    aosl/lib/tls.c
    aosl/lib/utils.c
    aosl/mm/mm.c
    aosl/net/dns.c
    aosl/net/inet.c
    aosl/net/ipv6.c
    aosl/net/netifs.c
    aosl/net/route.c
    aosl/net/sk_utils.c
    aosl/platform/src/bk725x/aosl_hal_atomic.c
    aosl/platform/src/bk725x/aosl_hal_errno.c
    aosl/platform/src/bk725x/aosl_hal_file.c
    aosl/platform/src/bk725x/aosl_hal_iomp.c
    aosl/platform/src/bk725x/aosl_hal_log.c
    aosl/platform/src/bk725x/aosl_hal_memory.c
    aosl/platform/src/bk725x/aosl_hal_socket.c
    aosl/platform/src/bk725x/aosl_hal_thread.c
    aosl/platform/src/bk725x/aosl_hal_time.c
    aosl/platform/src/bk725x/aosl_hal_utils.c
)

armino_component_register(
    SRCS "${aosl_sources}"
    INCLUDE_DIRS aosl/include aosl/platform/include aosl/platform/src/bk725x/config
    PRIV_REQUIRES bk_common lwip_intf_v2_1 bk_vfs bk_rtos
)
