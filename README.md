<h1 align="center">Flipper Zero: MTP</h1>

## Build Status

- **Latest Release**: [Download](https://github.com/Alex4386/f0-mtp/releases/latest)
- **Latest Nightly**: [Download](https://github.com/Alex4386/f0-mtp/actions/workflows/nightly.yml) _(GitHub Login Required)_

|                                           Nightly Build                                           |                                           Release Build                                           |
| :-----------------------------------------------------------------------------------------------: | :-----------------------------------------------------------------------------------------------: |
| ![Nightly Build](https://github.com/Alex4386/f0-mtp/actions/workflows/nightly.yml/badge.svg) | ![Release Build](https://github.com/Alex4386/f0-mtp/actions/workflows/release.yml/badge.svg) |

## What is this?
[**TL;DR** (Image)](https://github.com/Alex4386/f0-mtp/assets/27724108/12c5c152-c23a-4853-afe9-9db8d936d5d2)

`f0-mtp` is a application that implements [`MTP (Media Transfer Protocol)` defined by `USB-IF`](https://www.usb.org/document-library/media-transfer-protocol-v11-spec-and-mtp-v11-adopters-agreement) on Flipper Zero.  
This allows you to access the Flipper Zero's internal and SD card storages from your computer without the need of any additional drivers like [`HID-File-Transfer`](https://github.com/Kavakuo/HID-File-Transfer).  
  
If your computer can handle Android devices, it should be able to handle Flipper Zero as well with this application.  
  

### Features
* Access Internal and SD card storages
* List files and directories
   - Navigate directories
* Opening Files (Downloading Flipper files into Computer)
* Deleting Files

### Known Issues
* Creating directories, files, uploading files are not supported yet.
* Due to memory leak happening somewhere, sometimes the Flipper crashes during the file transfer.  
  - I'm currently busy working on "Creating" part to at least work, so this won't be fixed soon.

## How to build
See [HOW_TO_BUILD.md](HOW_TO_BUILD.md) for more information.  

## Special Thanks
- [viveris/uMTP-Responder](https://github.com/viveris/uMTP-Responder)
- [yoonghm/MTP](https://github.com/yoonghm/MTP)

**and Special NOT Thanks to**:  
- [Microsoft](https://microsoft.com) for making the [MTP Spec](https://www.usb.org/document-library/media-transfer-protocol-v11-spec-and-mtp-v11-adopters-agreement) so hard to understand. `>:(`  
- [Microsoft](https://microsoft.com) for reserving place for programmers to live in `WideChar` and `MultiByte` hellscape. `>:(`

## License
This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
