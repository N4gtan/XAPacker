#include "xa-interleaver/libxa_deinterleaver.hxx"
#include "xa-interleaver/libxa_interleaver.hxx"
#define VER "VERSION"
#define CD_SECTOR_SIZE 2352

#pragma pack(push, 1)
struct XAPheader
{
    unsigned char sync[12]   {0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00};
    unsigned char addr[3];
    unsigned char mode       {0x02};
    unsigned char subHead[8] {0x01, 0x00, 0x48, 0x00, 0x01, 0x00, 0x48, 0x00};
    unsigned char sign[4]    {'X', 'A', 'P', '1'};
    unsigned char stride     {0x08};
    unsigned char pad;
    unsigned char count;     // number of files
    unsigned char pad2;
    struct
    {
        unsigned char filenum;
        unsigned char id;
        unsigned char pad[6];
        unsigned int  begSec;
        unsigned int  endSec;
    } file[128];
    unsigned char crc[272]; // I know it should be edc[4] and ecc[276], but this way is easier to handle
};
#pragma pack(pop)

class XAPinterleaver : public interleaver
{
    void nullCustomizer(unsigned char *emptyBuffer, FileInfo &entry) override
    {
        entry.nullSubheader[0] = entry.filenum.value_or(emptyBuffer[FILENUM_OFFSET]);
        entry.nullSubheader[1] = entry.nullTrailing >= 0 ? entry.channel.value_or(0) : 0;
        entry.nullSubheader[2] = 0x48;
    }
public:
    using interleaver::interleaver;
};

class XAPdeinterleaver : public deinterleaver
{
    void createManifest(const std::filesystem::path &outputDir, const std::string &fileName, const char *type) override
    {
        FILE *manifest = fopen((outputDir / fileName).string().c_str(), "wb");
        if (!manifest)
        {
            fprintf(stderr, "Error: Cannot write manifest \"%s\". %s\n", fileName.c_str(), strerror(errno));
            return;
        }

        //fprintf(manifest, "chunk,type,file,null_trailing,xa_file_number,xa_channel_number\n");
        for (const FileInfo &entry : entries)
        {
            fprintf(manifest, "%d,%s,%s,%d,%d,%d\n", entry.sectorChunk, type,
                    entry.fileName.c_str(), entry.nullTrailing, entry.filenum, entry.channel);
        }
        fclose(manifest);
    }
public:
    using deinterleaver::deinterleaver;
};

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("XAPacker " VER " by N4gtan\n\n"
               " Usage: XAPacker <input> [size] [output]\n\n"
               " Input: Manifest or XAP file\n"
               "  Size: Optional output file(s) sector size (2336 or 2352). Defaults to input file sector size\n"
               "Output: Optional output dir/file path. Defaults to input file path\n");
        return EXIT_SUCCESS;
    }

    const std::filesystem::path inputPath = argv[1];
    constexpr uint8_t sectorStride = 8;
    int sectorSize = argc >= 3 ? atoi(argv[2]) : 0;

    if (strncasecmp(inputPath.extension().string().c_str(), ".xap", 4))
    {
        XAPinterleaver files(inputPath, sectorStride);
        if (files.entries.empty())
        {
            if (!errno)
                fprintf(stderr, "Invalid manifest\n");
            return EXIT_FAILURE;
        }

        const std::filesystem::path outputPath = argc >= 4 ? argv[3] : inputPath.parent_path() /  inputPath.stem() += "_NEW.XAP";
        FILE *outputFile = fopen(outputPath.string().c_str(), "w+b");
        if (!outputFile)
        {
            fprintf(stderr, "Error: Cannot write \"%s\". %s\n", outputPath.string().c_str(), strerror(errno));
            return EXIT_FAILURE;
        }
        std::unique_ptr<char[]> stdoBuf(new char [1024 * 1024]);
        setvbuf(outputFile, stdoBuf.get(), _IOFBF, 1024 * 1024);

        if (sectorSize == 0)
            sectorSize = files.entries.front().sectorSize;

        const int headerSectors = 2 + (files.entries.size() / 129);
        for (int i = 0; i < headerSectors; ++i)
        {
            XAPheader header{};
            if (i == 0)
                header.count = (uint8_t)files.entries.size();
            else
            {
                int startIndex = (i - 1) * 128;
                int limit      = std::min<int>(files.entries.size(), i * 128) - 1;
                for (auto &entry : files.entries)
                {
                    if (*entry.channel < startIndex || *entry.channel > limit)
                    {
                        if (*entry.channel < 0 || *entry.channel >= files.entries.size())
                        {
                            fprintf(stderr, "Error: Channel number \"%d\" for file \"%s\" is out of range [0, %zu]\n",
                                    *entry.channel, entry.filePath.filename().string().c_str(), files.entries.size() - 1);
                            return EXIT_FAILURE;
                        }
                        continue;
                    }

                    entry.begSec += headerSectors;
                    entry.endSec += headerSectors + sectorStride;

                    if ((*entry.channel % 128) == 0)
                    {
                        memcpy(&header.sign, &entry.begSec, sizeof(entry.begSec));
                        memcpy(&header.stride, &entry.endSec, sizeof(entry.endSec));
                    }
                    else
                    {
                        int index = *entry.channel - 1 - startIndex;
                        if (header.file[index].endSec != 0)
                        {
                            fprintf(stderr, "Error: Channel number \"%d\" for file \"%s\" is duplicated\n",
                                    *entry.channel, entry.filePath.filename().string().c_str());
                            return EXIT_FAILURE;
                        }
                        header.file[index].filenum = *entry.filenum;
                        header.file[index].id      = *entry.channel - 1;
                        header.file[index].begSec  = entry.begSec;
                        header.file[index].endSec  = entry.endSec;
                    }

                    if (*entry.channel == limit)
                    {
                        int index = limit - startIndex;
                        header.file[index].filenum = *entry.filenum;
                        header.file[index].id      = *entry.channel;
                    }
                }
            }

            fwrite(reinterpret_cast<const char *>(&header) + (CD_SECTOR_SIZE - sectorSize), 1, sectorSize, outputFile);
        }

        files.interleave(outputFile, sectorSize);
        fclose(outputFile);
    }
    else
    {
        const std::filesystem::path outputDir = argc >= 4 ? argv[3] : inputPath.parent_path() / inputPath.stem();
        XAPdeinterleaver(inputPath).deinterleave(outputDir, sectorSize);
        if (errno)
            return EXIT_FAILURE;
    }

    printf("Process complete.\n");

    return EXIT_SUCCESS;
}
