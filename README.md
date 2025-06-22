# RGR2Import

A command-line utility to download photos from Ricoh GR II camera over WiFi.

## Features

- Download photos directly from camera via WiFi
- Filter by file format (JPG, DNG, or all)
- Download specific files by name
- Organize photos by date in subfolders
- Skip already downloaded files
- Progress indication during download
- Custom target directory support

## Installation

```bash
# Install dependencies
make deps

# Compile
make

```

## Usage

### Download all photos

./rgr2import

### Download only JPG files

./rgr2import -f jpg

### Download only DNG files

./rgr2import -f dng

### Download specific file

./rgr2import -F R0001234.JPG

### Download to custom directory

./rgr2import -p /media/usb/photos

### Show help

./rgr2import -h

## Requirements

- libcurl4-openssl-dev
- libcjson-dev
- Ricoh GR II camera with WiFi enabled

## License

MIT License
