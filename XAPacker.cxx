#include "xa-interleaver/libxa_deinterleaver.hxx"
#include "xa-interleaver/libxa_interleaver.hxx"

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("XAPacker v1.0 by N4gtan\n\n");
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
        interleaver files(inputPath, sectorStride);
        if (files.entries.empty())
        {
            fprintf(stderr, "Invalid manifest\n");
            return EXIT_FAILURE;
        }
        if (!sectorSize)
            sectorSize = files.entries.front().sectorSize;

        unsigned char buffer[CD_SECTOR_SIZE] {0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x02,
                                            0x01, 0x00, 0x48, 0x00, 0x01, 0x00, 0x48, 0x00, 'X', 'A', 'P', '1', sectorStride, 0x00, (uint8_t)files.entries.size()};
        //memcpy(buffer + 0x1E, (size_t[]){files.entries.size()}, sizeof(short));
        const std::filesystem::path outputFile = argc >= 4 ? argv[3] : inputPath.stem() += "_NEW.XAP";
        std::fstream output(outputFile, std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc);
        const int offset = sectorSize - XA_DATA_SIZE;

        const size_t headerSectors = (files.entries.size() + 127) / 128 + 1;
        for (size_t i = 0; i < headerSectors; ++i)
        {
            output.write(reinterpret_cast<const char*>(buffer) + (CD_SECTOR_SIZE - sectorSize), sectorSize);

            for (int j = (i - 1) * 128; i > 0 && j < std::min(files.entries.size(), i * 128); ++j)
            {
                auto &entry = files.entries[j];
                if (!(*entry.channel % 128))
                {
                    if (entry.channel != 0)
                    {
                        output.seekp(sectorSize * (i - 1) + offset + 0x10 * *entry.channel, std::ios::beg);
                        unsigned char data[8] {*entry.filenum, (uint8_t)(*entry.channel - 1)};
                        output.write(reinterpret_cast<const char*>(data), sizeof(data));
                    }
                    output.seekp(sectorSize * i + offset + 0x08, std::ios::beg);
                }
                else
                {
                    output.seekp(sectorSize * i + offset + 0x10 * (*entry.channel - (i - 1) * 128), std::ios::beg);
                    unsigned char data[8] {*entry.filenum, (uint8_t)(*entry.channel - 1)};
                    output.write(reinterpret_cast<const char*>(data), sizeof(data));
                }
                output.write(reinterpret_cast<const char*>(&(entry.begSec += headerSectors)), sizeof(entry.begSec));
                output.write(reinterpret_cast<const char*>(&((entry.endSec += headerSectors) -= entry.nullTermination * sectorStride)), sizeof(entry.endSec));
            }

            if (i + 1 == headerSectors)
            {
                output.seekp(sectorSize * i + offset + 0x10 * (files.entries.size() - (i - 1) * 128), std::ios::beg);
                output.put(*files.entries.back().filenum);
                output.put(files.entries.size() - 1);
                output.seekp(0, std::ios::end);
            }
        }

        files.interleave(output, sectorSize, true);
        output.close();
    }
    else
    {
        const std::filesystem::path outputDir = argc >= 4 ? argv[3] : inputPath.parent_path() / inputPath.stem();
        deinterleaver(inputPath).deinterleave(outputDir, sectorSize);
    }

    printf("Process complete.\n");

    return EXIT_SUCCESS;
}
