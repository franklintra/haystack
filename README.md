# 🗂️ ImgFS - Image Filesystem inspired by Facebook Haystack

[![Build Status](https://img.shields.io/badge/build-passing-brightgreen)](https://github.com/franklintra/haystack)
[![License](https://img.shields.io/badge/license-MIT-blue)](LICENSE)
[![C](https://img.shields.io/badge/language-C-00599C)](https://en.wikipedia.org/wiki/C_(programming_language))
[![EPFL](https://img.shields.io/badge/EPFL-CS202-red)](https://www.epfl.ch/)

## 📸 Overview

**ImgFS** is an image filesystem implementation heavily inspired by **Facebook's Haystack** architecture. Based on concepts from the paper ["Finding a needle in Haystack: Facebook's photo storage"](https://www.usenix.org/legacy/event/osdi10/tech/full_papers/Beaver.pdf), this project implements a specialized filesystem optimized for storing and serving images efficiently.

### 🎯 Key Features

- **Efficient image storage and retrieval** - Optimized for photo access patterns
- **Multiple resolution support** - Automatic generation of thumbnails and small versions
- **Deduplication** - SHA-256 based image deduplication to save storage
- **HTTP web interface** - Built-in web server for image management
- **Metadata management** - Efficient tracking of image metadata

## 🏗️ Architecture

The ImgFS system implements a simplified version of Haystack's concepts:

### 📦 **Image Storage**
- Images stored in a single file with metadata headers
- Support for multiple resolutions (original, small, thumbnail)
- Efficient append-only write operations

### 📁 **Metadata Management**
- Fixed-size metadata entries for O(1) access
- SHA-256 checksums for integrity and deduplication
- Support for up to configurable maximum number of images

### ⚡ **Web Interface**
- HTTP server for image upload/download
- RESTful API for image operations
- HTML interface for browsing stored images

## 🚀 Getting Started

### Prerequisites

- GCC compiler (version 7.0 or higher)
- Make build system
- OpenSSL library (for SHA-256 hashing)
- libvips (for image processing)
- Linux/Unix environment

### Building the Project

```bash
# Clone the repository
git clone https://github.com/franklintra/haystack.git
cd haystack/done

# Build the project
make clean && make all
```

### Quick Start

```bash
# Create a new imgFS file
./imgfscmd create test.imgfs

# List contents
./imgfscmd list test.imgfs

# Start the web server
./imgfs_server test.imgfs 8080

# Access the web interface at http://localhost:8080
```

## 📝 Implementation Details

### Image Metadata Structure
```c
struct imgfs_metadata {
    char img_id[MAX_IMG_ID + 1];
    unsigned char SHA[SHA256_DIGEST_LENGTH];
    uint32_t unused_32;
    uint64_t unused_64;
    uint16_t is_valid;
    uint16_t unused_16;
    uint32_t orig_res[2];    // width and height
    uint32_t orig_size;       // in bytes
    uint64_t offset[NB_RES];  // offset in file for each resolution
    uint32_t size[NB_RES];    // size in bytes for each resolution
};
```

### File Operations
- **Create**: Initialize a new imgFS file with header and metadata
- **List**: Display all images with their properties
- **Insert**: Add new images with automatic deduplication
- **Delete**: Mark images as deleted (soft delete)
- **Read**: Retrieve images at different resolutions

## 📚 Project Structure

```
haystack/
├── done/           # Main implementation
├── provided/       # Provided course materials
├── grading/        # Grading and evaluation files
└── README.md       # This file
```

## 🤝 Contributing

Contributions are welcome! Please read our [Contributing Guidelines](CONTRIBUTING.md) before submitting PRs.

1. Fork the repository
2. Create your feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## 🎓 Academic Context

This project was developed as part of the CS-202 Computer Systems course at EPFL (École Polytechnique Fédérale de Lausanne). It serves as a practical implementation exercise for understanding distributed storage systems and systems programming concepts.

## 🙏 Acknowledgments

- Facebook Engineering for the original Haystack paper and design
- EPFL CS-202 course staff for project guidance

## 📖 References

1. Beaver, D., Kumar, S., Li, H. C., Sobel, J., & Vajgel, P. (2010). **Finding a needle in Haystack: Facebook's photo storage**. In OSDI (Vol. 10, pp. 1-8).
2. [Facebook Engineering: Needle in a Haystack](https://engineering.fb.com/2009/04/30/core-data/needle-in-a-haystack-efficient-storage-of-billions-of-photos/)
3. [High Scalability: Facebook Haystack](http://highscalability.com/blog/2009/10/13/why-are-facebook-digg-and-twitter-so-hard-to-scale.html)

---

<p align="center">
  <i>Built with ❤️ by <a href="https://github.com/franklintra">@franklintra</a></i>
</p>