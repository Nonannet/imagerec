# imagerec

A simple and lightweight image recognition tool for small Linux based
embedded systems like the MT7688AN SoC.

Images can be captured from v4l2 camera devices and recognizes lines
and circles using hough transformation.

The tool can be controlled by command line parameters or text over TCP/IP.

The TCP/IP interface is dessigned to be used by scripts (e.g. micro python)
localy on the embeded device or remotly from a PC for easy development.
Netcat can be very usefull for testing.

Is uses only fixed point operations and is optimized for low memory devices.

```text
Usage: imagerec [-options]
options:
         -h                 show help
         -c commands        ASCII command string
         -d port            TCP/IP-port to listen on
         -p device          capture device name
         -r whidth*hight    image resolution
         -n pixels          max number of pixels for l command
         -f file            file path for w command (# for index)
         -d                 return result data only

examples: imagerec -d /dev/video0 -r 640*480 -p 5044
          imagerec -d /dev/video0 -r 640*480 -c cgnexCngml -s
          imagerec -d /dev/video0 -r 640*480 -c cgnexCnw -f result.tif
          imagerec -d /dev/video0 -r 640*480 -c cgnwexsonwrCnw -f result#.tif

single byte ASCII commands:
          c     capture image
          n     normalize image
          g     apply gausian blur
          e     edge detection with sobel filter, must be
                folowd by 'x', 'o' or a hough transformation
          x     remove non edge pixel, must be folowd by
                'o' or a hough transformation
          o     convert directional slope to absolute slope
          C     circle hough transformation
          L     line hough transformation
          H     line hough transformation (horizontal only)
          V     line hough transformation (vertical only)
          M     miniscus hough transformation
          b     binarize
          m     remove non-local-maxima pixels
          l     list brightes pixels (from max. 32 non black pixels)
          p     list brightes pixel clusters (3x3)
          q     close connection
          s     store a copy of the current buffer
          r     recall a copy of the stored buffer
          w     write buffer to disk (TIF format)
          z     set index for output file name to zero
          d     show result data only
          i     show info and result data (default)

example: echo \"cngexCngmlq\" | nc localhost 5044
```
