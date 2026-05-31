#ifndef AD5940_PORT_H
#define AD5940_PORT_H

#include <Arduino.h>

#ifdef __cplusplus
extern "C" {
#endif

void AD5940_CsClr(void);
void AD5940_CsSet(void);
void AD5940_RstClr(void);
void AD5940_RstSet(void);
void AD5940_Delay10us(uint32_t time);
void AD5940_ReadWriteNBytes(unsigned char *pSendBuffer,
                            unsigned char *pRecvBuff,
                            unsigned long length);

// 按官方库要求实现
uint32_t AD5940_GetMCUIntFlag(void);
uint32_t AD5940_ClrMCUIntFlag(void);

void AD5940_Port_Init(void);
void Board_SelectWE1(void);

#ifdef __cplusplus
}
#endif

#endif