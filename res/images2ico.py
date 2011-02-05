#!/usr/bin/env python

import struct
import math
from StringIO import StringIO
from PIL import Image

def im2bmp(im):
  """Convert an image into a 32-bit BMP.
  Return the BMP data.
  Assume that sizes are powers of 2.
  """
  w, h = im.size
  data = struct.pack('<IiiHHII4I', 40, w, 2*h, 1, 32, 0, 0, 0,0,0,0)
  for pix in im.transpose(Image.FLIP_TOP_BOTTOM).getdata():
    data += struct.pack('<BBBB', pix[2], pix[1], pix[0], pix[3])
  # transparency bit mask
  rown = int( math.ceil(w/32.) * 4 )
  data += '\0' * (h*rown)
  return data


def images2ico(fout, fpngs):
  """Create an ICO from images filenames."""

  imgs = [ Image.open(f).convert('RGBA') for f in fpngs ]

  if isinstance(fout, basestring):
    fout = open(fout, 'wb')

  # header: 0000 0001 image-count
  HEADER_FMT = '<HHH'
  fout.write(struct.pack(HEADER_FMT, 0, 1, len(imgs)))

  # image info
  IMAGE_FMT = '<BBBBHHII'
  datas = []  # pixel data
  offset = struct.calcsize(HEADER_FMT)  # current data offset
  offset += struct.calcsize(IMAGE_FMT) * len(imgs)
  for im in imgs:
    w, h = im.size
    if w > 256 or h > 256:
      raise ValueError("image is too big")
    data = im2bmp(im)
    fout.write(struct.pack(IMAGE_FMT,
      w if w!=256 else 0, h if h!=256 else 0,
      0, 0, 1, 32, len(data), offset)
      )
    offset += len(data)
    datas.append(data)

  # image data
  for d in datas:
    fout.write(d)


def main():
  from optparse import OptionParser
  parser = OptionParser(
      description="Assemble images into a .ICO file.",
      usage="%prog OUTPUT FILES",
      )
  (opts, args) = parser.parse_args()

  if len(args) == 0:
    parser.error("no output file")
  if len(args) < 2:
    parser.error("no input file")
  images2ico(args[0], args[1:])


if __name__ == '__main__':
  main()

