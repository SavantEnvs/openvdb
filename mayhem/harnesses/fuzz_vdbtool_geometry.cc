// Fuzz vdb_tool's mesh/point-cloud file readers through their own entry point,
// openvdb::vdb_tool::Geometry::read(fileName), which dispatches on the file extension to
// readOBJ / readPLY / readPTS / readSTL / readGEO / readOFF / readXYZ (all parse untrusted
// files given to `vdb_tool -read`). The first input byte picks the extension; the rest is
// the file, written to a scratch file under $TMPDIR (STL and PTS have no stream reader).
// .vdb/.nvdb are covered by the other targets; ABC/USD/glTF/PDAL need optional deps.
#include "Geometry.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <string>
#include <unistd.h>

static const char* const kExt[] = {".obj", ".ply", ".pts", ".stl", ".geo", ".off", ".xyz"};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 1) return -1;
    const char* ext = kExt[data[0] % (sizeof(kExt) / sizeof(kExt[0]))];
    const char* tmp = std::getenv("TMPDIR");
    std::string path = std::string(tmp && *tmp ? tmp : "/tmp") + "/vdbtool-geomXXXXXX" + ext;
    int fd = mkstemps(&path[0], static_cast<int>(std::char_traits<char>::length(ext)));
    if (fd < 0) return 0;
    const uint8_t* p = data + 1;
    size_t left = size - 1;
    while (left > 0) {
        ssize_t w = write(fd, p, left);
        if (w <= 0) break;
        p += w; left -= static_cast<size_t>(w);
    }
    close(fd);
    try {
        openvdb::vdb_tool::Geometry geom;
        geom.read(path);
        (void)geom.vtxCount(); (void)geom.polyCount();
        if (!geom.isEmpty()) (void)geom.bbox();
    } catch (const std::exception&) {}
    unlink(path.c_str());
    return 0;
}
