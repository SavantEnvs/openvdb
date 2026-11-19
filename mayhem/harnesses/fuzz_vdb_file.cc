// Fuzz the OpenVDB .vdb file reader as vdb_print does: openvdb::io::File::open() parses
// the header and grid descriptors, then readGridMetadata()/readGrid() seek to each grid
// by its stored offsets (paths io::Stream never takes).
#include <openvdb/openvdb.h>
#include <openvdb/io/File.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <string>
#include <unistd.h>

static std::string g_path;

extern "C" int LLVMFuzzerInitialize(int*, char***) {
    openvdb::initialize();
    const char* tmp = getenv("TMPDIR");
    std::string tmpl = std::string(tmp && *tmp ? tmp : "/tmp") + "/fuzz_vdb_file_XXXXXX";
    int fd = mkstemp(&tmpl[0]);
    if (fd < 0) abort();
    close(fd);
    g_path = tmpl;
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    FILE* f = fopen(g_path.c_str(), "wb");
    if (!f) abort();
    if (size && fwrite(data, 1, size, f) != size) abort();
    fclose(f);
    try {
        openvdb::io::File file(g_path);
        file.open();
        std::ostringstream sink;
        if (auto meta = file.getMetadata()) sink << meta->str();
        for (auto it = file.beginName(); it != file.endName(); ++it) {
            const std::string name = it.gridName();
            try {
                if (auto gm = file.readGridMetadata(name)) gm->print(sink, 1);
                if (auto g = file.readGrid(name)) {
                    g->print(sink, 3);
                    (void)g->activeVoxelCount();
                    (void)g->evalActiveVoxelBoundingBox();
                }
            } catch (const std::exception&) {}
        }
        file.close();
    } catch (const std::exception&) {
    }
    return 0;
}
