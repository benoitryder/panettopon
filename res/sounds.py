#!/usr/bin/env python
import os
import sys
import time
from soundwaves import *

def stereo_sequence(sounds, damp):
    ret = []
    for i, sound in enumerate(sounds):
        d = i * damp
        if i % 2 == 0:
            d1, d2 = -d, d
        else:
            d1, d2 = d, -d
        ret.append(Track((sound.amplify(1+d1), sound.amplify(1+d2))))
    return ret


def pop_seq1_1(dur=0.04, amp=0.5):
    pieces = [(0, 0), (0.1, 1), (0.9, 0.3), (1, 0)]
    freqs = [360 + i*40 for i in range(4)]
    seq = [WaveTriangle(f, amp, dur).enveloppe_linear_pieces(pieces) for f in freqs]
    return stereo_sequence(seq, 0.05)

def pop_seq1_2(dur=0.04, amp=0.5):
    pieces0 = [(0, 0), (0.1, 1), (0.4, 0.6), (0.9, 0.3), (1, 0)]
    pieces1 = [(0, 0), (0.1, 1), (0.4, 0.6), (0.9, 0.3), (1, 0)]
    freqs0 = [440 + i*30 for i in range(6)]
    freqs1 = [360 + i*30 for i in range(6)]
    dur0 = 0.8*dur
    dur1 = dur
    seq = []
    for f0, f1 in zip(freqs0, freqs1):
        w0 = WaveTriangle(f0, amp, dur0).enveloppe_linear_pieces(pieces0)
        w1 = WaveTriangle(f1, amp, dur1).enveloppe_linear_pieces(pieces1)
        seq.append(w0 + w1)
    return stereo_sequence(seq, 0.04)

def pop_seq1(dur=0.04, amp=0.5):
    return pop_seq1_1() + pop_seq1_2()

def pop_seq2(dur=0.04, amp=0.5):
    pieces0 = [(0, 0), (0.1, 1), (0.4, 0.6), (1, 0)]
    pieces1 = [(0, 0), (0.1, 1), (0.4, 0.6), (1, 0)]
    freqs0 = [400 + i*25 for i in range(10)]
    freqs1 = [320 + i*25 for i in range(10)]
    dur0 = 0.7*dur
    dur1 = 0.8*dur
    amp0 = 0.8*amp
    amp1 = amp
    seq = []
    for f0, f1 in zip(freqs0, freqs1):
        w0 = WaveTriangle(f0, amp0, dur0).enveloppe_linear_pieces(pieces0)
        w1 = WaveTriangle(f1, amp1, dur1).enveloppe_linear_pieces(pieces1)
        seq.append(w1 + w0 + w1)
    return stereo_sequence(seq, 0.03)

def pop_seq3(dur=0.04, amp=0.5):
    pieces0 = [(0, 1), (0.35, 0.5), (0.65, 0.3), (1, 0)]
    freqs0 = [280 + i*30 for i in range(10)]
    nwaves = 4
    dur0 = 0.8*dur
    amp0 = amp
    noise = NoiseTriangle(1000, amp0/6, dur0)
    seq = []
    for f0 in freqs0:
        w = (WaveTriangle(f0, amp0, dur0) | noise).enveloppe_linear_pieces(pieces0)
        seq.append(w * nwaves)
    return stereo_sequence(seq, 0.03)

def pop_seq4(dur=0.04, amp=0.5):
    pieces0 = [(0, 1), (0.35, 0.5), (0.65, 0.3), (1, 0)]
    freqs0 = [280 + i*30 for i in range(10)]
    nwaves = 4
    dur0 = 0.8*dur
    amp0 = amp
    noise = NoiseTriangle(5000, amp0/6, dur0)
    seq = []
    for f0 in freqs0:
        w = (WaveTriangle(f0, amp0, dur0) | noise).enveloppe_linear_pieces(pieces0)
        seq.append(w * nwaves)
    return stereo_sequence(seq, 0.03)


def pop_all(dur=0.04, amp=0.5):
    return [globals()['pop_seq%d' % (i+1)](dur, amp) for i in range(4)]


def main():
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument('-o', '--output', metavar='DIR', default='',
            help="output directory")
    parser.add_argument('-1', '--single', action='store_true',
            help="generate a single WAV file with all sounds")
    args = parser.parse_args()

    if not os.path.isdir(args.output):
        os.makedirs(args.output)

    pop_seqs = pop_all()
    if args.single:
        merge_sequence([merge_sequence(seq, dt=0.1) for seq in pop_seqs], dt=0.2).write(os.path.join(args.output, 'pop-all.wav'))
    else:
        for i, seq in enumerate(pop_seqs):
            for j, track in enumerate(seq):
                track.write(os.path.join(args.output, 'pop-%d-%d.wav' % (i, j)))

if __name__ == '__main__':
    main()

