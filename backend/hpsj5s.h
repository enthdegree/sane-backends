#ifndef __HPSJ5S_MIDDLE_LEVEL_API_HEADER__
#define __HPSJ5S_MIDDLE_LEVEL_API_HEADER__


#include "hpsj5s_int.h"

/*
        Types:
*/
/*Color modes we support: 1-bit Drawing, 2-bit Halftone, 8-bit Gray Scale, 24-bt True Color*/
typedef enum
{ Drawing, Halftone, GrayScale, TrueColor }
enumColorDepth;

/*Middle-level API:*/

static int DetectScanner (void);

static void StandByScanner (void);

static void SwitchHardwareState (SANE_Byte mask, SANE_Byte invert_mask);

static int CheckPaperPresent (void);

static int ReleasePaper (void);

static int PaperFeed (SANE_Word wLinesToFeed);

static void TransferScanParameters (enumColorDepth enColor,
				    SANE_Word wResolution,
				    SANE_Word wCorrectedLength);

static void TurnOnPaperPulling (enumColorDepth enColor,
				SANE_Word wResolution);

static void TurnOffPaperPulling (void);

static SANE_Byte GetCalibration (void);

static void CalibrateScanElements (void);

/*Internal-use functions:*/

static int OutputCheck (void);
static int InputCheck (void);
static int CallCheck (void);
static void LoadingPaletteToScanner (void);

/*Low level warappers:*/

static void WriteAddress (SANE_Byte Address);

static void WriteData (SANE_Byte Data);

static void WriteScannerRegister (SANE_Byte Address, SANE_Byte Data);

static void CallFunctionWithParameter (SANE_Byte Function,
				       SANE_Byte Parameter);

static SANE_Byte CallFunctionWithRetVal (SANE_Byte Function);

static SANE_Byte ReadDataByte (void);

static void ReadDataBlock (SANE_Byte * Buffer, int lenght);

#endif
