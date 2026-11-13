#!/usr/bin/env python3
"""What the logs of AutoStory runs say about the video: does the picture fall
behind while a stream runs, and do the frames keep coming?

Two things other controller programs' users run into (see
Source/PocketPilot/OTHERS.md): a picture that gets later the longer the
program runs, and a capture card that drops frames. Both can be read from
the log (SerialPrograms.log) afterwards.

1. The delay. For every walk the log has the d-pad press ("issue_dpad():
   left, delay = ..., hold = ...") and the moment the walk was seen finished
   in the video ("Walked left: 4 of 4 - ..."). For equal walks (as many
   tiles, the same hold) the time between the end of the hold and that
   moment is the same, plus how late the picture is. So: per video stream
   (from one "Starting Camera" to the next), per hour since it started, the
   median of that time against the stream's own median for equal walks. A
   stream that falls behind shows as a number that grows with the hours.
   One look of the walk check is 50 ms, so single hours can be off by that.

2. The frames. Once a minute (not every minute is in the log) the video
   source reports how many frames came in: "VideoSource::report_source_frame():
   Count = 3,601". 3,600 is 60 frames a second.

Usage: python3 log_video_check.py SerialPrograms.log [more logs ...]
The same log sent twice, grown in between, counts once.
"""
import collections
import datetime
import re
import statistics
import sys

LINE = re.compile(r'^\ufeff?(\d{4}-\d\d-\d\d \d\d:\d\d:\d\d)\.(\d{6}) - \[([^\]]*)\]: (.*)$')
DPAD = re.compile(r'^issue_dpad\(\): (\w+), delay = (\d+)ms, hold = (\d+)ms')
WALK = re.compile(r'^Walked (\w+): (\d+) of (\d+) - ')
FRAMES = re.compile(r'^VideoSource::report_source_frame\(\): Count = ([\d,]+)')


def read(path, streams, minutes):
    camera = None
    press = None
    with open(path, 'r', errors='replace') as log:
        for line in log:
            if ('issue_dpad' not in line and 'Walked ' not in line
                    and 'Starting Camera' not in line and 'report_source_frame' not in line):
                continue
            match = LINE.match(line)
            if not match:
                continue
            body = match.group(4)
            stamp = datetime.datetime.strptime(match.group(1) + '.' + match.group(2), '%Y-%m-%d %H:%M:%S.%f')
            if body.startswith('Starting Camera'):
                camera = stamp
                press = None
                streams.setdefault(camera, {})
                continue
            frames = FRAMES.match(body)
            if frames:
                minutes[match.group(1)] = int(frames.group(1).replace(',', ''))
                continue
            dpad = DPAD.match(body)
            if dpad:
                press = (stamp, dpad.group(1), int(dpad.group(3)))
                continue
            walked = WALK.match(body)
            if walked and press is not None and camera is not None:
                pressed, direction, hold = press
                press = None
                if walked.group(1) != direction or walked.group(2) != walked.group(3):
                    continue        # another direction, or not the whole run
                after = (stamp - pressed).total_seconds() * 1000.0 - hold
                if 0 <= after <= 15000:
                    streams[camera][pressed] = (int(walked.group(2)), hold, after)


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    streams = {}        # start of the camera -> {time of the press: (tiles, hold, ms after the hold)}
    minutes = {}        # the second of the report -> frames in that minute
    for path in sys.argv[1:]:
        read(path, streams, minutes)

    print('1. The delay, per video stream')
    rows = []
    for camera, walks in streams.items():
        groups = collections.defaultdict(list)
        for pressed, (tiles, hold, after) in walks.items():
            groups[(tiles, hold)].append((pressed, after))
        against = []        # (hours since the camera started, ms against the median of equal walks)
        for items in groups.values():
            if len(items) < 20:
                continue
            median = statistics.median(after for _, after in items)
            for pressed, after in items:
                against.append(((pressed - camera).total_seconds() / 3600.0, after - median))
        if len(against) < 100:
            continue
        hours = collections.defaultdict(list)
        for hour, value in against:
            hours[int(hour)].append(value)
        medians = {hour: statistics.median(values) for hour, values in hours.items() if len(values) >= 15}
        if len(medians) >= 2:
            rows.append((max(hour for hour, _ in against), camera, len(against), medians))
    rows.sort(reverse=True)
    every = []
    for span, camera, count, medians in rows:
        every.extend(medians.values())
        print('   camera started %s, last walk %5.1f h later, %5d walks | %s' % (
            camera.strftime('%Y-%m-%d %H:%M'), span, count,
            ' '.join('%dh:%+.0f' % (hour, medians[hour]) for hour in sorted(medians))))
    if every:
        print('   %d of the %d streams with walks had enough equal walks in two or more hours: %.0f hours together.' % (
            len(rows), sum(1 for walks in streams.values() if walks), sum(row[0] for row in rows)))
        print('   %d hourly medians: %d within 5 ms of the stream\'s own, %d within 10 ms, the largest %.0f ms.' % (
            len(every), sum(1 for v in every if abs(v) <= 5), sum(1 for v in every if abs(v) <= 10),
            max(abs(v) for v in every)))
    else:
        print('   not enough walks in these logs')

    print()
    print('2. The frames, per day (3,600 a minute is 60 a second)')
    days = collections.defaultdict(list)
    for stamp, count in minutes.items():
        days[stamp[:10]].append(count)
    print('   day          minutes   under 3,500   under 3,590    lowest')
    for day in sorted(days):
        counts = days[day]
        print('   %s   %6d   %10d   %10d   %7d' % (
            day, len(counts), sum(1 for c in counts if c < 3500), sum(1 for c in counts if c < 3590), min(counts)))
    return 0


if __name__ == '__main__':
    sys.exit(main())
