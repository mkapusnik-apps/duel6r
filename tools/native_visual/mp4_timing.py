"""Narrow, source-table-backed timing for one ordinary, unfragmented AVC MP4.

This is not a general MP4 demuxer. FFprobe still owns packet/sample mapping.
Unsupported tables/edits fail closed rather than estimating a frame lifetime.
"""
from fractions import Fraction
import json
import struct

from .common import require


LIMIT = 72000


def tables(source):
    with source.open('rb') as stream:
        stream.seek(0, 2)
        size = stream.tell()
        require(0 < size < 104857600, 'unsupported MP4 size')

        def boxes(start, end):
            result = []
            while start < end:
                require(end - start >= 8, 'truncated MP4 box')
                stream.seek(start)
                length, kind = struct.unpack('>I4s', stream.read(8))
                header = 8
                if length == 1:
                    require(end - start >= 16, 'truncated extended MP4 box')
                    length = struct.unpack('>Q', stream.read(8))[0]
                    header = 16
                elif length == 0:
                    length = end - start
                require(header <= length <= end - start, 'MP4 box outside parent')
                result.append((kind, start + header, start + length))
                require(len(result) <= LIMIT, 'too many MP4 boxes')
                start += length
            return result

        def one(entries, kind, optional=False):
            found = [entry for entry in entries if entry[0] == kind]
            require(len(found) == 1 or (optional and not found), 'missing/duplicate MP4 ' + repr(kind))
            return found[0] if found else None

        def children(entry):
            return boxes(entry[1], entry[2])

        def payload(entry):
            require(entry[2] - entry[1] <= 4 * 1024 * 1024, 'oversized timing metadata')
            stream.seek(entry[1])
            data = stream.read(entry[2] - entry[1])
            require(len(data) == entry[2] - entry[1], 'truncated timing metadata')
            return data

        def full(entry, versions=(0,)):
            data = payload(entry)
            require(len(data) >= 4 and data[0] in versions and data[1:4] == b'\0\0\0',
                    'unsupported MP4 timing version/flags')
            return data

        def clock(entry):
            data = full(entry, (0, 1))
            offset = 20 if data[0] else 12
            duration_bytes = 8 if data[0] else 4
            require(len(data) >= offset + 4 + duration_bytes, 'truncated MP4 clock')
            scale = int.from_bytes(data[offset:offset+4], 'big')
            duration = int.from_bytes(data[offset+4:offset+4+duration_bytes], 'big')
            require(scale > 0 and duration > 0, 'invalid MP4 clock')
            return scale, duration

        def runs(entry, composition=False):
            data = full(entry, (0, 1) if composition else (0,))
            require(len(data) >= 8, 'truncated sample timing')
            count = int.from_bytes(data[4:8], 'big')
            require(0 < count <= LIMIT and len(data) == 8 + count * 8, 'invalid sample timing entries')
            values = []
            for offset in range(8, len(data), 8):
                number = int.from_bytes(data[offset:offset+4], 'big')
                duration = int.from_bytes(data[offset+4:offset+8], 'big', signed=composition and data[0] == 1)
                require(0 < number <= LIMIT - len(values), 'invalid sample timing count')
                require(composition or duration > 0, 'nonpositive STTS duration')
                values.extend([duration] * number)
            return values

        root = boxes(0, size)
        require(not any(box[0] == b'moof' for box in root), 'fragmented MP4 unsupported')
        one(root, b'ftyp')
        media = [box for box in root if box[0] == b'mdat']
        require(bool(media), 'missing MP4 media')
        movie = children(one(root, b'moov'))
        require(not any(box[0] == b'mvex' for box in movie), 'fragmented timing unsupported')
        movie_scale, _ = clock(one(movie, b'mvhd'))
        track = children(one(movie, b'trak'))
        tkhd = payload(one(track, b'tkhd'))
        require(len(tkhd) >= 24 and tkhd[0] in (0, 1), 'unsupported track identity')
        identity_offset = 20 if tkhd[0] else 12
        track_id = int.from_bytes(tkhd[identity_offset:identity_offset+4], 'big')
        mdia = children(one(track, b'mdia'))
        handler = full(one(mdia, b'hdlr'))
        require(handler[8:12] == b'vide', 'not a single video track')
        scale, declared_duration = clock(one(mdia, b'mdhd'))
        minf = children(one(mdia, b'minf'))
        dref = payload(one(children(one(minf, b'dinf')), b'dref'))
        require(dref == b'\0\0\0\0\0\0\0\1\0\0\0\x0curl \0\0\0\1',
                'external/unsupported media reference')
        stbl = children(one(minf, b'stbl'))
        stsd = full(one(stbl, b'stsd'))
        require(len(stsd) >= 24 and int.from_bytes(stsd[4:8], 'big') == 1
                and stsd[12:16] == b'avc1' and stsd[22:24] == b'\0\1',
                'unsupported sample description')
        durations = runs(one(stbl, b'stts'))
        composition = one(stbl, b'ctts', optional=True)
        offsets = runs(composition, True) if composition else [0] * len(durations)
        require(len(offsets) == len(durations), 'CTTS/STTS count mismatch')
        # mdhd may include composition preroll; it need not equal the decode
        # duration. A shorter declaration could cap FFprobe's stream duration.
        # Never use this scalar to synthesize the final sample lifetime.
        require(declared_duration >= sum(durations), 'mdhd caps source STTS sum')
        stsz = full(one(stbl, b'stsz'))
        require(len(stsz) >= 12 and int.from_bytes(stsz[8:12], 'big') == len(durations),
                'sample size/timing count mismatch')
        require(len(stsz) == (12 if int.from_bytes(stsz[4:8], 'big') else 12 + 4*len(durations)),
                'truncated sample size table')
        edit = None
        edts = one(track, b'edts', optional=True)
        if edts:
            data = full(one(children(edts), b'elst'), (0, 1))
            require(len(data) >= 8 and int.from_bytes(data[4:8], 'big') == 1, 'multiple/empty edits unsupported')
            require(len(data) == (28 if data[0] else 20), 'invalid edit length')
            width = 8 if data[0] else 4
            length = int.from_bytes(data[8:8+width], 'big')
            start = int.from_bytes(data[8+width:8+2*width], 'big', signed=True)
            require(length > 0 and start >= 0 and data[-4:] == b'\0\1\0\0', 'non-unit/empty edit unsupported')
            edit = (start, Fraction(length * scale, movie_scale))
        return dict(track_id=track_id, scale=scale, duration=sum(durations), mdhd_duration=declared_duration, durations=durations,
                    offsets=offsets, edit=edit, media=[(box[1], box[2]) for box in media])


def recover(source, decoded, command):
    timing = tables(source)
    entries = ('stream=index,id,codec_name,time_base,start_pts,duration_ts,nb_frames:'
               'packet=stream_index,pts,dts,duration,pos,size,flags')
    args = ['ffprobe', '-v', 'error', '-fflags', '+nofillin', '-select_streams', 'v',
            '-show_streams', '-show_packets', '-show_entries', entries, '-of', 'json']
    normal = json.loads(command(args + [str(source)]))
    raw = json.loads(command(args + ['-ignore_editlist', '1', str(source)]))
    n = len(timing['durations'])
    require(n == len(decoded['frames']), 'decoded/source sample count mismatch')
    for probe in (normal, raw):
        require(len(probe['streams']) == 1 and len(probe['packets']) == n, 'packet/source count mismatch')
        stream = probe['streams'][0]
        require(stream['codec_name'] == 'h264' and int(stream['id'], 0) == timing['track_id'], 'track identity mismatch')
        require(Fraction(stream['time_base']) == Fraction(1, timing['scale'])
                == Fraction(decoded['streams'][0]['time_base']), 'source time base mismatch')
        require(int(stream['nb_frames']) == n and stream['duration_ts'] == timing['duration'],
                'source stream count/duration contradicts STTS')
    require(all(p.get('duration') in (None, 'N/A') for p in normal['packets']),
            'packet durations present; unsupported missing frame duration path')
    original, packets = raw['packets'], normal['packets']
    raw_dts = [p.get('dts') for p in original]
    require(all(type(value) is int and abs(value) <= 2**53-1 for value in raw_dts)
            and raw_dts[0] == 0, 'unsupported raw decode timeline')
    recovered = [b-a for a,b in zip(raw_dts, raw_dts[1:])] + [timing['duration'] - raw_dts[-1]]
    require(recovered == timing['durations'], 'DTS deltas disagree with complete source STTS')
    shift = timing['edit'][0] if timing['edit'] else 0
    seen = set()
    for index, (a, b, duration) in enumerate(zip(original, packets, recovered)):
        require(all(a.get(key) == b.get(key) for key in ('stream_index', 'pos', 'size', 'flags')),
                'edited/raw packet identity/order mismatch')
        # FFprobe exposes two or three flag positions depending on its version;
        # only key/non-key with every other flag clear is supported.
        require(a.get('flags') in ('__', 'K_', '___', 'K__'), 'discarded/corrupt/unsupported packet flags')
        pos, size = int(a['pos']), int(a['size'])
        require(size > 0 and (pos,size) not in seen
                and any(start <= pos and pos+size <= end for start,end in timing['media']), 'invalid packet media bounds')
        seen.add((pos,size))
        require(all(type(p.get(key)) is int and abs(p[key]) <= 2**53-1
                    for p in (a,b) for key in ('pts','dts')), 'missing/noninteger packet timing')
        require(a['pts'] == a['dts'] + timing['offsets'][index]
                and a['pts']-b['pts'] == a['dts']-b['dts'] == shift, 'ambiguous edit/CTTS mapping')
        if timing['edit']:
            start, length = timing['edit']
            require(start <= a['pts'] and a['pts']+duration <= start+length, 'edit clips presentation sample')
    ordered = sorted(zip(packets, recovered), key=lambda item: item[0]['pts'])
    require([p['pts'] for p,d in ordered] == [f['pts'] for f in decoded['frames']], 'decoded/presentation packet map mismatch')
    for frame, (packet, duration) in zip(decoded['frames'], ordered):
        known = frame.get('duration', frame.get('pkt_duration'))
        require(known in (None, 'N/A') or known == duration, 'frame/source duration conflict')
    return dict(durations=[d for p,d in ordered], method='MP4 STTS + raw packet DTS + CTTS/edit identity',
                track_id=timing['track_id'], sample_count=n, stts_sum=timing['duration'], mdhd_duration=timing['mdhd_duration'],
                edit_shift=shift, time_base=f"1/{timing['scale']}", acquisition_time_claim=False)
