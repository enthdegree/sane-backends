/*
        HP ScanJet 5s kernel module interface.
        Wrote by: Max Vorobiev.
*/

#ifndef __HPSJ5S_KERNEL_MODULE_INTERFACE__
#define __HPSJ5S_KERNEL_MODULE_INTERFACE__

/*
        Scanner registers (not all - some of them i cann't identify...)
*/

/*Here we place function code to perform*/
#define REGISTER_FUNCTION_CODE          0x70
/*Here we place parameter for function*/
#define REGISTER_FUNCTION_PARAMETER     0x60

/*
        Here we define addresses, used for reading values:
*/

/*this address used to get results (image strips, function result codes, etc.)*/
#define ADDRESS_RESULT                  0x20

/*
        Scanner functions (not all - some of them i cann't identify...)
*/

/*Change scanner hardware state: use FLAGS_HW_xxxx for parameters*/
#define FUNCTION_SETUP_HARDWARE         0xA0

/*Hardware control flags:*/
#define FLAGS_HW_MOTOR_READY		0x1	/*Set this flag and non-zero speed to start rotation */
#define FLAGS_HW_LAMP_ON		0x2	/*Set this flag to turn on lamp */
#define FLAGS_HW_INDICATOR_OFF		0x4	/*Set[Clean] this flag to turn off[on] green light indicator */

/******************Code for kernel module operations******************************/

typedef enum
{ OP_WRITE_ADDRESS,		/*Output byte in address mode */
  OP_WRITE_DATA_BYTE,		/*Output byte in data mode */
  OP_WRITE_SCANER_REGISTER,	/*Output one byte in address mode and one byte in data mode */
  OP_CALL_FUNCTION_WITH_PARAMETER	/*Output first value via 0x70 address, second value via 0x60 */
}
Op_Code;

typedef struct
{
  unsigned char Address;
}
strWriteAddress;

typedef struct
{
  unsigned char Data;
}
strWriteDataByte;

typedef struct
{
  unsigned char Address;
  unsigned char Data;
}
strWriteScannerRegister;

typedef struct
{
  unsigned char Function;
  unsigned char Parameter;
}
strFunctionWithParameter;

typedef struct
{
  Op_Code Code;			/*Operation to perform */
  union
  {
    strWriteAddress WrAddr;
    strWriteDataByte WrData;
    strWriteScannerRegister WrScannerReg;
    strFunctionWithParameter FuncParam;
  }
  Parameters;
}
strOperationBlock;

#endif
