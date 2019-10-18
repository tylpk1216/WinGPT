#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <windows.h>
#include <stdbool.h>
//-----------------------------------------------------------------------------
#define SECTOR_SIZE         512
//-----------------------------------------------------------------------------
#pragma pack(1)

// GPT Header
typedef struct GPTHeader_ {
    char signature[8];
    unsigned int fixed;
    unsigned int headerSize;
    char headerCRC32[4];
    unsigned int reserved;
    unsigned long long firstHeaderLBA;
    unsigned long long secondHeaderLBA;
    unsigned long long firstPartLBA;
    unsigned long long lastPartLBA;
    char guid[16];
    unsigned long long startPartHeaderLBA;
    unsigned int partCount;
    unsigned int partHeaderSize;
    char partSeqCRC32[4];
} GPTHeader;

// GPT Partition Header
typedef struct GPTPartHeader_ {
    char partTypeGUID[16];
    char partGUID[16];
    unsigned long long firstLBA;
    unsigned long long lastLBA;
    char label[8];
    char name[72];
} GPTPartHeader;

#pragma pack()
//---------------------------------------------------------------------------
// num is sector number, it starts with 0
bool ReadSect(const char *dsk, char *buf, unsigned long long num)
{
    if (strlen(dsk) == 0) {
        return false;
    }

    DWORD dwRead;
    HANDLE hDisk = CreateFile(dsk, GENERIC_READ, FILE_SHARE_VALID_FLAGS, 0, OPEN_EXISTING, 0, 0);
    if (hDisk == INVALID_HANDLE_VALUE) {
        CloseHandle(hDisk);
        return false;
    }

    UINT64 n = num * SECTOR_SIZE;
    UINT64 base = (UINT64)pow(2, 32);
    long low = n % base;
    long hign = n / base;

    SetFilePointer(hDisk, low, &hign, FILE_BEGIN);
    ReadFile(hDisk, buf, SECTOR_SIZE, &dwRead, 0);

    CloseHandle(hDisk);

    return true;
}
//-----------------------------------------------------------------------------
void showStrValue(const char *str, unsigned char *buf, int size, int isHex)
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
    showStrValue("Signature              - ", p->signature, 8, 0);
    printf("Fixed                  - 0x%08X \r\n", p->fixed);
    printf("GPT Header Size        - %d \r\n", p->headerSize);
    showStrValue("Header CRC32           - ", p->headerCRC32, 4, 1);
    printf("Reserved               - %d \r\n", p->reserved);
    printf("1st Header LBA         - 0x%X (%llu) \r\n", p->firstHeaderLBA, p->firstHeaderLBA);
    printf("2nd Header LBA         - 0x%X (%llu) \r\n", p->secondHeaderLBA, p->secondHeaderLBA);
    printf("First Partition LBA    - 0x%X \r\n", p->firstPartLBA, p->firstPartLBA);
    printf("Last Partition LBA     - 0x%X \r\n", p->lastPartLBA, p->lastPartLBA);
    showStrValue("GUID                   - ", p->guid, 16, 1);
    printf("Start Partition Header - 0x%X (%llu) \r\n", p->startPartHeaderLBA, p->startPartHeaderLBA);
    printf("Partition Count        - %d \r\n", p->partCount);
    printf("Partition Header Size  - %d \r\n", p->partHeaderSize);
    showStrValue("Partition Seq CRC32    - ", p->partSeqCRC32, 4, 1);
}
//-----------------------------------------------------------------------------
void printfPartTypeGUID(char *format, unsigned char *buf)
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
void printfGPTPartHeader(int index, GPTPartHeader *p)
{
    int is4KAlign = 0;
    if ((p->firstLBA * 512) % 4096 == 0) {
        is4KAlign = 1;
    }

    printf("GPT Partition Header #%02d \r\n", index);
    printf("--------------------------- \r\n");
    printfPartTypeGUID("Partition GUID       - ", p->partTypeGUID);
    showStrValue("GUID                 - ", p->partGUID, 16, 1);
    printf("Partition Begin LBA  - 0x%X (%llu) \r\n", p->firstLBA, p->firstLBA);
    printf("Partition End LBA    - 0x%X (%llu) \r\n", p->lastLBA, p->lastLBA);
    printf("4K Alignment         - %d \r\n", is4KAlign);
    printf("ASD SSD Benchmark    - %llu \r\n", p->firstLBA * 512 / 1024);
    printf("\r\n");
}
//-----------------------------------------------------------------------------
void processGPTPartitionHeader(char *drv, GPTHeader *gptHeader)
{
    unsigned long long startSector = gptHeader->startPartHeaderLBA;
    unsigned int partCount = gptHeader->partCount;
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
                unsigned int count = (gptHeader->startPartHeaderLBA - startSector) * 4 + i;
                printf("Total Partitions are %d. \r\n", count);
                return;
            }

            printfGPTPartHeader(partIndex++, p);
        }

        startSector++;
    }
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

    sprintf(drv, "\\\\.\\PhysicalDrive%d", diskNum);

    unsigned long long sector = 1;
    char secBuf[SECTOR_SIZE];
    ReadSect(drv, secBuf, sector);

    GPTHeader *gptHeader;
    gptHeader = (GPTHeader*)secBuf;

    // GPT Header
    printGPTHeader(gptHeader);

    printf("\r\n");

    // GPT Partition Header
    processGPTPartitionHeader(drv, gptHeader);

    printf("\r\n");

    system("pause");
    return 0;
}