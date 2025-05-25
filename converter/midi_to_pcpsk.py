import mido
import struct

def midi_to_freq(note):
    return int(440.0 * (2 ** ((note - 69) / 12.0)))

def parse_midi(filename):
    mid = mido.MidiFile(filename)
    ticks_per_beat = mid.ticks_per_beat
    tempo = 500000  # default 120 BPM

    melody = []
    note_start_info = {}   # (note, channel) -> start_tick
    current_notes = set()

    # Flatten and sort all track events by absolute tick
    events = []
    for track in mid.tracks:
        abs_tick = 0
        for msg in track:
            abs_tick += msg.time
            events.append((abs_tick, msg))
    events.sort(key=lambda x: x[0])

    for abs_tick, msg in events:
        if msg.type == 'set_tempo':
            tempo = msg.tempo

        elif msg.type == 'note_on' and msg.velocity > 0:
            note_start_info[(msg.note, msg.channel)] = abs_tick
            current_notes.add((msg.note, msg.channel))

        elif msg.type == 'note_off' or (msg.type == 'note_on' and msg.velocity == 0):
            key = (msg.note, msg.channel)
            if key in note_start_info:
                start_tick = note_start_info.pop(key)
                duration_ticks = abs_tick - start_tick
                dur_s = mido.tick2second(duration_ticks, ticks_per_beat, tempo)
                dur_ms = max(int(dur_s * 1000), 1)
                current_notes.discard(key)

                # find the highest note still playing (or this one if none)
                if current_notes:
                    highest = max([n for n,c in current_notes] + [msg.note])
                else:
                    highest = msg.note

                if msg.note == highest:
                    freq = midi_to_freq(msg.note)
                    melody.append((freq, dur_ms))

    return melody

def write_c_array(melody, out_file="melody.c"):
    with open(out_file, "w") as f:
        f.write("int melody[] = {\n")
        for freq, dur in melody:
            f.write(f"    {freq}, {dur},\n")
        f.write("};\n")

def write_binary_array(melody, out_file="melody.bin"):
    """
    Writes the melody as a sequence of uint32_t values:
      freq0, dur0, freq1, dur1, ...
    Each as little-endian 4-byte ints.
    """
    with open(out_file, "wb") as f:
        for freq, dur in melody:
            f.write(struct.pack('<II', freq, dur))

if __name__ == "__main__":
    import sys
    if len(sys.argv) < 2:
        print("Usage: python midi_to_pcspk.py <file.mid>")
        sys.exit(1)

    melody = parse_midi(sys.argv[1])
    write_c_array(melody, "melody.c")
    write_binary_array(melody, "melody.bin")
    print("Generated melody.c and melody.bin!")