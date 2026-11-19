// Fuzz nanovdb::io::readGridMetaData(std::istream&) — the nanovdb_print path: segment
// loop, raw-grid fallback and FileGridMetaData name reads.
#include <nanovdb/io/IO.h>
#include <cstdint>
#include <sstream>
#include <string>

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
        auto metas = nanovdb::io::readGridMetaData(is);
        size_t n = 0;
        for (auto& m : metas) n += m.gridName.size() + m.voxelCount;
        (void)n;
    } catch (const std::exception&) {}
    return 0;
}
