import sys
import wave
import math
import random
import time
from array import array
import itertools

framerate = 48000


def _samples_data(channels):
    """Return an array of samples data"""

    nchannels = len(channels)
    assert 1 <= nchannels <= 2

    # convert from [-1,1] to [-2**15+1, 2**15-1]
    k = float(2**15 - 1)
    if nchannels == 1:
        samples = channels[0]
    else:
        samples = itertools.chain.from_iterable(zip(*channels))
    # assume reasonnably small files, that fit in memory
    return array('h', (int(k * a) for a in samples))

def write_wav(output, channels):
    """Write a WAV file from channel samples"""

    nchannels = len(channels)
    data = _samples_data(channels)

    w = wave.open(output, 'w')
    w.setnchannels(nchannels)
    w.setsampwidth(2)  # 16-bit audio
    w.setframerate(framerate)
    w.setnframes(len(data) / nchannels)
    w.writeframesraw(data.tostring())
    w.close()

def _play_ossaudiodev(channels):
    dev = ossaudiodev.open('w')
    dev.setfmt(ossaudiodev.AFMT_S16_LE)
    dev.setchannels(len(channels))
    dev.speed(framerate)
    dev.writeall(_samples_data(channels).tostring())
    dev.close()

def _play_winsound(channels):
    import winsound
    from cStringIO import StringIO
    buf = StringIO()
    write_wav(buf, channels)
    winsound.PlaySound(buf.getvalue(), winsound.SND_MEMORY)

def _play_unavailable(channels):
    raise NotImplementedError("playing sounds not supported on this platform")

try:
    import ossaudiodev
    play = _play_ossaudiodev
except ImportError:
    try:
        import winsound
        play = _play_winsound
    except ImportError:
        play = _play_unavailable


def play_sequence(seq, dt=0.3):
    for s in seq:
        time.sleep(dt)
        s.play()

def merge_sequence(seq, dt=0.1):
    silence = Sound(0 for i in range(int(dt * framerate)))
    tsilence = Track((silence, silence))
    result = Track((Sound([]), Sound([])))
    for s in seq:
        result += tsilence
        result += s
    result += tsilence
    return result


def draw(channels):
    import matplotlib.pyplot as plt
    for samples in channels:
        plt.plot(samples)
    plt.xlim([0, max(len(samples) for samples in channels) - 1])
    plt.ylim([-1,1])
    plt.axhline(y=0, color='gray')
    plt.show()


def _to_samples(o):
    if isinstance(o, Sound):
        return o.samples
    else:
        return list(o)

def _to_sample_count(o):
    if isinstance(o, (float, int, long)):
        return int(o * framerate)
    else:
        return len(o)


class Sound:
    def __init__(self, samples=None):
        if samples is None:
            self.samples = []
        else:
            self.samples = list(samples)

    def write(self, output):
        write_wav(output, [self.samples])

    def play(self):
        play([self.samples])

    def draw(self):
        draw([self.samples])


    def enveloppe(self, signal, pieces):
        """Apply a signal to enveloppe samples
        pieces is a list of (position, amp_ratio).
        """
        return self & signal(pieces, dur=self)

    def amplify(self, k):
        return Sound(k*v for v in self.samples)


    def copy(self):
        return Sound(self.samples)

    def __len__(self):
        return len(self.samples)

    def __iter__(self):
        return iter(self.samples)

    @property
    def duration(self):
        return float(len(self.samples)) / framerate

    def __repr__(self):
        return "<%s len=%d>" % (self.__class__.__name__, len(self.samples))

    def __add__(self, other):
        return Sound(self.samples + _to_samples(other))

    def __radd__(self, other):
        return Sound(_to_samples(other) + self.samples)

    def __iadd__(self, other):
        self.samples += _to_samples(other)
        return self

    def __mul__(self, k):
        return Sound(self.samples * k)

    def __rmul__(self, k):
        return Sound(k * self.samples)

    def __imul__(self, k):
        self.samples *= k
        return self

    def __or__(self, other):
        return Sound(a + b for a, b in zip(self.samples, _to_samples(other)))

    __ror__ = __or__

    def __and__(self, other):
        return Sound(a * b for a, b in zip(self.samples, _to_samples(other)))

    __rand__ = __and__


class Wave(Sound):
    def __init__(self, freq, amp=0.5, dur=None, cycles=None, **kwargs):
        self.freq = freq
        self.amp = amp
        if (dur is None) == (cycles is None):
            raise ValueError("either dur or cycles must be set")
        if cycles is None:
            cycles = _to_sample_count(dur) / int(framerate / freq)
        self.samples = self.wave_period_samples(**kwargs) * cycles

    def __repr__(self):
        return "<%s f=%r, a=%r, len=%d>" % (self.__class__.__name__, self.freq, self.amp, len(self.samples))

    def wave_period_samples(self):
        raise NotImplementedError()

class WaveSin(Wave):
    def wave_period_samples(self):
        period = int(framerate / self.freq)
        return [self.amp * math.sin(2 * math.pi * float(t) / period) for t in range(period)]

class WaveSquare(Wave):
    def wave_period_samples(self):
        period2 = int(framerate / self.freq / 2)
        ret = [self.amp for t in range(period2)]
        return ret + [-x for x in ret]

class WaveSawtooth(Wave):
    def wave_period_samples(self):
        period2 = int(framerate / self.freq / 2)
        k = self.amp / period2
        ret = [t * k for t in range(period2)]
        return ret + [-x for x in ret[::-1]]

class WaveTriangle(Wave):
    def wave_period_samples(self):
        period4 = int(framerate / self.freq / 4)
        k = self.amp / period4
        ticks = range(period4)
        ret = [t * k for t in ticks] + [self.amp + (t * -k) for t in ticks]
        return ret + [-x for x in ret]

class WaveTrapezium(Wave):
    def wave_period_samples(self, crest=0.5):
        assert 0.0 <= crest <= 1.0
        period2 = int(framerate / self.freq / 2)
        pattack = int(period2 * (1 - crest) / 2)
        pcrest = period2 - 2 * pattack
        k = self.amp / pattack
        tattack = range(pattack)
        tcrest = range(pcrest)
        ret = ([t * k for t in tattack]
            + [self.amp for t in tcrest]
            + [self.amp + (t * -k) for t in tattack])
        return ret + [-x for x in ret]


class Signal(Sound):
    def __init__(self, pieces, dur=None):
        self.pieces = pieces
        if dur is not None:
            # pieces contains positions (in [0, 1]), not ticks
            n = _to_sample_count(dur)
            pieces = [(int(p * n), a) for p, a in pieces]

        self.samples = []
        i0, a0 = 0, 0.0
        for i1, a1 in pieces:
            a1 = float(a1)
            assert i1 >= i0
            if i1 > i0:
                self.samples.extend(self.signal_piece_samples(a0, a1, i1 - i0))
            i0, a0 = i1, a1

    def __repr__(self):
        return "<%s len=%d %r>" % (self.__class__.__name__, len(self.samples), self.pieces)

    def signal_piece_samples(self, a0, a1, n):
        raise NotImplementedError()

class SignalTriangle(Signal):
    def signal_piece_samples(self, a0, a1, n):
        return (a0 + t * float(a1 - a0) / n for t in range(n))

class SignalSin(Signal):
    def signal_piece_samples(self, a0, a1, n):
        y = (a1 + a0) / 2
        r = (a1 - a0) / 2
        return (y - r * math.cos(t * math.pi / n) for t in range(n))


class Noise(Sound):
    def __init__(self, amp, dur, seed=None):
        if seed is not None:
            random.seed(seed)
        n = _to_sample_count(dur)
        self.amp = amp
        self.samples = [random.uniform(-amp, amp) for t in range(n)]

class NoiseSignal(Sound):
    def __init__(self, signal, freq, amp=0.5, dur=None, seed=None):
        self.freq = freq
        self.amp = amp
        self.seed = seed
        if seed is not None:
            random.seed(seed)

        nsamples = _to_sample_count(dur)
        period = int(framerate / self.freq)
        pieces = [(i, random.uniform(-amp, amp)) for i in range(0, nsamples, period)]
        self.samples = signal(pieces).samples

    def __repr__(self):
        return "<%s f=%r, a=%r, seed=%r, len=%d>" % (self.__class__.__name__, self.freq, self.amp, self.seed, len(self.samples))


class Silence(Sound):
    def __init__(self, dur):
        self.samples = [0 for i in range(_to_sample_count(dur))]


class Track:
    def __init__(self, channels=None, n=None):
        if channels is None and n is None:
            raise ValueError("either channels or n must be set")
        if channels is None:
            self.channels = tuple(Sound() for i in range(n))
        elif n is not None:
            if not isinstance(channels, Sound):
                channels = Sound(channels)
            self.channels = tuple(channels.copy() for i in range(n))
        else:
            self.channels = tuple(c.copy() if isinstance(c, Sound) else Sound(c) for c in channels)

    def samples(self):
        return [ch.samples for ch in self.channels]

    def write(self, output):
        write_wav(output, self.samples())

    def play(self):
        play(self.samples())

    def draw(self):
        draw(self.samples())

    def __repr__(self):
        return "<%s %r>" % (self.__class__.__name__, self.channels)

    def __add__(self, other):
        if isinstance(other, Track):
            return Track([c + o for c, o in zip(self.channels, other.channels)])
        return Track([c + other for c in self.channels])

    def __radd__(self, other):
        if isinstance(other, Track):
            return Track([o + c for c, o in zip(self.channels, other.channels)])
        return Track([other + c for c in self.channels])

    def __iadd__(self, other):
        if isinstance(other, Track):
            for c, o in zip(self.channels, other.channels):
                c += o
        else:
            for c in self.channels:
                c += other
        return self

