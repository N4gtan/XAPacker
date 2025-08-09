#include "xa-interleaver/libxa_deinterleaver.hxx"
#include "xa-interleaver/libxa_interleaver.hxx"
#define VER "VERSION"
#define CD_SECTOR_SIZE 2352

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

class XAPinterleaver : public interleaver
{
    void nullCustomizer(unsigned char *emptyBuffer, FileInfo &entry) override
    {
        entry.nullSubheader[0] = entry.filenum.value_or(emptyBuffer[FILENUM_OFFSET]);
        entry.nullSubheader[1] = entry.nullTermination >= 0 ? entry.channel.value_or(0) : 0;
        entry.nullSubheader[2] = 0x48;
    }
public:
    XAPinterleaver(const std::filesystem::path &path, const int &stride) : interleaver(path, stride) {}
};

class XAPdeinterleaver : public deinterleaver
{
    void createManifest(const std::filesystem::path &outputDir, const std::string &fileName) override
    {
        FILE *manifest = fopen((outputDir / fileName).string().c_str(), "w");
        if (!manifest)
        {
            fprintf(stderr, "Error: Cannot write manifest \"%s\". %s\n", fileName.c_str(), strerror(errno));
            return;
        }

        //fprintf(manifest, "chunk,type,file,null_termination,xa_file_number,xa_channel_number\n");
        for (const FileInfo &entry : entries)
        {
            fprintf(manifest, "%d,%s,%s,%d,%d,%d\n", entry.sectorChunk, inputSectorSize == XA_DATA_SIZE ? "xa" : "xacd",
                    entry.fileName.c_str(), entry.nullTermination, entry.filenum, entry.channel);
        }
        fclose(manifest);
    }
public:
    XAPdeinterleaver(const std::filesystem::path &path) : deinterleaver(path) {}
};

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("XAPacker " VER " by N4gtan\n\n");
        printf("    Usage: XAPacker <input> <2336/2352> <output>\n\n");
        printf("    Input: Manifest or XAP file\n");
        printf("2336/2352: Output file sector size (2336 or 2352). Defaults to input file sector size\n");
        printf("   Output: Optional output file(s) path. Defaults to input file path\n");
        return EXIT_SUCCESS;
    }

    const std::filesystem::path inputPath = argv[1];
    const uint8_t sectorStride = 8;
    int sectorSize = argc >= 3 ? atoi(argv[2]) : 0;

    if (strcasecmp(inputPath.extension().string().c_str(), ".xap"))
    {
        XAPinterleaver files(inputPath, sectorStride);
        if (files.entries.empty())
        {
            fprintf(stderr, "Invalid manifest\n");
            return EXIT_FAILURE;
        }

        const std::filesystem::path outputFile = argc >= 4 ? argv[3] : inputPath.stem() += "_NEW.XAP";
        std::fstream output(outputFile, std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc);
        if (!output)
        {
            fprintf(stderr, "Error: Cannot write output file.\n");
            return EXIT_FAILURE;
        }

        if (!sectorSize)
            sectorSize = files.entries.front().sectorSize;

        const size_t headerSectors = 2 + (files.entries.size() / 129);
        for (size_t i = 0; i < headerSectors; ++i)
        {
            XAPheader header {};
            if (!i)
                header.count = (uint8_t)files.entries.size();
            else
            {
                int startIndex = (i - 1) * 128;
                int limit      = std::min(files.entries.size(), i * 128) - 1;
                int limitIndex = limit - startIndex;
                for (int j = startIndex; j <= limit; ++j)
                {
                    auto &entry   = files.entries[j];
                    entry.begSec += headerSectors;
                   (entry.endSec += headerSectors) -= entry.nullTermination * sectorStride;

                    if (!(*entry.channel % 128))
                    {
                        memcpy(&header.sign, &entry.begSec, sizeof(entry.begSec));
                        memcpy(&header.stride, &entry.endSec, sizeof(entry.endSec));
                    }
                    else
                    {
                        int index = *entry.channel - 1 - startIndex;
                        if (index < 0 || index >= limitIndex)
                        {
                            fprintf(stderr, "Error: Invalid channel number \"%d\" for file \"%s\"\n", *entry.channel, entry.filePath.filename().string().c_str());
                            return EXIT_FAILURE;
                        }
                        header.file[index].filenum = *entry.filenum;
                        header.file[index].id      = *entry.channel - 1;
                        header.file[index].begSec  = entry.begSec;
                        header.file[index].endSec  = entry.endSec;
                    }
                }

                header.file[limitIndex].filenum = *files.entries[limit].filenum;
                header.file[limitIndex].id      = limit;
            }

            output.write(reinterpret_cast<const char*>(&header) + (CD_SECTOR_SIZE - sectorSize), sectorSize);
        }

        files.interleave(output, sectorSize);
        output.close();
    }
    else
    {
        const std::filesystem::path outputDir = argc >= 4 ? argv[3] : inputPath.parent_path() / inputPath.stem();
        XAPdeinterleaver(inputPath).deinterleave(outputDir, sectorSize);
    }

    printf("Process complete.\n");

    return EXIT_SUCCESS;
}
