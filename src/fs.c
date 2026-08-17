/* fs.c -- Filesystem-independent callback wrappers (a tiny VFS layer).
 * A driver fills an fs_node_t with callbacks; these helpers validate and
 * delegate to them.  Paths, descriptors, offsets, permissions, and per-task
 * open-file state lives in task.c rather than in filesystem drivers. */

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

fs_node_t *finddir_fs(fs_node_t *node, const char *name)
{
    if (node == 0 || name == 0) return 0;
    // Is the node a directory, and does it have a callback?
    if ( (node->flags&0x7) == FS_DIRECTORY &&
         node->finddir != 0 )
        return node->finddir(node, name);
    else
        return 0;
}

fs_node_t *find_path_fs(fs_node_t *root, const char *path)
{
    return resolve_path_fs(root, root, path);
}

fs_node_t *resolve_path_fs(fs_node_t *root, fs_node_t *working_directory,
                           const char *path)
{
    if (root == 0 || working_directory == 0 || path == 0)
        return 0;

    /* A leading slash selects root; otherwise begin at the caller's cwd. */
    fs_node_t *node = *path == '/' ? root : working_directory;
    while (*path == '/')
        path++;
    if (*path == '\0')
        return node;

    while (*path != '\0')
    {
        char component[128];
        u32int length = 0;

        /* Copy exactly one name, rejecting names that cannot fit fs_node.name. */
        while (path[length] != '\0' && path[length] != '/')
        {
            if (length + 1 >= sizeof(component))
                return 0;
            component[length] = path[length];
            length++;
        }
        component[length] = '\0';

        if (!strcmp(component, "."))
        {
            /* A dot means the directory already stored in node. */
        }
        else if (!strcmp(component, ".."))
        {
            /* Root has no parent, so repeated /../../ remains safely at root. */
            if (node != root && node->parent != 0)
                node = node->parent;
        }
        else
        {
            node = finddir_fs(node, component);
            if (node == 0)
                return 0;
        }

        path += length;
        while (*path == '/')
            path++;
    }
    return node;
}

int get_path_fs(fs_node_t *root, fs_node_t *node, char *buffer, u32int capacity)
{
    if (root == 0 || node == 0 || buffer == 0 || capacity < 2)
        return -1;

    /* Save ancestors from leaf to root, then emit them in reverse order. */
    fs_node_t *components[32];
    u32int count = 0;
    while (node != root)
    {
        if (node == 0 || node->parent == 0 || count >= 32)
            return -1; /* Not below this root, or deeper than supported paths. */
        components[count++] = node;
        node = node->parent;
    }

    u32int used = 0;
    buffer[used++] = '/';
    for (u32int i = count; i > 0; i--)
    {
        const char *name = components[i - 1]->name;
        u32int length = (u32int)strlen(name);
        if (used + length + (i > 1 ? 1 : 0) + 1 > capacity)
            return -1;
        memcpy(buffer + used, name, length);
        used += length;
        if (i > 1)
            buffer[used++] = '/';
    }
    buffer[used] = '\0';
    return (int)used;
}

int mkdir_fs(fs_node_t *parent, const char *name)
{
    if (parent == 0 || name == 0 ||
        (parent->flags & 0x7) != FS_DIRECTORY || parent->mkdir == 0)
        return -1;
    return parent->mkdir(parent, name);
}

int mkdir_path_fs(fs_node_t *root, fs_node_t *working_directory,
                  const char *path)
{
    if (root == 0 || working_directory == 0 || path == 0 || *path == '\0')
        return -1;

    fs_node_t *directory = *path == '/' ? root : working_directory;
    while (*path == '/')
        path++;
    if (*path == '\0')
        return -1; /* `/` already exists; it is not a new child name. */

    for (;;)
    {
        char component[128];
        u32int length = 0;
        while (path[length] != '\0' && path[length] != '/')
        {
            if (length + 1 >= sizeof(component))
                return -1;
            component[length] = path[length];
            length++;
        }
        component[length] = '\0';
        if (length == 0)
            return -1;

        const char *remainder = path + length;
        while (*remainder == '/')
            remainder++;
        if (*remainder == '\0')
        {
            /* The final component is the requested new directory name. */
            if (!strcmp(component, ".") || !strcmp(component, ".."))
                return -1;
            return mkdir_fs(directory, component);
        }

        if (!strcmp(component, "."))
        {
            /* Stay in the same directory. */
        }
        else if (!strcmp(component, ".."))
        {
            if (directory != root && directory->parent != 0)
                directory = directory->parent;
        }
        else
        {
            directory = finddir_fs(directory, component);
            if (directory == 0 || (directory->flags & 0x7) != FS_DIRECTORY)
                return -1;
        }
        path = remainder;
    }
}
