// File		:proxyfs-socket.c
// Author	:Victor Kovalevich
// Created	:Fri Jul 11 13:07:00 2025
#include <linux/netlink.h>
#include "proxyfs.h"

//
// NETLINK receive message callback
static void proxyfs_socket_recv_msg(struct sk_buff *sk_buffer)
{
    struct nlmsghdr* nl_header;

    ///// //
    ///// // Note: following code can be used for minimal access restrictions
    ///// const struct cred* cred = NETLINK_CB(sk_buffer).sk ? NETLINK_CB(sk_buffer).sk->sk_socket->file->f_cred : NULL;
    ///// if (!cap_raised(cred->cap_effective, CAP_NET_ADMIN)) {
    /////     pr_warn("Access denied: no CAP_NET_ADMIN capability\n");
    /////     return;
    ///// }

    nl_header = (struct nlmsghdr*)sk_buffer->data;
    proxyfs_context_set_client_pid(nl_header->nlmsg_pid);
    pr_info("%s: Registered client PID=%d\n",
            MODULE_NAME,
            proxyfs_context_get_client_pid());
    if (nl_header->nlmsg_len > 0 && nlmsg_data(nl_header)) {
        pr_info("%s: Registration message: %s\n",
                MODULE_NAME,
                (char*)nlmsg_data(nl_header));
    }
}

struct sock* proxyfs_socket_init(const int nl_unit_id)
{
    struct netlink_kernel_cfg nl_cfg = {
        .input = proxyfs_socket_recv_msg,
        .flags = 0,
        .groups = 0,
    };
    struct sock* nl_socket = NULL;
    if ((nl_socket = netlink_kernel_create(&init_net, nl_unit_id, &nl_cfg)) == NULL) {
        pr_err("%s: unable to create a Netlink socket with [%d] unit\n",
               MODULE_NAME,
               nl_unit_id);
    } else {
        pr_info("%s: netlink_kernel_create() with [%d] unit\n",
                MODULE_NAME,
                nl_unit_id);
    }

    return nl_socket;
}

void proxyfs_socket_release(struct sock* nl_socket)
{
    if (nl_socket == NULL) {
        return;
    }

    netlink_kernel_release(nl_socket);

    pr_info("%s: netlink_kernel_release()\n",
            MODULE_NAME);
}

// Sending of a message to the client (if it is registered)
void proxyfs_socket_send_msg(const char* msg_body, size_t msg_len)
{
    struct sk_buff* sk_buffer_out;
    struct nlmsghdr* nl_header;
    int res;

    //
    // Note: NETLINK socket supports multythread access to send messages
    //       thus no any extra synchronizations are required
    if (proxyfs_context_get_nl_socket() == NULL ||
        proxyfs_context_get_client_pid() <= 0) {
        return;
    }

    struct pid* pid_struct = find_vpid(proxyfs_context_get_client_pid());
    struct task_struct* task = pid_struct ? get_pid_task(pid_struct, PIDTYPE_PID) : NULL;

    if (task) {
        do {
            //
            // NOTE: message should be released within `nlmsg_unicast` even
            //       an issue occurs
            if ((sk_buffer_out = nlmsg_new(msg_len, GFP_KERNEL)) == NULL) {
                pr_err("%s: Failed to allocate sk_buffer_out\n",
                       MODULE_NAME);
                break;
            }
            nl_header = nlmsg_put(sk_buffer_out, 0, 0, NLMSG_DONE, msg_len, 0);
            memcpy(nlmsg_data(nl_header), msg_body, msg_len);

            // Unicast the message to the client by using its PID (client_pid)
            if ((res = nlmsg_unicast(proxyfs_context_get_nl_socket(),
                                     sk_buffer_out,
                                     proxyfs_context_get_client_pid())) < 0) {
                pr_err("%s: Error sending to user %d due to the issue: %d, "
                       "connection is closed ",
                       MODULE_NAME,
                       proxyfs_context_get_client_pid(),
                       res);
                proxyfs_context_set_client_pid(0);
            }
        } while (false);
        put_task_struct(task);
    } else {
        pr_warn("%s: The process with PID %d does not exist, "
                "the connection is closed forcibly, unable to send the message %s",
                MODULE_NAME,
                proxyfs_context_get_client_pid(),
                msg_body);
        proxyfs_context_set_client_pid(0);
    }
}
