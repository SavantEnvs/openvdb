// Fuzz the NanoVDB .nvdb reader: nanovdb::io::readGrids(std::istream&) (Segment::read,
// FileGridMetaData::read, Internal::read with the zlib codec) and GridHandle::read.
#include <nanovdb/io/IO.h>
#include <nanovdb/HostBuffer.h>
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
    const std::string buf(reinterpret_cast<const char*>(data), size);
    try {
        std::istringstream is(buf, std::ios_base::binary);
        auto handles = nanovdb::io::readGrids(is);
        for (auto& h : handles) {
            for (uint32_t i = 0; i < h.gridCount(); ++i) {
                const nanovdb::GridMetaData* m = h.gridMetaData(i);
                if (m) { (void)m->shortGridName(); (void)m->activeVoxelCount(); }
            }
        }
    } catch (const std::exception&) {}
    try {
        std::istringstream is(buf, std::ios_base::binary);
        nanovdb::GridHandle<nanovdb::HostBuffer> h;
        h.read(is);
        (void)h.gridCount();
    } catch (const std::exception&) {}
    return 0;
}
