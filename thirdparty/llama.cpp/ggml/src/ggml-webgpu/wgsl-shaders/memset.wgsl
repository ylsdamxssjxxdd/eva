@group(0) @binding(0)
var<storage, read_write> output_buffer: array<u32>;

struct Params {
    offset: u32, // in bytes
    size: u32,   // in bytes
    value: u32,  // 4 8-bit values, which are either repeating (memset_tensor) or may be separate (cleaning up unaligned set_tensor operations)
};

@group(0) @binding(1)
var<uniform> params: Params;

override wg_size: u32;
override bytes_per_thread: u32;

@compute @workgroup_size(wg_size)
fn main(@builtin(global_invocation_id) gid: vec3<u32>) {
    let i = gid.x * bytes_per_thread;
    let start = params.offset;
    let end = params.offset + params.size;

    for (var j: u32 = 0u; j < bytes_per_thread; j = j + 1u) {
        let byte_index = start + i + j;
        if (byte_index + 4u <= end) {
            output_buffer[(byte_index >> 2u)] = params.value;
        } else {
            // Handle tail (unaligned)
            for (var k: u32 = 0u; k < 4u; k = k + 1u) {
                let idx = byte_index + k;
                if (idx < end) {
                    let word_idx = idx >> 2u;
                    let byte_offset = (idx & 3u) * 8u;
                    let mask = ~(0xffu << byte_offset);
                    let existing = output_buffer[word_idx];
                    output_buffer[word_idx] = (existing & mask) | ((params.value & 0xffu) << byte_offset);
                }
            }
        }
    }
}
