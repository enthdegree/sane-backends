/* $Id$
   SnapScan backend scan data sources */

#ifndef SNAPSCAN_SOURCES_H
#define SNAPSCAN_SOURCES_H

typedef struct source Source;

typedef SANE_Int (*SourceRemaining) (Source *ps);
typedef SANE_Int (*SourceBytesPerLine) (Source *ps);
typedef SANE_Int (*SourcePixelsPerLine) (Source *ps);
typedef SANE_Status (*SourceGet) (Source *ps, SANE_Byte *pbuf, SANE_Int *plen);
typedef SANE_Status (*SourceDone) (Source *ps);

#define SOURCE_GUTS \
  SnapScan_Scanner *pss;\
  SourceRemaining remaining;\
  SourceBytesPerLine bytesPerLine;\
  SourcePixelsPerLine pixelsPerLine;\
  SourceGet get;\
  SourceDone done

struct source {
  SOURCE_GUTS;
};

static
SANE_Status Source_init (Source *pself, SnapScan_Scanner *pss,
			 SourceRemaining remaining,
			 SourceBytesPerLine bytesPerLine,
			 SourcePixelsPerLine pixelsPerLine,
			 SourceGet get,
			 SourceDone done);


/* base sources */


#endif
