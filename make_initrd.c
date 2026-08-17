/* Host-side builder for the kernel's tiny, read-only initrd format.
 * Usage: make_initrd host-file name-in-image [host-file name-in-image ...]
 * Image names may be paths such as "bin/hello" or "etc/system/config".
 * Directories are inferred by the kernel; only files occupy headers.
 * The output is a count, 64 fixed headers, then each file's raw bytes. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_INITRD_FILES 64

struct initrd_header
{
    unsigned char magic; /* 0xBF identifies a valid file record. */
    char name[64];       /* Null-terminated name visible inside the image. */
    uint32_t offset;     /* Byte offset from the image's first byte. */
    uint32_t length;     /* File length in bytes. */
};

static int valid_image_path(const char *path)
{
    size_t length = strlen(path);
    if (length == 0 || path[0] == '/' || path[length - 1] == '/')
        return 0;

    const char *component = path;
    for (const char *cursor = path; ; ++cursor)
    {
        if (*cursor != '/' && *cursor != '\0')
            continue;
        size_t component_length = (size_t)(cursor - component);
        if (component_length == 0 ||
            (component_length == 1 && component[0] == '.') ||
            (component_length == 2 && component[0] == '.' && component[1] == '.'))
            return 0;
        if (*cursor == '\0')
            return 1;
        component = cursor + 1;
    }
}

int main(int argc, char **argv)
{
    if (argc < 3 || (argc - 1) % 2 != 0)
    {
        fprintf(stderr, "usage: %s host-file image-name [host-file image-name ...]\n", argv[0]);
        return 1;
    }

    int file_count = (argc - 1) / 2;
    if (file_count > MAX_INITRD_FILES)
    {
        fprintf(stderr, "error: at most %d files are supported\n", MAX_INITRD_FILES);
        return 1;
    }

    /* Zeroing unused headers avoids leaking stack garbage into the image. */
    struct initrd_header headers[MAX_INITRD_FILES] = {{0}};
    uint32_t next_offset = sizeof(uint32_t) + sizeof(headers);

    /* First pass: validate inputs and calculate every on-image location. */
    for (int i = 0; i < file_count; ++i)
    {
        const char *host_name = argv[i * 2 + 1];
        const char *image_name = argv[i * 2 + 2];
        if (strlen(image_name) >= sizeof(headers[i].name))
        {
            fprintf(stderr, "error: image name exceeds 63 bytes: %s\n", image_name);
            return 1;
        }
        if (!valid_image_path(image_name))
        {
            fprintf(stderr, "error: invalid relative image path: %s\n", image_name);
            return 1;
        }
        for (int previous = 0; previous < i; ++previous)
        {
            if (strcmp(headers[previous].name, image_name) == 0)
            {
                fprintf(stderr, "error: duplicate image path: %s\n", image_name);
                return 1;
            }
        }

        FILE *input = fopen(host_name, "rb");
        if (input == NULL || fseek(input, 0, SEEK_END) != 0)
        {
            fprintf(stderr, "error: cannot inspect %s\n", host_name);
            if (input) fclose(input);
            return 1;
        }
        long length = ftell(input);
        fclose(input);
        if (length < 0 || (uint64_t)next_offset + (uint64_t)length > UINT32_MAX)
        {
            fprintf(stderr, "error: inputs exceed the 32-bit image size limit\n");
            return 1;
        }

        headers[i].magic = 0xBF;
        strcpy(headers[i].name, image_name); /* Safe after the length check. */
        headers[i].offset = next_offset;
        headers[i].length = (uint32_t)length;
        next_offset += headers[i].length;
        printf("adding %s as %s (%u bytes)\n", host_name, image_name, headers[i].length);
    }

    FILE *output = fopen("initrd.img", "wb");
    if (output == NULL)
    {
        perror("initrd.img");
        return 1;
    }

    uint32_t count_on_disk = (uint32_t)file_count;
    if (fwrite(&count_on_disk, sizeof(count_on_disk), 1, output) != 1 ||
        fwrite(headers, sizeof(headers), 1, output) != 1)
    {
        fprintf(stderr, "error: failed while writing initrd headers\n");
        fclose(output);
        return 1;
    }

    /* Second pass streams data so a huge input never needs a huge allocation. */
    unsigned char buffer[4096];
    for (int i = 0; i < file_count; ++i)
    {
        FILE *input = fopen(argv[i * 2 + 1], "rb");
        if (input == NULL)
        {
            fprintf(stderr, "error: cannot reopen %s\n", argv[i * 2 + 1]);
            fclose(output);
            return 1;
        }
        uint32_t remaining = headers[i].length;
        while (remaining != 0)
        {
            size_t chunk = remaining < sizeof(buffer) ? remaining : sizeof(buffer);
            if (fread(buffer, 1, chunk, input) != chunk ||
                fwrite(buffer, 1, chunk, output) != chunk)
            {
                fprintf(stderr, "error: failed while copying %s\n", argv[i * 2 + 1]);
                fclose(input);
                fclose(output);
                return 1;
            }
            remaining -= (uint32_t)chunk;
        }
        fclose(input);
    }

    if (fclose(output) != 0)
    {
        perror("initrd.img");
        return 1;
    }
    return 0;
}
