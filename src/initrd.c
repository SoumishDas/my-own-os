/*
 * initrd.c -- Turn a flat list of archived path names into a read-only VFS tree.
 *
 * On disk, the GRUB module is intentionally simple: a file count, 64 headers,
 * and raw file bytes.  A header name may contain slashes (for example,
 * "etc/system/config.txt").  At boot this driver creates the missing directory
 * nodes and links everything into a tree.  Directories therefore cost no image
 * records and contain no data; they are inferred from file paths.
 *
 * Each internal tree node begins with fs_node_t.  Because it is the first
 * member, a callback receiving fs_node_t* can safely cast back and follow the
 * parent/child/sibling links.  Children are a linked list, avoiding resizing or
 * invalidating arrays while the tree is constructed.
 */

#include "initrd.h"
#include "kheap.h"

#define INITRD_HEADER_SLOTS 64
#define MAX_PATH_COMPONENTS 32

typedef struct initrd_tree_node
{
    fs_node_t fs;                         /* Public VFS view; must remain first. */
    struct initrd_tree_node *parent;
    struct initrd_tree_node *first_child;
    struct initrd_tree_node *next_sibling;
} initrd_tree_node_t;

initrd_header_t *initrd_header;
initrd_file_header_t *file_headers;
fs_node_t *initrd_root;
fs_node_t *initrd_dev;

static u32int initrd_location; /* First module byte in the kernel address space. */
static u32int initrd_end;      /* One byte past the module, for bounds checking. */
static struct dirent shared_dirent;

static void configure_directory(initrd_tree_node_t *directory);

static initrd_tree_node_t *tree_node_from_fs(fs_node_t *node)
{
    /* Valid because fs is the first field of initrd_tree_node_t. */
    return (initrd_tree_node_t*)node;
}

static initrd_tree_node_t *new_tree_node(const char *name, u32int flags)
{
    /* Individual allocations keep every pointer stable as runtime nodes grow. */
    initrd_tree_node_t *node = (initrd_tree_node_t*)kmalloc(sizeof(*node));
    ASSERT(node != 0);
    memset(node, 0, sizeof(*node));
    ASSERT((u32int)strlen(name) < (u32int)sizeof(node->fs.name));
    strcpy(node->fs.name, name);
    node->fs.flags = flags;
    return node;
}

static void attach_child(initrd_tree_node_t *parent, initrd_tree_node_t *child)
{
    ASSERT(parent != 0 && child != 0 && child->parent == 0);
    child->parent = parent;
    child->fs.parent = &parent->fs;
    child->next_sibling = parent->first_child;
    parent->first_child = child;
}

static initrd_tree_node_t *find_child(initrd_tree_node_t *directory,
                                      const char *name)
{
    for (initrd_tree_node_t *child = directory->first_child;
         child != 0; child = child->next_sibling)
        if (!strcmp(child->fs.name, name))
            return child;
    return 0;
}

static u32int initrd_read(fs_node_t *node, u32int offset, u32int size,
                          u8int *buffer)
{
    /* A file node's inode is the corresponding file_headers[] index. */
    initrd_file_header_t *header = &file_headers[node->inode];
    if (offset >= header->length)
        return 0;
    if (size > header->length - offset)
        size = header->length - offset;
    memcpy(buffer, (u8int*)(initrd_location + header->offset + offset), size);
    return size;
}

static struct dirent *initrd_readdir(fs_node_t *node, u32int index)
{
    initrd_tree_node_t *directory = tree_node_from_fs(node);
    initrd_tree_node_t *child = directory->first_child;
    while (child != 0 && index != 0)
    {
        child = child->next_sibling;
        index--;
    }
    if (child == 0)
        return 0;

    strcpy(shared_dirent.name, child->fs.name);
    shared_dirent.ino = child->fs.inode;
    return &shared_dirent;
}

static fs_node_t *initrd_finddir(fs_node_t *node, const char *name)
{
    initrd_tree_node_t *child = find_child(tree_node_from_fs(node), name);
    return child == 0 ? 0 : &child->fs;
}

static int initrd_mkdir(fs_node_t *parent_node, const char *name)
{
    initrd_tree_node_t *parent = tree_node_from_fs(parent_node);
    if (name == 0 || *name == '\0' || find_child(parent, name) != 0)
        return -1;

    initrd_tree_node_t *directory = new_tree_node(name, FS_DIRECTORY);
    configure_directory(directory);
    attach_child(parent, directory);
    return 0;
}

static void configure_directory(initrd_tree_node_t *directory)
{
    directory->fs.flags = FS_DIRECTORY;
    directory->fs.readdir = initrd_readdir;
    directory->fs.finddir = initrd_finddir;
    directory->fs.mkdir = initrd_mkdir;
}

static initrd_tree_node_t *get_or_create_directory(initrd_tree_node_t *parent,
                                                    const char *name)
{
    initrd_tree_node_t *node = find_child(parent, name);
    if (node != 0)
    {
        /* A path may reuse a directory, but may not walk through a file. */
        ASSERT((node->fs.flags & 0x7) == FS_DIRECTORY);
        return node;
    }

    node = new_tree_node(name, FS_DIRECTORY);
    configure_directory(node);
    attach_child(parent, node);
    return node;
}

static void add_archived_file(u32int header_index)
{
    initrd_file_header_t *header = &file_headers[header_index];

    /* Prove the fixed-width archive name contains a terminator before reading it. */
    u32int path_length = 0;
    while (path_length < sizeof(header->name) && header->name[path_length] != '\0')
        path_length++;
    ASSERT(path_length > 0 && path_length < sizeof(header->name));
    ASSERT(header->name[0] != '/' && header->name[path_length - 1] != '/');

    initrd_tree_node_t *directory = tree_node_from_fs(initrd_root);
    u32int position = 0;
    u32int component_count = 0;
    while (position < path_length)
    {
        char component[128];
        u32int component_length = 0;
        while (position < path_length && header->name[position] != '/')
        {
            ASSERT(component_length + 1 < sizeof(component));
            component[component_length++] = header->name[position++];
        }
        component[component_length] = '\0';
        ASSERT(component_length != 0);
        ASSERT(strcmp(component, ".") != 0 && strcmp(component, "..") != 0);
        ASSERT(++component_count <= MAX_PATH_COMPONENTS);

        if (position < path_length)
        {
            /* Consume exactly one separator; consecutive slashes are invalid. */
            position++;
            ASSERT(position < path_length && header->name[position] != '/');
            directory = get_or_create_directory(directory, component);
            continue;
        }

        /* Final component is the actual archived file and must be unique. */
        ASSERT(find_child(directory, component) == 0);
        initrd_tree_node_t *file = new_tree_node(component, FS_FILE);
        file->fs.inode = header_index;
        file->fs.length = header->length;
        file->fs.read = initrd_read;
        attach_child(directory, file);
    }
}

fs_node_t *initialise_initrd(u32int location, u32int end)
{
    ASSERT(location < end);
    ASSERT(end - location >= sizeof(initrd_header_t) +
                             INITRD_HEADER_SLOTS * sizeof(initrd_file_header_t));
    initrd_location = location;
    initrd_end = end;
    initrd_header = (initrd_header_t*)location;
    file_headers = (initrd_file_header_t*)(location + sizeof(initrd_header_t));
    ASSERT(initrd_header->nfiles <= INITRD_HEADER_SLOTS);

    initrd_tree_node_t *root = new_tree_node("initrd", FS_DIRECTORY);
    configure_directory(root);
    initrd_root = &root->fs;

    initrd_tree_node_t *dev = new_tree_node("dev", FS_DIRECTORY);
    configure_directory(dev);
    attach_child(root, dev);
    initrd_dev = &dev->fs;

    for (u32int i = 0; i < initrd_header->nfiles; i++)
    {
        ASSERT(file_headers[i].magic == 0xBF);
        ASSERT(file_headers[i].offset <= initrd_end - initrd_location);
        ASSERT(file_headers[i].length <= initrd_end - initrd_location -
                                         file_headers[i].offset);
        add_archived_file(i);
    }
    return initrd_root;
}
