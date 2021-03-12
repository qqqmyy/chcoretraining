#pragma once

#include "chcore/container/hashtable.h"
#include "chcore/ipc.h"
#include <termios.h>
#include <chcore/lwip_defs.h>

#include "poll.h"

#define MAX_FD  40960
#define MIN_FD  0

//fd:进程特有的文件描述符表的索引

/* Type of fd */
enum fd_type {
	FD_TYPE_FILE = 0,//文件
	FD_TYPE_PIPE,//管道
	FD_TYPE_SOCK,//socket
	FD_TYPE_STDIN,//输入
	FD_TYPE_STDOUT,//输出
	FD_TYPE_STDERR,//标准错误输出
	FD_TYPE_EVENT,//事件
	FD_TYPE_TIMER,//计时器
	FD_TYPE_EPOLL,//可扩展I/O事件通知机制
};

struct fd_ops {
	int (*read) (int fd, void *buf, size_t count);//读
	int (*write) (int fd, void *buf, size_t count);//写
	int (*close) (int fd);//关闭
	int (*poll) (int fd, struct pollarg *arg);//监视并等待多个文件描述符的属性变化
	int (*ioctl) (int fd, unsigned long request, void *arg);//设备控制接口函数（文件描述符，交互协议，可变参数）
};

extern struct fd_ops epoll_ops;
extern struct fd_ops socket_ops;
extern struct fd_ops file_ops;
extern struct fd_ops event_op;
extern struct fd_ops timer_op;
extern struct fd_ops pipe_op;
extern struct fd_ops stdin_ops;
extern struct fd_ops stdout_ops;
extern struct fd_ops stderr_ops;

/*
 * Each fd will have a fd structure `fd_desc` which can be found from
 * the `fd_dic`. `fd_desc` structure contains the basic information of
 * the fd.
 */
 //每一个文件描述符fd都有一个fd结构"fd_desc"  fd_dic,fd_desc结构包括文件描述符fd的基础信息
struct fd_desc {
	/* Identification used by corresponding service 对应设备的标识号*/
	union {
		int conn_id;//连接id
		int fd;//文件描述符id
	};
	/* Baisc informantion of fd     fd的基本信息*/
	int flags;		/* Flags of the file    文件的flag*/
	int cap;		/* Service's cap of fd, 0 if no service     如果没有设备就是0*/
	enum fd_type type;	/* Type for debug use   用来debug的类型*/
	struct fd_ops *fd_op;
	/* Private data of fd   文件描述符fd的私有数据*/

	/* stored termios */
	struct termios termios;

	void *private_data;//私有数据
};

extern struct fd_desc *fd_dic[MAX_FD];
extern struct htable fs_cap_struct_htable;

struct fs_fast_path {
	ipc_struct_t *_fs_ipc_struct;
	int fid;
};

struct fs_cap_struct {
	int fsm_cap;
	ipc_struct_t *_fs_ipc_struct;
	struct hlist_node node;
};

/* fd */
int alloc_fd(void);
int alloc_fd_since(int min);
void free_fd(int fd);

/* fd operation */
int chcore_read(int fd, void *buf, size_t count);
int chcore_write(int fd, void *buf, size_t count);
int chcore_close(int fd);
int chcore_ioctl(int fd, unsigned long request, void *arg);
int chcore_readv(int fd, const struct iovec *iov, int iovcnt);
int chcore_writev(int fd, const struct iovec *iov, int iovcnt);

ipc_struct_t *cap2struct(int key, int cap);
