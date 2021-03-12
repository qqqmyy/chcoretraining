#include <atomic.h>
#include <stdlib.h>
#include <string.h>

#include "fd.h"
#include <chcore/bug.h>
#include <limits.h>

/* 
 * No lock is used to protect fd_dic and its member
 * since we only modify the fd_desc when init by far ... 
 */
/* Global fd desc table */
struct fd_desc *fd_dic[MAX_FD] = {0};
struct htable fs_cap_struct_htable;
/* Default fd operations */
struct fd_ops default_ops;

/*
 * Helper function.
 */
static int iov_check(const struct iovec *iov, int iovcnt)
{
	int iov_i;
	size_t iov_len_sum;

	if (iovcnt <= 0 || iovcnt > IOV_MAX)
		return -EINVAL;

	iov_len_sum = 0;
	for (iov_i = 0; iov_i < iovcnt; iov_i++)
		iov_len_sum += (iov + iov_i)->iov_len;

	if (iov_len_sum > _POSIX_SSIZE_MAX)
		return -EINVAL;

	return 0;
}


int alloc_fd(void) { return alloc_fd_since(0); }

int alloc_fd_since(int min)
{
	int cur_fd;
	struct fd_desc *new_desc;

	/* Malloc fd_desc structure */
	new_desc = malloc(sizeof(struct fd_desc));
	/* Init fd_desc structure */
	memset(new_desc, 0, sizeof(struct fd_desc));
	/* Set default operation */
	new_desc->fd_op = &default_ops;

	/* XXX: performance */
	cur_fd = MIN_FD > min ? MIN_FD : min;
	for (; cur_fd < MAX_FD; cur_fd++) {
		if (!fd_dic[cur_fd] &&
		    a_cas_p((void *)&fd_dic[cur_fd], 0, new_desc) == 0)
			break;
	}
	if (cur_fd == MAX_FD) {
		return -ENFILE;
	}

	return cur_fd;
}

/* XXX Concurrent problem */
void free_fd(int fd)
{
	free(fd_dic[fd]);
	fd_dic[fd] = 0;
}

/* Convert fsm_cap to the ipc_struct将fsm_cap转化为ipc结构
 *
 * the `fsm_cap' is the cap in the fsm (different cap represent different
 * physical fs). It is a const value. `fsm_cap' is used to find the ipc_struct
 * in the hash table.
 * the `libc_cap' is the cap in libc (the cap that fsm passes to libc), is an
 * indeterminate value. `libc_cap' is only used to establish access with the
 * physical fs for the first time.
 * If fsm sends us the cap of the same file system twice, then we will get two
 * different caps. So fsm will only send it to us once (copy this cap to libc).
 * We should use this `libc_cap' to register the physical fs server to become a
 * clinet.
 * The same `fsm_cap' is the key for us to find this physical fs. */
 //fsm_cap是fsm的cap 不同的cap代表不同的物理fs 是常量 fsm用来在哈希表汇总找到IPC结构
 //libc_cap是libc的cap 是不确定值 libc_cap只用第一次和物理文件系统建立连接
 //如果fsm两次向我们发送同一个文件系统的cap，那么我们将得到两个不同的cap 所以fsm只会把它向我们发送一次（把这个cap值复制发送给libc）
 //我们应该使用这个libc_cap来登记这个物理fs服务器 让它变成一个客户端
 //同样的fsm_cap是我们找到这个物理fs的key值
ipc_struct_t *cap2struct(int fsm_cap, int libc_cap)
{
	struct htable *ht = &fs_cap_struct_htable;
	struct hlist_head *head;
	struct fs_cap_struct *cap_struct, *target = NULL;
	ipc_struct_t *ipc_struct;

	/* init the hash table if it has not been initialized */
	//初始化哈希表
	if (!ht->buckets)
		init_htable(ht, MAX_FD);

	/* search the ipc_struct in the hash table using the fsm_cap */
	//用fsm_cap在哈希表中查找ipc_struct
	head = htable_get_bucket(ht, (u32)fsm_cap);
	for_each_in_hlist(cap_struct, node, head) {
		if (cap_struct->fsm_cap == fsm_cap) {
			target = cap_struct;
			break;
		}
	}

	/* if we have registerd the cap, the corresponding ipc_struct will be in
	 * the target. If the target is NULL, then we have not registered this
	 * cap yet */
	 //如果我们登记了这个cap，对应的ipc_struct将会在target中 如果target为空，说明我们还没有登记
	if (!target) {
		/* register the cap first using the libc_cap */
		ipc_struct = ipc_register_client(libc_cap);
		if (ipc_struct == NULL) {
			printf("fs register client failed, fsm_cap=%d"
			       "libc_cap=%d\n", fsm_cap, libc_cap);
			exit(-1);
		}

		/* add this cap to the hash table */
		//将这个cap加入哈希表
		target = malloc(sizeof(struct fs_cap_struct));
		target->fsm_cap = fsm_cap;
		target->_fs_ipc_struct = ipc_struct;
		htable_add(ht, (u32)fsm_cap, &target->node);
	}

	/* now we get the target cap anyway */
	//现在得到目标taget cap
	BUG_ON(!target);
	return target->_fs_ipc_struct;
}


/* fd opeartions */
int chcore_read(int fd, void *buf, size_t count)
{
	if (fd_dic[fd] == 0)
		return -EBADF;
	return fd_dic[fd]->fd_op->read(fd, buf, count);
}

int chcore_write(int fd, void *buf, size_t count)
{
	if(fd_dic[fd] == 0)
		return -EBADF;
	return fd_dic[fd]->fd_op->write(fd, buf, count);
}

int chcore_close(int fd)
{
	if(fd_dic[fd] == 0)
		return -EBADF;
	return fd_dic[fd]->fd_op->close(fd);
}

int chcore_ioctl(int fd, unsigned long request, void *arg)
{
	if(fd_dic[fd] == 0)
		return -EBADF;
	return fd_dic[fd]->fd_op->ioctl(fd, request, arg);
}

int chcore_readv(int fd, const struct iovec *iov, int iovcnt)
{
	int iov_i, ret;
	ssize_t byte_read;

	if ((ret = iov_check(iov, iovcnt)) != 0)
		return ret;

	byte_read = 0;
	for (iov_i = 0; iov_i < iovcnt; iov_i++) {
		ret = chcore_read(fd, (void *)((iov + iov_i)->iov_base),
				  (size_t)(iov + iov_i)->iov_len);
		if (ret < 0) {
			return ret;
		}

		byte_read += ret;
		if (ret != (iov + iov_i)->iov_len) {
			return byte_read;
		}
	}

	return byte_read;
}

int chcore_writev(int fd, const struct iovec *iov, int iovcnt)
{
	int iov_i, ret;
	ssize_t byte_written;

	if ((ret = iov_check(iov, iovcnt)) != 0)
		return ret;

	byte_written = 0;
	for (iov_i = 0; iov_i < iovcnt; iov_i++) {
		ret = chcore_write(fd, (void *)((iov + iov_i)->iov_base),
				   (size_t)(iov + iov_i)->iov_len);
		if (ret < 0) {
			return ret;
		}

		byte_written += ret;
		if (ret != (iov + iov_i)->iov_len) {
			return byte_written;
		}
	}
	return byte_written;
}

/* Default file operation */

/* TYPE TO STRING */
static char fd_type[][20] = {
	"FILE",
	"PIPE",
	"SCOK",
	"STDIN",
	"STDOUT",
	"STDERR",
	"EVENT",
	"TIMER",
	"EPOLL",
};

static int fd_default_read(int fd, void *buf, size_t size)
{
	printf("READ fd %d type %s not impl!\n", fd, fd_type[fd_dic[fd]->type]);
	return -EINVAL;
}

static int fd_default_write(int fd, void *buf, size_t size)
{
	printf("WRITE fd %d type %s not impl!\n", fd, fd_type[fd_dic[fd]->type]);
	return -EINVAL;
}

static int fd_default_close(int fd)
{
	printf("CLOSE fd %d type %s not impl!\n", fd, fd_type[fd_dic[fd]->type]);
	return -EINVAL;
}

static int fd_default_poll(int fd, struct pollarg *arg)
{
	printf("POLL fd %d type %s not impl!\n", fd, fd_type[fd_dic[fd]->type]);
	return -EINVAL;
}

static int fd_default_ioctl(int fd, unsigned long request, void *arg)
{
	printf("IOCTL fd %d type %s not impl!\n", fd, fd_type[fd_dic[fd]->type]);
	return -EINVAL;
}

struct fd_ops default_ops = {
	.read = fd_default_read,
	.write = fd_default_write,
	.close = fd_default_close,
	.poll = fd_default_poll,
	.ioctl = fd_default_ioctl,
};
