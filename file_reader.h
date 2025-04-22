#ifndef FILE_READER_H
#define FILE_READER_H
#include <stdint.h>
#include <stdio.h>

struct clusters_chain_t {
    uint16_t *clusters;
    size_t size;
};

struct dir_t {
    struct volume_t *wolumin;
    size_t sektor;
    size_t bajt32;
};

struct dir_entry_t {
    char name[12];
    size_t size;
    int is_archived;
    int is_readonly;
    int is_system;
    int is_hidden;
    int is_directory;
};

struct disk_t {
    FILE *dysk_f;
};

struct file_t {
    struct volume_t *volume;
    char name[13];
    uint32_t first_cluster;
    int32_t size;
    int32_t pos;
    struct clusters_chain_t *chain_cluster;
};

struct volume_t {
    struct disk_t *disk;
    int16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t fat_count;
    uint16_t sectors_per_fat;
    uint16_t root_dir_capacity;
    uint16_t logical_sectors16;
    uint32_t logical_sectors32;
    uint16_t magiczna_liczba;
    uint32_t fat1_start;
    uint32_t fat2_start;
    uint32_t root_start;
    uint32_t data_start;
    uint32_t sector_per_root;
    uint16_t volume_start;
};

struct disk_t* disk_open_from_file(const char* volume_file_name);
int disk_read(struct disk_t* pdisk, int32_t first_sector, void* buffer, int32_t sectors_to_read);
int disk_close(struct disk_t* pdisk);

struct volume_t* fat_open(struct disk_t* pdisk, uint32_t first_sector);
int fat_close(struct volume_t* pvolume);

struct file_t* file_open(struct volume_t* pvolume, const char* file_name);
int file_close(struct file_t* stream);
size_t file_read(void *ptr, size_t size, size_t nmemb, struct file_t *stream);
int32_t file_seek(struct file_t* stream, int32_t offset, int whence);

struct dir_t* dir_open(struct volume_t* pvolume, const char* dir_path);
int dir_read(struct dir_t* pdir, struct dir_entry_t* pentry);
int dir_close(struct dir_t* pdir);

#endif //FILE_READER_H
