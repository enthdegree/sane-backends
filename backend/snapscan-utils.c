/* $Id$
   SnapScan backend utility functions */


static inline
SnapScan_Mode
actual_mode (SnapScan_Scanner *pss)
{
  if (pss->preview == SANE_TRUE)
    return pss->preview_mode;
  else
    return pss->mode;
}


static inline
int
is_colour_mode (SnapScan_Mode m)
{
  return (m == MD_COLOUR) || (m == MD_BILEVELCOLOUR);
}

