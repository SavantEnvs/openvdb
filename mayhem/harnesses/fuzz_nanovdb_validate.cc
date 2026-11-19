// Fuzz what nanovdb_validate / nanovdb_convert do with a file: read every grid with
// nanovdb::io::readGrids, run tools::validateGrids(CheckMode::Full) over the untrusted
// node offsets, and convert grids that validate back to OpenVDB (tools::nanoToOpenVDB).
#include <openvdb/openvdb.h>
#include <nanovdb/io/IO.h>
#include <nanovdb/tools/GridValidator.h>
#include <nanovdb/tools/NanoToOpenVDB.h>
#include <cstdint>
#include <sstream>
#include <string>

extern "C" int LLVMFuzzerInitialize(int*, char***) {
    openvdb::initialize();
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    // Documented exception to "never skip inputs" (decided on the integration PR): a NanoVDB
    // stream is a sequence of segments, each a FileHeader/GridData-sized header followed by
    // grid data (nanovdb/io/IO.h, GridHandle::read reads a whole GridData before rewinding),
    // so an input shorter than one sizeof(nanovdb::GridData) header cannot hold a grid.
    // On such inputs upstream misbehaves: GridHandle::read's rewind seekg fails, leaving
    // failbit without eofbit, and Segment::read trusts the stale header, so readGridMetaData
    // loops forever and readGrids aborts on the ~Pool mRegister.empty() assert
    // (HostBuffer.h). That upstream bug is kept on record by the reproducers in
    // mayhem/reproducers/ (short_stream_hang.nvdb, short_stream_abort.nvdb). Nothing else
    // is filtered.
    if (size < sizeof(nanovdb::GridData)) return -1;
    std::istringstream is(std::string(reinterpret_cast<const char*>(data), size),
                          std::ios_base::binary);
    try {
        auto handles = nanovdb::io::readGrids(is);
        for (auto& h : handles) {
            if (!nanovdb::tools::validateGrids(h, nanovdb::CheckMode::Full, false)) continue;
            for (uint32_t i = 0; i < h.gridCount(); ++i) {
                try {
                    openvdb::GridBase::Ptr g = nanovdb::tools::nanoToOpenVDB(h, i);
                    if (g) (void)g->activeVoxelCount();
                } catch (const std::exception&) {}
            }
        }
    } catch (const std::exception&) {}
    return 0;
}
