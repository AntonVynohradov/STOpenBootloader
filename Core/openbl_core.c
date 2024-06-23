/**
  ******************************************************************************
  * @file    openbl_core.c
  * @author  MCD Application Team
  * @brief   Open Bootloader core file
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2019-2021 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include <stdbool.h>

#include "openbl_core.h"
#include "openbl_mem.h"
#include "interfaces_conf.h"

#include "usart_interface.h"
#include "i2c_interface.h"
#include "fdcan_interface.h"
#include "spi_interface.h"
#include "usb_interface.h"
#include "iwdg_interface.h"

#include "openbl_usart_cmd.h"
#include "openbl_i2c_cmd.h"
#include "openbl_fdcan_cmd.h"
#include "openbl_spi_cmd.h"

/* External variables --------------------------------------------------------*/
extern OPENBL_MemoryTypeDef FLASH_Descriptor;
extern OPENBL_MemoryTypeDef RAM_Descriptor;
extern OPENBL_MemoryTypeDef OB1_Descriptor;
#ifdef OB2_START_ADDRESS
extern OPENBL_MemoryTypeDef OB2_Descriptor;
#endif
extern OPENBL_MemoryTypeDef OTP_Descriptor;
extern OPENBL_MemoryTypeDef ICP1_Descriptor;
#ifdef ICP2_START_ADDRESS
extern OPENBL_MemoryTypeDef ICP2_Descriptor;
#endif

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
static uint32_t NumberOfInterfaces = 0U;
static OPENBL_HandleTypeDef a_InterfacesTable[INTERFACES_SUPPORTED];
static OPENBL_HandleTypeDef *p_Interface;
#ifdef USARTx
static OPENBL_HandleTypeDef USART_Handle;
#endif
#ifdef I2Cx
static OPENBL_HandleTypeDef I2C_Handle;
#endif
#ifdef FDCANx
static OPENBL_HandleTypeDef FDCAN_Handle;
#endif
#ifdef SPIx
static OPENBL_HandleTypeDef SPI_Handle;
#endif
#ifdef USB_OTG_FS
static OPENBL_HandleTypeDef USB_Handle;
#endif
static OPENBL_HandleTypeDef IWDG_Handle;

#ifdef USARTx
static OPENBL_OpsTypeDef USART_Ops =
{
  OPENBL_USART_Configuration,
  OPENBL_USART_DeInit,
  OPENBL_USART_ProtocolDetection,
  OPENBL_USART_GetCommandOpcode,
  OPENBL_USART_SendByte
};
#endif 

#ifdef I2Cx
static OPENBL_OpsTypeDef I2C_Ops =
{
  OPENBL_I2C_Configuration,
  OPENBL_I2C_DeInit,
  OPENBL_I2C_ProtocolDetection,
  OPENBL_I2C_GetCommandOpcode,
  OPENBL_I2C_SendAcknowledgeByte
};
#endif

#ifdef FDCANx
static OPENBL_OpsTypeDef FDCAN_Ops =
{
  OPENBL_FDCAN_Configuration,
  OPENBL_FDCAN_DeInit,
  OPENBL_FDCAN_ProtocolDetection,
  OPENBL_FDCAN_GetCommandOpcode,
  NULL
};
#endif

#ifdef SPIx
static OPENBL_OpsTypeDef SPI_Ops =
{
  OPENBL_SPI_Configuration,
  OPENBL_SPI_DeInit,
  OPENBL_SPI_ProtocolDetection,
  OPENBL_SPI_GetCommandOpcode,
  OPENBL_SPI_SendAcknowledgeByte
};
#endif
#ifdef USB_OTG_FS
static OPENBL_OpsTypeDef USB_Ops =
{
  OPENBL_USB_Configuration,
  OPENBL_USB_DeInit,
  OPENBL_USB_ProtocolDetection,
  NULL,
  NULL
};
#endif
#ifdef IWDG
static OPENBL_OpsTypeDef IWDG_Ops =
{
  OPENBL_IWDG_Configuration,
  NULL,
  NULL,
  NULL,
  NULL
};
#else
#error "Independent watchdog (IWDG) is required for this library"
#endif
/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/**
  * @brief  This function is used to detect if there is any activity on a given interface.
  * @retval None.
  */
static uint32_t OPENBL_InterfaceDetection(void)
{
  uint32_t counter;
  uint8_t detected = 0U;

  for (counter = 0U; counter < NumberOfInterfaces; counter++)
  {
    if (a_InterfacesTable[counter].p_Ops->Detection != NULL)
    {
      detected = a_InterfacesTable[counter].p_Ops->Detection();

      if (detected == 1U)
      {
        p_Interface = &(a_InterfacesTable[counter]);
        break;
      }
    }
  }

  return detected;
}

/**
  * @brief  This function is used to get the command opcode from the given interface and execute the right command.
  * @retval None.
  */
static void OPENBL_CommandProcess(void)
{
  uint8_t command_opcode;

  /* Get the user command opcode */
  if (p_Interface->p_Ops->GetCommandOpcode != NULL)
  {
    command_opcode = p_Interface->p_Ops->GetCommandOpcode();

    switch (command_opcode)
    {
      case CMD_GET_COMMAND:
        if (p_Interface->p_Cmd->GetCommand != NULL)
        {
          p_Interface->p_Cmd->GetCommand();
        }
        else
        {
          if (p_Interface->p_Ops->SendByte != NULL)
          {
            p_Interface->p_Ops->SendByte(NACK_BYTE);
          }
        }
        break;

      case CMD_GET_VERSION:
        if (p_Interface->p_Cmd->GetVersion != NULL)
        {
          p_Interface->p_Cmd->GetVersion();
        }
        else
        {
          if (p_Interface->p_Ops->SendByte != NULL)
          {
            p_Interface->p_Ops->SendByte(NACK_BYTE);
          }
        }
        break;

      case CMD_GET_ID:
        if (p_Interface->p_Cmd->GetID != NULL)
        {
          p_Interface->p_Cmd->GetID();
        }
        else
        {
          if (p_Interface->p_Ops->SendByte != NULL)
          {
            p_Interface->p_Ops->SendByte(NACK_BYTE);
          }
        }
        break;

      case CMD_READ_MEMORY:
        if (p_Interface->p_Cmd->ReadMemory != NULL)
        {
          p_Interface->p_Cmd->ReadMemory();
        }
        else
        {
          if (p_Interface->p_Ops->SendByte != NULL)
          {
            p_Interface->p_Ops->SendByte(NACK_BYTE);
          }
        }
        break;

      case CMD_WRITE_MEMORY:
        if (p_Interface->p_Cmd->WriteMemory != NULL)
        {
          p_Interface->p_Cmd->WriteMemory();
        }
        else
        {
          if (p_Interface->p_Ops->SendByte != NULL)
          {
            p_Interface->p_Ops->SendByte(NACK_BYTE);
          }
        }
        break;

      case CMD_GO:
        if (p_Interface->p_Cmd->Go != NULL)
        {
          p_Interface->p_Cmd->Go();
        }
        else
        {
          if (p_Interface->p_Ops->SendByte != NULL)
          {
            p_Interface->p_Ops->SendByte(NACK_BYTE);
          }
        }
        break;

      case CMD_READ_PROTECT:
        if (p_Interface->p_Cmd->ReadoutProtect != NULL)
        {
          p_Interface->p_Cmd->ReadoutProtect();
        }
        else
        {
          if (p_Interface->p_Ops->SendByte != NULL)
          {
            p_Interface->p_Ops->SendByte(NACK_BYTE);
          }
        }
        break;

      case CMD_READ_UNPROTECT:
        if (p_Interface->p_Cmd->ReadoutUnprotect != NULL)
        {
          p_Interface->p_Cmd->ReadoutUnprotect();
        }
        else
        {
          if (p_Interface->p_Ops->SendByte != NULL)
          {
            p_Interface->p_Ops->SendByte(NACK_BYTE);
          }
        }
        break;

      case CMD_EXT_ERASE_MEMORY:
        if (p_Interface->p_Cmd->EraseMemory != NULL)
        {
          p_Interface->p_Cmd->EraseMemory();
        }
        else
        {
          if (p_Interface->p_Ops->SendByte != NULL)
          {
            p_Interface->p_Ops->SendByte(NACK_BYTE);
          }
        }
        break;

      case CMD_LEG_ERASE_MEMORY:
        if (p_Interface->p_Cmd->EraseMemory != NULL)
        {
          p_Interface->p_Cmd->EraseMemory();
        }
        else
        {
          if (p_Interface->p_Ops->SendByte != NULL)
          {
            p_Interface->p_Ops->SendByte(NACK_BYTE);
          }
        }
        break;

      case CMD_WRITE_PROTECT:
        if (p_Interface->p_Cmd->WriteProtect != NULL)
        {
          p_Interface->p_Cmd->WriteProtect();
        }
        else
        {
          if (p_Interface->p_Ops->SendByte != NULL)
          {
            p_Interface->p_Ops->SendByte(NACK_BYTE);
          }
        }
        break;

      case CMD_WRITE_UNPROTECT:
        if (p_Interface->p_Cmd->WriteUnprotect != NULL)
        {
          p_Interface->p_Cmd->WriteUnprotect();
        }
        else
        {
          if (p_Interface->p_Ops->SendByte != NULL)
          {
            p_Interface->p_Ops->SendByte(NACK_BYTE);
          }
        }
        break;

      case CMD_NS_WRITE_MEMORY:
        if (p_Interface->p_Cmd->NsWriteMemory != NULL)
        {
          p_Interface->p_Cmd->NsWriteMemory();
        }
        else
        {
          if (p_Interface->p_Ops->SendByte != NULL)
          {
            p_Interface->p_Ops->SendByte(NACK_BYTE);
          }
        }
        break;

      case CMD_NS_ERASE_MEMORY:
        if (p_Interface->p_Cmd->NsEraseMemory != NULL)
        {
          p_Interface->p_Cmd->NsEraseMemory();
        }
        else
        {
          if (p_Interface->p_Ops->SendByte != NULL)
          {
            p_Interface->p_Ops->SendByte(NACK_BYTE);
          }
        }
        break;

      case CMD_NS_WRITE_PROTECT:
        if (p_Interface->p_Cmd->NsWriteProtect != NULL)
        {
          p_Interface->p_Cmd->NsWriteProtect();
        }
        else
        {
          if (p_Interface->p_Ops->SendByte != NULL)
          {
            p_Interface->p_Ops->SendByte(NACK_BYTE);
          }
        }
        break;

      case CMD_NS_WRITE_UNPROTECT:
        if (p_Interface->p_Cmd->NsWriteUnprotect != NULL)
        {
          p_Interface->p_Cmd->NsWriteUnprotect();
        }
        else
        {
          if (p_Interface->p_Ops->SendByte != NULL)
          {
            p_Interface->p_Ops->SendByte(NACK_BYTE);
          }
        }
        break;

      case CMD_NS_READ_PROTECT:
        if (p_Interface->p_Cmd->NsReadoutProtect != NULL)
        {
          p_Interface->p_Cmd->NsReadoutProtect();
        }
        else
        {
          if (p_Interface->p_Ops->SendByte != NULL)
          {
            p_Interface->p_Ops->SendByte(NACK_BYTE);
          }
        }
        break;

      case CMD_NS_READ_UNPROTECT:
        if (p_Interface->p_Cmd->NsReadoutUnprotect != NULL)
        {
          p_Interface->p_Cmd->NsReadoutUnprotect();
        }
        else
        {
          if (p_Interface->p_Ops->SendByte != NULL)
          {
            p_Interface->p_Ops->SendByte(NACK_BYTE);
          }
        }
        break;

      case CMD_SPEED:
        if (p_Interface->p_Cmd->Speed != NULL)
        {
          p_Interface->p_Cmd->Speed();
        }
        else
        {
          if (p_Interface->p_Ops->SendByte != NULL)
          {
            p_Interface->p_Ops->SendByte(NACK_BYTE);
          }
        }
        break;

      case CMD_SPECIAL_COMMAND:
        if (p_Interface->p_Cmd->SpecialCommand != NULL)
        {
          p_Interface->p_Cmd->SpecialCommand();
        }
        else
        {
          if (p_Interface->p_Ops->SendByte != NULL)
          {
            p_Interface->p_Ops->SendByte(NACK_BYTE);
          }
        }
        break;

      case CMD_EXTENDED_SPECIAL_COMMAND:
        if (p_Interface->p_Cmd->ExtendedSpecialCommand != NULL)
        {
          p_Interface->p_Cmd->ExtendedSpecialCommand();
        }
        else
        {
          if (p_Interface->p_Ops->SendByte != NULL)
          {
            p_Interface->p_Ops->SendByte(NACK_BYTE);
          }
        }
        break;

      /* Unknown command opcode */
      default:
        if (p_Interface->p_Ops->SendByte != NULL)
        {
          p_Interface->p_Ops->SendByte(NACK_BYTE);
        }
        break;
    }
  }
}

/* Exported functions --------------------------------------------------------*/

/**
  * @brief  This function is used to initialize the registered interfaces in the Open Bootloader MW.
  * @retval None.
  */
void OPENBL_Init(void)
{ 
  #ifdef USARTx
  /* Register USART interfaces */
  USART_Handle.p_Ops = &USART_Ops;
  USART_Handle.p_Cmd = OPENBL_USART_GetCommandsList();

  OPENBL_RegisterInterface(&USART_Handle);
  #endif

  #ifdef I2Cx
  /* Register I2C interfaces */
  I2C_Handle.p_Ops = &I2C_Ops;
  I2C_Handle.p_Cmd = OPENBL_I2C_GetCommandsList();

  OPENBL_RegisterInterface(&I2C_Handle);
  #endif

  #ifdef FDCANx
  /* Register FDCAN interfaces */
  FDCAN_Handle.p_Ops = &FDCAN_Ops;
  FDCAN_Handle.p_Cmd = OPENBL_FDCAN_GetCommandsList();

  OPENBL_RegisterInterface(&FDCAN_Handle);
  #endif

  #ifdef SPIx
  /* Register SPI interfaces */
  SPI_Handle.p_Ops = &SPI_Ops;
  SPI_Handle.p_Cmd = OPENBL_SPI_GetCommandsList();

  OPENBL_RegisterInterface(&SPI_Handle);
  #endif

  #ifdef USB_OTG_FS
  #error "USB OTG FS Not implemented."
  /* Register SPI interfaces */
  USB_Handle.p_Ops = &USB_Ops;
  USB_Handle.p_Cmd = NULL;
  OPENBL_RegisterInterface(&USB_Handle);
  #endif

  #ifdef IWDG
  /* Register IWDG interfaces */
  IWDG_Handle.p_Ops = &IWDG_Ops;
  IWDG_Handle.p_Cmd = NULL;

  OPENBL_RegisterInterface(&IWDG_Handle);
  #endif

  for (uint32_t counter = 0U; counter < NumberOfInterfaces; counter++)
  {
    if (a_InterfacesTable[counter].p_Ops->Init != NULL)
    {
      a_InterfacesTable[counter].p_Ops->Init();
    }
  }

  /* Initialise memories */
  OPENBL_MEM_RegisterMemory(&FLASH_Descriptor);
  OPENBL_MEM_RegisterMemory(&RAM_Descriptor);
  OPENBL_MEM_RegisterMemory(&OB1_Descriptor);
  #ifdef OB2_START_ADDRESS
  OPENBL_MEM_RegisterMemory(&OB2_Descriptor);
  #endif
  OPENBL_MEM_RegisterMemory(&OTP_Descriptor);
  OPENBL_MEM_RegisterMemory(&ICP1_Descriptor);
  #ifdef ICP2_START_ADDRESS
  OPENBL_MEM_RegisterMemory(&ICP2_Descriptor);
  #endif
}

/**
  * @brief  This function is used to de-initialize the Open Bootloader MW.
  * @retval None.
  */
void OPENBL_DeInit(void)
{
  HAL_RCC_DeInit();
}

/**
  * @brief  This function is used to de-initialize the registered interfaces in the Open Bootloader MW.
  * @retval None.
  */
void OPENBL_InterfacesDeInit(void)
{
  uint32_t counter;

  for (counter = 0U; counter < NumberOfInterfaces; counter++)
  {
    if (a_InterfacesTable[counter].p_Ops->DeInit != NULL)
    {
      a_InterfacesTable[counter].p_Ops->DeInit();
    }
  }
}

/**
  * @brief  This function is used to register a given interface in the Open Bootloader MW.
  * @retval None.
  */
ErrorStatus OPENBL_RegisterInterface(OPENBL_HandleTypeDef *Interface)
{
  ErrorStatus status = SUCCESS;

  if (NumberOfInterfaces < INTERFACES_SUPPORTED)
  {
    a_InterfacesTable[NumberOfInterfaces].p_Ops = Interface->p_Ops;
    a_InterfacesTable[NumberOfInterfaces].p_Cmd = Interface->p_Cmd;

    NumberOfInterfaces++;
  }
  else
  {
    status = ERROR;
  }

  return status;
}

/**
  * @brief  This function is used to select which protocol will be used when communicating with the host.
  * @param  None.
  * @retval None.
  */
void OPENBL_Handler(void)
{
  static uint32_t interface_detected = 0U;

  if (interface_detected == 0U)
  {
    interface_detected = OPENBL_InterfaceDetection();
  }

  if (interface_detected == 1U)
  {
    OPENBL_CommandProcess();
  }
}