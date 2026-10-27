import struct
import sys
import csv

block_size = 512 # number of bytes in there
magic = 0xADC5
sample_size = 84 # number of samples in each block

# magic(H) + seq(H) + t_us(I) + 84*3 int16 (h) = 2+2+4+504 = 512 bytes
BLOCK_FORMAT = "<HHI" + "h" * (sample_size * 3)


def PARSE_LOGBLOCK(in_path, out_path):
    rows_written = 0
    blocks_read = 0
    blocks_skipped = 0
    last_seq = None
    gaps_found = 0

    with open(in_path, "rb") as f_in, open(out_path, "w", newline="") as f_out:
        writer = csv.writer(f_out) # open the csv file writer
        writer.writerow(["block_seq", "t_us", "sample_index_in_block", "x", "y", "z"]) # block number, timestamp, index in block, x val, y val, z val

        while True:

            chunk = f_in.read(block_size)
            if len(chunk) < block_size: # catch weird stuff like blocks that are too small
                if len(chunk) > 0:
                    print(f"WARN: {len(chunk)} trailing bytes at end of file")
                break

            fields = struct.unpack(BLOCK_FORMAT, chunk)
            magic, seq, t_us = fields[0], fields[1], fields[2]
            samples = fields[3:] # 252 int16 values, 3 per sample

            if magic != magic:
                blocks_skipped += 1
                continue  # not a valid block, probably stale/unwritten space idk

            if last_seq is not None and seq != (last_seq + 1) & 0xFFFF: # catch random gaps in data
                gap = (seq - last_seq - 1) & 0xFFFF
                print(f"WARN: Gap detected with {gap} block(s) missing between seq {last_seq} and {seq}")
                gaps_found += gap
            last_seq = seq

            for i in range(sample_size):
                x = samples[i * 3] 
                y = samples[i * 3 + 1]
                z = samples[i * 3 + 2]
                writer.writerow([seq, t_us, i, x, y, z])
                rows_written += 1

            blocks_read += 1

    print(f"Parsed {blocks_read} valid blocks ({rows_written} sample rows) and outputted into {out_path}")
    if blocks_skipped:
        print(f"Skipped {blocks_skipped} blocks with invalid magic (corrupt/unwritten space)")
    if gaps_found:
        print(f"Total missing blocks across gaps: {gaps_found} ({gaps_found * sample_size} samples lost)")


if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python3 parsingV1.py <input.bin> <output.csv>") 
        sys.exit(1)

    PARSE_LOGBLOCK(sys.argv[1], sys.argv[2])
