// Fuzz the OpenVDB .vdb stream reader: openvdb::io::Stream(std::istream&) parses the
// archive header, metadata, grid descriptors, transforms and tree topology/buffers.
#include <openvdb/openvdb.h>
#include <openvdb/io/Stream.h>
#include <cstdint>
#include <sstream>
#include <string>

extern "C" int LLVMFuzzerInitialize(int*, char***) {
    openvdb::initialize();
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    std::istringstream is(std::string(reinterpret_cast<const char*>(data), size),
                          std::ios_base::binary);
    try {
        openvdb::io::Stream stream(is);
        openvdb::MetaMap::Ptr meta = stream.getMetadata();
        if (meta) (void)meta->str();
        openvdb::GridPtrVecPtr grids = stream.getGrids();
        if (grids) {
            std::ostringstream sink;
            for (const auto& g : *grids) {
                if (!g) continue;
                g->print(sink, /*verboseLevel=*/3);
                (void)g->activeVoxelCount();
                (void)g->evalActiveVoxelBoundingBox();
                (void)g->memUsage();
            }
        }
    } catch (const std::exception&) {
        // openvdb::Exception and std I/O errors are the reader's way of rejecting input.
    }
    return 0;
}
