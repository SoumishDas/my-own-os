/* fs.c -- Filesystem-independent callback wrappers (a tiny VFS layer).
 * A driver fills an fs_node_t with callbacks; these helpers validate and
 * delegate to them.  Paths, descriptors, offsets, permissions, and per-task
 * open-file state do not exist yet. */

#include "fs.h"

fs_node_t *fs_root = 0; // The root of the filesystem.

u32int read_fs(fs_node_t *node, u32int offset, u32int size, u8int *buffer)
{
    /* Zero currently means EOF, unsupported operation, or bad arguments. */
    if (node == 0 || buffer == 0) return 0;
    // Has the node got a read callback?
    if (node->read != 0)
        return node->read(node, offset, size, buffer);
    else
        return 0;
}

u32int write_fs(fs_node_t *node, u32int offset, u32int size, u8int *buffer)
{
    if (node == 0 || buffer == 0) return 0;
    // Has the node got a write callback?
    if (node->write != 0)
        return node->write(node, offset, size, buffer);
    else
        return 0;
}

void open_fs(fs_node_t *node, u8int read, u8int write)
{
    (void)read;
    (void)write;
    if (node == 0) return;
    // Has the node got an open callback?
    if (node->open != 0)
        node->open(node);
}

void close_fs(fs_node_t *node)
{
    if (node == 0) return;
    // Has the node got a close callback?
    if (node->close != 0)
        node->close(node);
}

struct dirent *readdir_fs(fs_node_t *node, u32int index)
{
    if (node == 0) return 0;
    // Is the node a directory, and does it have a callback?
    if ( (node->flags&0x7) == FS_DIRECTORY &&
         node->readdir != 0 )
        return node->readdir(node, index);
    else
        return 0;
}

fs_node_t *finddir_fs(fs_node_t *node, char *name)
{
    if (node == 0 || name == 0) return 0;
    // Is the node a directory, and does it have a callback?
    if ( (node->flags&0x7) == FS_DIRECTORY &&
         node->finddir != 0 )
        return node->finddir(node, name);
    else
        return 0;
}
