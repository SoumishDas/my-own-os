/*
 * initrd.c -- Read-only filesystem backed directly by a GRUB-loaded RAM image.
 *
 * The image begins with a file count, reserves room for 64 fixed-size file
 * headers, then stores each file's raw bytes.  This driver wraps those records
 * in fs_node_t objects so callers use the generic VFS helpers in fs.c.  It is
 * deliberately tiny: there are no paths below the root, writes, persistence,
 * file descriptors, permissions, or executable loading yet.
 */

#include "initrd.h"
#include "kheap.h"

initrd_header_t *initrd_header;     // The header.
initrd_file_header_t *file_headers; // The list of file headers.
fs_node_t *initrd_root;             // Our root directory node.
fs_node_t *initrd_dev;              // We also add a directory node for /dev, so we can mount devfs later on.
fs_node_t *root_nodes;              // List of file nodes.
int nroot_nodes;                    // Number of file nodes.
static u32int initrd_location; /* First byte of the module in kernel memory. */
static u32int initrd_end;      /* One byte past the module; used for validation. */

struct dirent dirent;

static u32int initrd_read(fs_node_t *node, u32int offset, u32int size, u8int *buffer)
{
    /* inode is the array index assigned while initialise_initrd builds nodes. */
    initrd_file_header_t header = file_headers[node->inode];
    if (offset >= header.length)
        return 0;
    if (size > header.length-offset)
        size = header.length-offset;
    memcpy(buffer, (u8int*) (initrd_location+header.offset+offset), size);
    return size;
}

static struct dirent *initrd_readdir(fs_node_t *node, u32int index)
{
    /* Index zero is a synthetic /dev directory; image files start at one. */
    if (node == initrd_root && index == 0)
    {
      strcpy(dirent.name, "dev");
      dirent.name[3] = 0;
      dirent.ino = 0;
      return &dirent;
    }

    if (index == 0 || index-1 >= (u32int)nroot_nodes)
        return 0;
    strcpy(dirent.name, root_nodes[index-1].name);
    dirent.name[strlen(root_nodes[index-1].name)] = 0;
    dirent.ino = root_nodes[index-1].inode;
    return &dirent;
}

static fs_node_t *initrd_finddir(fs_node_t *node, char *name)
{
    if (node == initrd_root &&
        !strcmp(name, "dev") )
        return initrd_dev;

    int i;
    for (i = 0; i < nroot_nodes; i++)
        if (!strcmp(name, root_nodes[i].name))
            return &root_nodes[i];
    return 0;
}

fs_node_t *initialise_initrd(u32int location, u32int end)
{
    /* Reject a missing/truncated module before treating arbitrary RAM as headers. */
    ASSERT(location < end);
    ASSERT(end - location >= sizeof(initrd_header_t) +
                             64 * sizeof(initrd_file_header_t));
    initrd_location = location;
    initrd_end = end;
    // Initialise the main and file header pointers and populate the root directory.
    initrd_header = (initrd_header_t *)location;
    file_headers = (initrd_file_header_t *) (location+sizeof(initrd_header_t));
    ASSERT(initrd_header->nfiles <= 64);

    // Initialise the root directory.
    initrd_root = (fs_node_t*)kmalloc(sizeof(fs_node_t));
    strcpy(initrd_root->name, "initrd");
    initrd_root->mask = initrd_root->uid = initrd_root->gid = initrd_root->inode = initrd_root->length = 0;
    initrd_root->flags = FS_DIRECTORY;
    initrd_root->read = 0;
    initrd_root->write = 0;
    initrd_root->open = 0;
    initrd_root->close = 0;
    initrd_root->readdir = &initrd_readdir;
    initrd_root->finddir = &initrd_finddir;
    initrd_root->ptr = 0;
    initrd_root->impl = 0;

    // Initialise the /dev directory (required!)
    initrd_dev = (fs_node_t*)kmalloc(sizeof(fs_node_t));
    strcpy(initrd_dev->name, "dev");
    initrd_dev->mask = initrd_dev->uid = initrd_dev->gid = initrd_dev->inode = initrd_dev->length = 0;
    initrd_dev->flags = FS_DIRECTORY;
    initrd_dev->read = 0;
    initrd_dev->write = 0;
    initrd_dev->open = 0;
    initrd_dev->close = 0;
    initrd_dev->readdir = &initrd_readdir;
    initrd_dev->finddir = &initrd_finddir;
    initrd_dev->ptr = 0;
    initrd_dev->impl = 0;

    root_nodes = 0;
    if (initrd_header->nfiles > 0)
    {
        ASSERT(initrd_header->nfiles <= 0xFFFFFFFF / sizeof(fs_node_t));
        root_nodes = (fs_node_t*)kmalloc(sizeof(fs_node_t) * initrd_header->nfiles);
        memset(root_nodes, 0, sizeof(fs_node_t) * initrd_header->nfiles);
    }
    nroot_nodes = initrd_header->nfiles;

    /* Validate every untrusted image record before exposing it as a VFS node. */
    u32int i;
    for (i = 0; i < initrd_header->nfiles; i++)
    {
        ASSERT(file_headers[i].magic == 0xBF);
        ASSERT(file_headers[i].offset <= initrd_end - initrd_location);
        ASSERT(file_headers[i].length <= initrd_end - initrd_location -
                                         file_headers[i].offset);
        // Create a new file node.
        strcpy(root_nodes[i].name, file_headers[i].name);
        root_nodes[i].mask = root_nodes[i].uid = root_nodes[i].gid = 0;
        root_nodes[i].length = file_headers[i].length;
        root_nodes[i].inode = i;
        root_nodes[i].flags = FS_FILE;
        root_nodes[i].read = &initrd_read;
        root_nodes[i].write = 0;
        root_nodes[i].readdir = 0;
        root_nodes[i].finddir = 0;
        root_nodes[i].open = 0;
        root_nodes[i].close = 0;
        root_nodes[i].impl = 0;
        root_nodes[i].ptr = 0;
    }
    return initrd_root;
}
