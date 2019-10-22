// for Linux
#define _LARGEFILE_SOURCE
//-----------------------------------------------------------------------------
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>
#include <string.h>
//-----------------------------------------------------------------------------
#define SECTOR_SIZE         512
//-----------------------------------------------------------------------------
#pragma pack(1)

// GPT Header - ignore some fields
typedef struct GPTHeader_ {
    unsigned char signature[8];
    unsigned int fixed;
    unsigned int headerSize;
    unsigned char headerCRC32[4];
    unsigned int reserved;
    unsigned long long firstHeaderLBA;
    unsigned long long secondHeaderLBA;
    unsigned long long firstPartLBA;
    unsigned long long lastPartLBA;
    unsigned char guid[16];
    unsigned long long startPartHeaderLBA;
    unsigned int partCount;
    unsigned int partHeaderSize;
    unsigned char partSeqCRC32[4];
} GPTHeader;

// GPT Partition Header
typedef struct GPTPartHeader_ {
    unsigned char partTypeGUID[16];
    unsigned char partGUID[16];
    unsigned long long firstLBA;
    unsigned long long lastLBA;
    unsigned char label[8];
    unsigned char name[72];
} GPTPartHeader;

#pragma pack()
//---------------------------------------------------------------------------
// num is sector number, it starts with 0
bool ReadSect(const char *dsk, char *buf, unsigned long long num)
{
    if (strlen(dsk) == 0) {
        return false;
    }

    FILE *f = fopen(dsk, "rb");
    if (!f) {
        return false;
    }

    unsigned long long n = num * SECTOR_SIZE;

    fseeko(f, n, SEEK_SET);
    fread(buf, SECTOR_SIZE, 1, f);

    fclose(f);

    return true;
}
//-----------------------------------------------------------------------------
void printS(const char *str, unsigned char *buf, int size, int isHex)
{
    printf("%s", str);

    for (int i = 0; i < size; i++) {
        if (isHex) {
            printf("%02X", buf[i]);
        } else {
            printf("%c", buf[i]);
        }
    }
    printf(" \r\n");
}
//-----------------------------------------------------------------------------
void printGPTHeader(GPTHeader *p)
{
    printf("First GPT Header \r\n");
    printf("------------------ \r\n");
    printS("Signature              - ", p->signature, 8, 0);
    printf("Fixed                  - 0x%08X \r\n", p->fixed);
    printf("GPT Header Size        - %d \r\n", p->headerSize);
    printS("Header CRC32           - ", p->headerCRC32, 4, 1);
    printf("Reserved               - %d \r\n", p->reserved);
    printf("1st Header LBA         - 0x%llX (%llu) \r\n", p->firstHeaderLBA, p->firstHeaderLBA);
    printf("2nd Header LBA         - 0x%llX (%llu) \r\n", p->secondHeaderLBA, p->secondHeaderLBA);
    printf("First Partition LBA    - 0x%llX \r\n", p->firstPartLBA);
    printf("Last Partition LBA     - 0x%llX \r\n", p->lastPartLBA);
    printS("GUID                   - ", p->guid, 16, 1);
    printf("Start Partition Header - 0x%llX (%llu) \r\n", p->startPartHeaderLBA, p->startPartHeaderLBA);
    printf("Partition Count        - %d \r\n", p->partCount);
    printf("Partition Header Size  - %d \r\n", p->partHeaderSize);
    printS("Partition Seq CRC32    - ", p->partSeqCRC32, 4, 1);
}
//-----------------------------------------------------------------------------
void printG(char *format, unsigned char *buf)
{
    printf("%s", format);

    printf(
        "%02X%02X%02X%02X-%02X%02X-%02X%02X-",
        buf[3], buf[2], buf[1], buf[0],
        buf[5], buf[4],
        buf[7], buf[6]
    );

    for (int i = 8; i <= 15; i++) {
        printf("%02X", buf[i]);

        if (i == 9) {
            printf("-");
        }
    }

    printf(" \r\n");
}
//-----------------------------------------------------------------------------
void printGPTPartHeader(int index, GPTPartHeader *p)
{
    char* SSDMsg[2] = {"No", "Yes"};

    int is4KAlign = 0;
    if ((p->firstLBA * 512) % 4096 == 0) {
        is4KAlign = 1;
    }

    printf("GPT Partition Header #%02d \r\n", index);
    printf("--------------------------- \r\n");
    printG("Partition GUID       - ", p->partTypeGUID);
    printS("GUID                 - ", p->partGUID, 16, 1);
    printf("Partition Begin LBA  - 0x%llX (%llu) \r\n", p->firstLBA, p->firstLBA);
    printf("Partition End LBA    - 0x%llX (%llu) \r\n", p->lastLBA, p->lastLBA);
    printf("4K Alignment         - %s \r\n",SSDMsg[is4KAlign]);
    printf("ASD SSD Benchmark    - %llu \r\n", p->firstLBA * 512 / 1024);
    printf("\r\n");
}
//-----------------------------------------------------------------------------
void processGPTPartitionHeader(char *drv, GPTHeader *gptHeader)
{
    unsigned long long startSector = gptHeader->startPartHeaderLBA;
    unsigned int partHeaderSize = gptHeader->partHeaderSize;

    if (partHeaderSize != 128) {
        printf("GPT Partition Header size is %d, not supported yet. \r\n", partHeaderSize);
        return;
    }

    unsigned partIndex = 1;
    char secBuf[SECTOR_SIZE];

    while (1) {
        ReadSect(drv, secBuf, startSector);

        for (int i = 0; i < 4; i++) {
            GPTPartHeader *p = (GPTPartHeader*)&secBuf[128 * i];

            if (p->partTypeGUID[0] == 0x00 && p->partTypeGUID[1] == 0x00) {
                unsigned int count = (startSector - gptHeader->startPartHeaderLBA) * 4 + i;
                printf("Total Partitions are %d. \r\n", count);
                return;
            }

            printGPTPartHeader(partIndex++, p);
        }

        startSector++;
    }
}
//-----------------------------------------------------------------------------
void getDriveName(char *drv, unsigned int diskNum)
{
#ifdef __linux__
    sprintf(drv, "/dev/sd%c", 'a' + diskNum);
#else
    sprintf(drv, "\\\\.\\PhysicalDrive%d", diskNum);
#endif
}
//---------------------------------------------------------------------------
int main(int argc, char* argv[])
{
    unsigned int diskNum = 0;

    char drv[64];
    memset(drv, 0x00, sizeof(drv));

    if (argc == 2) {
        diskNum = atoi(argv[1]);
    }

    getDriveName(drv, diskNum);

    unsigned long long sector = 1;
    char secBuf[SECTOR_SIZE];
    bool res = ReadSect(drv, secBuf, sector);

    if (!res) {
        printf("Can't read sector \r\n");
        return -1;
    }

    GPTHeader *gptHeader;
    gptHeader = (GPTHeader*)secBuf;

    // GPT Header
    printGPTHeader(gptHeader);

    printf("\r\n");

    // GPT Partition Header
    processGPTPartitionHeader(drv, gptHeader);

    printf("\r\n");

    return 0;
}