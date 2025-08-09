# XAPacker
`XAPacker` unpacks .XAP audio files into single xa audio files or packs them into a new .XAP file.

The .XAP format is utilized by Digimon Rumble Arena and maybe other PSX games.

>[!NOTE]
>The tool does not regenerate ECC/EDC for the output.

## Download
[Releases](../../releases/latest) for Windows, Linux and macOS (built by github CI)

## Usage
```
XAPacker <input> <2336/2352> <output>
```

### Commands
>[!CAUTION]
>All arguments are positional so, to set an `<output>`, you need to set the previous ones.

>[!TIP]
>On Windows, can simply drag and drop a supported file into the exe to run it with the default options.

Required:
```
<input>     Can be a .xap file to unpack or a .csv file to pack.
```
Optional:
```
<2336/2352> Output file sector size (2336 or 2352). Defaults to input file (or the first file in the .csv) sector size
<output>    Optional output file(s) path. Defaults to input file path
```
Examples:
```
XAPacker path/to/input.xap 2352 path/to/output/
XAPacker path/to/input.csv 2336 path/to/output.xap
```
## Manifest
The .csv manifest file is the same as [xa-interleaver](../../../xa-interleaver), which has the following format:
```
chunk,type,file,null_termination,xa_file_number,xa_channel_number
```
More info about each field can be found [here](../../../xa-interleaver?tab=readme-ov-file#manifest).

## Compile
A C++ compiler (MSVC, GCC, Clang) is required.

The submodule folder and its contents are required.

### MSVC:
```
cl /std:c++17 /O2 /EHsc /FeXAPacker XAPacker.cxx
```
### GCC:
```
g++ -std=c++17 -O2 -o XAPacker XAPacker.cxx
```
### Clang:
```
clang++ -std=c++17 -O2 -o XAPacker XAPacker.cxx
```
