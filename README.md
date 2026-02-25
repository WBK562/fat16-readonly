# FAT16 File System Parser (Read-Only)

A low-level C implementation of a FAT16 file system parser. This project provides a POSIX-like API to interact with virtual disk images, bypassing standard OS file handling to interact directly with the file system structures.

Ideal for learning about:
* **Disk Layout:** Boot sectors, FAT tables, and data regions.
* **Manual Memory Mapping:** Calculating physical offsets from logical cluster addresses.
* **Embedded Systems Logic:** Working with fixed-size structures and little-endian data.

## ✨ Key Features

* **Custom Block Device Layer:** Abstracts raw disk image access.
* **POSIX-inspired API:** Includes `file_open`, `file_read`, `file_seek`, and `file_close`.
* **Directory Traversal:** Functions for opening and listing directory contents (`dir_open`, `dir_read`).
* **Robust Offset Calculation:** Handles Cluster-to-LBA (Logical Block Address) mapping.

## 🛠 Technical Highlights

* **Endianness Management:** Ensures data is correctly interpreted from binary disk images.
* **Zero-Dependency:** Written in pure C using only standard headers.
* **Resource Management:** Carefully handles file handles and volume pointers to prevent memory leaks.

## 💻 Getting Started

### Prerequisites
* A C compiler (GCC, Clang, or MSVC).
* A FAT16 formatted disk image (a sample image is usually included in the repository).

### Building
You can compile the project using the provided `Makefile`:
```bash
make
```

Clean build artifacts:
```bash
make clean
```
