/**
  ******************************************************************************
  * @file    flash_interface.c
  * @author  MCD Application Team
  * @brief   Contains FLASH access functions
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2021 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "openbl_core.h"
#include "platform.h"
#include "openbl_mem.h"
#include "common_interface.h"
#include "flash_interface.h"
#include "i2c_interface.h"
#include "optionbytes_interface.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
uint32_t Flash_BusyState = FLASH_BUSY_STATE_DISABLED;
FLASH_ProcessTypeDef FlashProcess = {.Lock = HAL_UNLOCKED, \
                                     .ErrorCode = HAL_FLASH_ERROR_NONE, \
                                     .ProcedureOnGoing = 0U, \
                                     .Address = 0U, \
                                     .Bank = FLASH_BANK_1, \
                                     .Page = 0U, \
                                     .NbPagesToErase = 0U
                                    };

/* Private function prototypes -----------------------------------------------*/
#ifdef FLASH_TYPEPROGRAM_QUADWORD
static void OPENBL_FLASH_ProgramQuadWord(uint32_t Address, uint32_t Data);
#else
static void OPENBL_FLASH_ProgramDoubleWord(uint32_t Address, uint64_t Data);
#endif
static ErrorStatus OPENBL_FLASH_EnableWriteProtection(uint8_t *ListOfPages, uint32_t Length);
static ErrorStatus OPENBL_FLASH_DisableWriteProtection(void);
#if defined (__ICCARM__)
__ramfunc static HAL_StatusTypeDef OPENBL_FLASH_SendBusyState(uint32_t Timeout);
__ramfunc static HAL_StatusTypeDef OPENBL_FLASH_WaitForLastOperation(uint32_t Timeout);
__ramfunc static HAL_StatusTypeDef OPENBL_FLASH_ExtendedErase(FLASH_EraseInitTypeDef *pEraseInit, uint32_t *PageError);
#else
__attribute__((section(".ramfunc"))) static HAL_StatusTypeDef OPENBL_FLASH_SendBusyState(uint32_t Timeout);
__attribute__((section(".ramfunc"))) static HAL_StatusTypeDef OPENBL_FLASH_WaitForLastOperation(uint32_t Timeout);
__attribute__((section(".ramfunc"))) static HAL_StatusTypeDef OPENBL_FLASH_ExtendedErase(
  FLASH_EraseInitTypeDef *pEraseInit, uint32_t *PageError);
#endif /* (__ICCARM__) */

/* Exported variables --------------------------------------------------------*/
OPENBL_MemoryTypeDef FLASH_Descriptor =
{
  FLASH_START_ADDRESS,
  FLASH_END_ADDRESS,
  FLASH_BL_SIZE,
  FLASH_AREA,
  OPENBL_FLASH_Read,
  OPENBL_FLASH_Write,
  OPENBL_FLASH_SetReadOutProtectionLevel,
  OPENBL_FLASH_SetWriteProtection,
  OPENBL_FLASH_JumpToAddress,
  NULL,
  OPENBL_FLASH_Erase
};

/* Exported functions --------------------------------------------------------*/

/**
  * @brief  Unlock the FLASH control register access.
  * @retval None.
  */
void OPENBL_FLASH_Unlock(void)
{
  HAL_FLASH_Unlock();
}

/**
  * @brief  Lock the FLASH control register access.
  * @retval None.
  */
void OPENBL_FLASH_Lock(void)
{
  HAL_FLASH_Lock();
}

/**
  * @brief  Unlock the FLASH Option Bytes Registers access.
  * @retval None.
  */
void OPENBL_FLASH_OB_Unlock(void)
{
  HAL_FLASH_Unlock();

  HAL_FLASH_OB_Unlock();
}

/**
  * @brief  This function is used to read data from a given address.
  * @param  Address The address to be read.
  * @retval Returns the read value.
  */
uint8_t OPENBL_FLASH_Read(uint32_t Address)
{
  return (*(uint8_t *)(Address));
}

/**
  * @brief  This function is used to write data in FLASH memory.
  * @param  Address The address where that data will be written.
  * @param  Data The data to be written.
  * @param  DataLength The length of the data to be written.
  * @retval None.
  */
void OPENBL_FLASH_Write(uint32_t Address, uint8_t *Data, uint32_t DataLength)
{
  uint32_t index = 0U;
  uint32_t length = DataLength;

  #ifdef FLASH_TYPEPROGRAM_QUADWORD

  uint32_t remainder = 0U;
  uint8_t remainder_data[16] = {0x0};
 

  /* Check the remaining of quad-word */
  remainder = length & 0xFU;

  if (remainder)
  {
    length = (length & 0xFFFFFFF0U);

    /* copy the remaining bytes */
    for (index = 0U; index < remainder; index++)
    {
      remainder_data[index] = *(Data + length + index);
    }

    /* fill the upper bytes with 0xFF */
    for (index = remainder; index < 16U; index++)
    {
      remainder_data[index] = 0xFF;
    }
  }
  #else

  if (length & 7U)
  {
    length = (length & 0xFFFFFFF8U) + 8U;
  }

  #endif

  /* Unlock the flash memory for write operation */
  OPENBL_FLASH_Unlock();

  #ifdef FLASH_TYPEPROGRAM_QUADWORD

  for (index = 0U; index < length; (index += 16U))
  {
    OPENBL_FLASH_ProgramQuadWord((Address + index), (uint32_t)((Data + index)));
  }

  if (remainder)
  {
    OPENBL_FLASH_ProgramQuadWord((Address + length), (uint32_t)((remainder_data)));
  }

  #else

  for (index = 0U; index < length; (index += 8U))
  {
    OPENBL_FLASH_ProgramDoubleWord((Address + index), (uint64_t)(*((uint64_t *)((uint32_t)Data + index))));
  }

  #endif
  /* Lock the Flash to disable the flash control register access */
  OPENBL_FLASH_Lock();
}

/**
  * @brief  This function is used to jump to a given address.
  * @param  Address The address where the function will jump.
  * @retval None.
  */
void OPENBL_FLASH_JumpToAddress(uint32_t Address)
{
  Function_Pointer jump_to_address;

  /* De-initialize all HW resources used by the Open Bootloader to their reset values */
  OPENBL_DeInit();

  /* Enable IRQ */
  Common_EnableIrq();

  jump_to_address = (Function_Pointer)(*(__IO uint32_t *)(Address + 4U));

  /* Initialize user application's stack pointer */
  Common_SetMsp(*(__IO uint32_t *) Address);

  jump_to_address();
}

/**
  * @brief  Return the FLASH Read Protection level.
  * @retval The return value can be one of the following values:
  *         @arg OB_RDP_LEVEL_0: No protection
  *         @arg OB_RDP_LEVEL_1: Read protection of the memory
  *         @arg OB_RDP_LEVEL_2: Full chip protection
  */
uint32_t OPENBL_FLASH_GetReadOutProtectionLevel(void)
{
  FLASH_OBProgramInitTypeDef flash_ob;

  /* Get the Option bytes configuration */
  HAL_FLASHEx_OBGetConfig(&flash_ob);

  return flash_ob.RDPLevel;
}

/**
  * @brief  Return the FLASH Read Protection level.
  * @param  Level Can be one of these values:
  *         @arg OB_RDP_LEVEL_0: No protection
  *         @arg OB_RDP_LEVEL_1: Read protection of the memory
  *         @arg OB_RDP_LEVEL_2: Full chip protection
  * @retval None.
  */
void OPENBL_FLASH_SetReadOutProtectionLevel(uint32_t Level)
{
  FLASH_OBProgramInitTypeDef flash_ob;

  if (Level != OB_RDP_LEVEL2)
  {
    flash_ob.OptionType = OPTIONBYTE_RDP;
    flash_ob.RDPLevel   = Level;

    /* Unlock the FLASH registers & Option Bytes registers access */
    OPENBL_FLASH_OB_Unlock();

    /* Change the RDP level */
    HAL_FLASHEx_OBProgram(&flash_ob);
  }

  /* Register system reset callback */
  Common_SetPostProcessingCallback(OPENBL_OB_Launch);
}

/**
  * @brief  This function is used to enable or disable write protection of the specified FLASH areas.
  * @param  State Can be one of these values:
  *         @arg DISABLE: Disable FLASH write protection
  *         @arg ENABLE: Enable FLASH write protection
  * @param  ListOfPages Contains the list of pages to be protected.
  * @param  Length The length of the list of pages to be protected.
  * @retval An ErrorStatus enumeration value:
  *          - SUCCESS: Enable or disable of the write protection is done
  *          - ERROR:   Enable or disable of the write protection is not done
  */
ErrorStatus OPENBL_FLASH_SetWriteProtection(FunctionalState State, uint8_t *ListOfPages, uint32_t Length)
{
  ErrorStatus status = SUCCESS;

  if (State == ENABLE)
  {
    OPENBL_FLASH_EnableWriteProtection(ListOfPages, Length);

    /* Register system reset callback */
    Common_SetPostProcessingCallback(OPENBL_OB_Launch);
  }
  else if (State == DISABLE)
  {
    OPENBL_FLASH_DisableWriteProtection();

    /* Register system reset callback */
    Common_SetPostProcessingCallback(OPENBL_OB_Launch);
  }
  else
  {
    status = ERROR;
  }

  return status;
}

/**
  * @brief  This function is used to start FLASH mass erase operation.
  * @param  *p_Data Pointer to the buffer that contains mass erase operation options.
  * @param  DataLength Size of the Data buffer.
  * @retval An ErrorStatus enumeration value:
  *          - SUCCESS: Mass erase operation done
  *          - ERROR:   Mass erase operation failed or the value of one parameter is not OK
  */
ErrorStatus OPENBL_FLASH_MassErase(uint8_t *p_Data, uint32_t DataLength)
{
  uint32_t page_error;
  uint16_t bank_option;
  ErrorStatus status   = SUCCESS;
  FLASH_EraseInitTypeDef erase_init_struct;

  /* Unlock the flash memory for erase operation */
  OPENBL_FLASH_Unlock();

  erase_init_struct.TypeErase = FLASH_TYPEERASE_MASSERASE;

  if (DataLength >= 2)
  {
    bank_option = *(uint16_t *)(p_Data);

    if (bank_option == FLASH_MASS_ERASE)
    {
      erase_init_struct.Banks = 0U;
    }
    else if (bank_option == FLASH_BANK1_ERASE)
    {
      erase_init_struct.Banks = FLASH_BANK_1;
    }
    #ifdef FLASH_OPTR_DBANK
    else if (bank_option == FLASH_BANK2_ERASE)
    {
      erase_init_struct.Banks = FLASH_BANK_2;
    }
    #endif
    else
    {
      status = ERROR;
    }

    if (status == SUCCESS)
    {
      if (OPENBL_FLASH_ExtendedErase(&erase_init_struct, &page_error) != HAL_OK)
      {
        status = ERROR;
      }
      else
      {
        status = SUCCESS;
      }
    }
  }
  else
  {
    status = ERROR;
  }

  /* Lock the Flash to disable the flash control register access */
  OPENBL_FLASH_Lock();

  return status;
}

/**
  * @brief  This function is used to erase the specified FLASH pages.
  * @param  *p_Data Pointer to the buffer that contains erase operation options.
  * @param  DataLength Size of the Data buffer.
  * @retval An ErrorStatus enumeration value:
  *          - SUCCESS: Erase operation done
  *          - ERROR:   Erase operation failed or the value of one parameter is not OK
  */
ErrorStatus OPENBL_FLASH_Erase(uint8_t *p_Data, uint32_t DataLength)
{
  uint32_t counter;
  uint32_t pages_number;
  uint32_t page_error;
  uint32_t errors = 0U;
  ErrorStatus status = SUCCESS;
  FLASH_EraseInitTypeDef erase_init_struct;

  /* Unlock the flash memory for erase operation */
  OPENBL_FLASH_Unlock();

  /* Clear error programming flags */
  __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);

  pages_number  = (uint32_t)(*(uint16_t *)(p_Data));
  p_Data       += 2;

  erase_init_struct.TypeErase = FLASH_TYPEERASE_PAGES;
  erase_init_struct.NbPages   = 1U;

  for (counter = 0U; ((counter < pages_number) && (counter < (DataLength / 2U))) ; counter++)
  {
    erase_init_struct.Page = ((uint32_t)(*(uint16_t *)(p_Data)));

    if (erase_init_struct.Page <= 127)
    {
      erase_init_struct.Banks = FLASH_BANK_1;
    }
    #ifdef FLASH_OPTR_DBANK
    else if (erase_init_struct.Page <= 255)
    {
      erase_init_struct.Banks = FLASH_BANK_2;
    }
    #endif
    else
    {
      status = ERROR;
    }

    if (status != ERROR)
    {
      if (OPENBL_FLASH_ExtendedErase(&erase_init_struct, &page_error) != HAL_OK)
      {
        errors++;
      }
    }
    else
    {
      /* Reset the status for next erase operation */
      status = SUCCESS;
    }

    p_Data += 2;
  }

  if (errors > 0)
  {
    status = ERROR;
  }
  else
  {
    status = SUCCESS;
  }

  /* Lock the Flash to disable the flash control register access */
  OPENBL_FLASH_Lock();

  return status;
}

/**
 * @brief  This function is used to Set Flash busy state variable to activate busy state sending
 *         during flash operations
 * @retval None.
*/
void OPENBL_Enable_BusyState_Flag(void)
{
  /* Enable Flash busy state sending */
  Flash_BusyState = FLASH_BUSY_STATE_ENABLED;
}

/**
 * @brief  This function is used to disable the send of busy state in I2C non stretch mode.
 * @retval None.
*/
void OPENBL_Disable_BusyState_Flag(void)
{
  /* Disable Flash busy state sending */
  Flash_BusyState = FLASH_BUSY_STATE_DISABLED;
}

/* Private functions ---------------------------------------------------------*/

#ifdef FLASH_TYPEPROGRAM_QUADWORD
/**
  * @brief  Program double word at a specified FLASH address.
  * @param  Address specifies the address to be programmed.
  * @param  Data specifies the data to be programmed.
  * @retval None.
  */
static void OPENBL_FLASH_ProgramQuadWord(uint32_t Address, uint32_t Data)
{  
  HAL_FLASH_Program(FLASH_TYPEPROGRAM_QUADWORD, Address, Data); 
}
#else
/**
  * @brief  Program double word at a specified FLASH address.
  * @param  Address specifies the address to be programmed.
  * @param  Data specifies the data to be programmed.
  * @retval None.
  */
static void OPENBL_FLASH_ProgramDoubleWord(uint32_t Address, uint64_t Data)
{
  HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, Address, Data);
}
#endif
/**
  * @brief  This function is used to enable write protection of the specified FLASH areas.
  * @param  ListOfPages Contains the list of pages to be protected.
  * @param  Length The length of the list of pages to be protected.
  * @retval An ErrorStatus enumeration value:
  *          - SUCCESS: Enable or disable of the write protection is done
  *          - ERROR:   Enable or disable of the write protection is not done
  */
static ErrorStatus OPENBL_FLASH_EnableWriteProtection(uint8_t *ListOfPages, uint32_t Length)
{
  uint8_t wrp_start_offset = 0x7F;
  uint8_t wrp_end_offset   = 0x00;
  ErrorStatus status       = SUCCESS;
  FLASH_OBProgramInitTypeDef flash_ob;

  /* Unlock the FLASH registers & Option Bytes registers access */
  OPENBL_FLASH_OB_Unlock();

  flash_ob.OptionType = OPTIONBYTE_WRP;

  /* Write protection of bank 1 area WRPA 1 area */
  if (Length >= 2U)
  {
    wrp_start_offset = *(ListOfPages);
    wrp_end_offset   = *(ListOfPages + 1U);

    flash_ob.WRPArea        = OB_WRPAREA_BANK1_AREAA;
    flash_ob.WRPStartOffset = wrp_start_offset;
    flash_ob.WRPEndOffset   = wrp_end_offset;
    // flash_ob.WRPLock        = DISABLE;

    HAL_FLASHEx_OBProgram(&flash_ob);
  }

  /* Write protection of bank 1 area WRPA 2 area */
  if (Length >= 4U)
  {
    wrp_start_offset = *(ListOfPages + 2U);
    wrp_end_offset   = *(ListOfPages + 3U);

    flash_ob.WRPArea        = OB_WRPAREA_BANK1_AREAB;
    flash_ob.WRPStartOffset = wrp_start_offset;
    flash_ob.WRPEndOffset   = wrp_end_offset;
    // flash_ob.WRPLock        = DISABLE;

    HAL_FLASHEx_OBProgram(&flash_ob);
  }
#ifdef FLASH_OPTR_DBANK
  /* Write protection of bank 2 area WRPB 1 area */
  if (Length >= 6U)
  {
    wrp_start_offset = *(ListOfPages + 4);
    wrp_end_offset   = *(ListOfPages + 5);

    flash_ob.WRPArea        = OB_WRPAREA_BANK2_AREAA;
    flash_ob.WRPStartOffset = wrp_start_offset;
    flash_ob.WRPEndOffset   = wrp_end_offset;
    // flash_ob.WRPLock        = DISABLE;

    HAL_FLASHEx_OBProgram(&flash_ob);
  }

  /* Write protection of bank 2 area WRPB 2 area */
  if (Length >= 8U)
  {
    wrp_start_offset = *(ListOfPages + 6);
    wrp_end_offset   = *(ListOfPages + 7);

    flash_ob.WRPArea        = OB_WRPAREA_BANK2_AREAB;
    flash_ob.WRPStartOffset = wrp_start_offset;
    flash_ob.WRPEndOffset   = wrp_end_offset;
    // flash_ob.WRPLock        = DISABLE;

    HAL_FLASHEx_OBProgram(&flash_ob);
  }
#endif //FLASH_OPTR_DBANK
  return status;
}

/**
  * @brief  This function is used to disable write protection.
  * @retval An ErrorStatus enumeration value:
  *          - SUCCESS: Enable or disable of the write protection is done
  *          - ERROR:   Enable or disable of the write protection is not done
  */
static ErrorStatus OPENBL_FLASH_DisableWriteProtection(void)
{
  uint8_t wrp_start_offset = 0x7F;
  uint8_t wrp_end_offset   = 0x00;
  ErrorStatus status       = SUCCESS;
  FLASH_OBProgramInitTypeDef flash_ob;

  /* Unlock the FLASH registers & Option Bytes registers access */
  OPENBL_FLASH_OB_Unlock();

  flash_ob.OptionType = OPTIONBYTE_WRP;

  /* Disable write protection of bank 1 area WRPA A area */
  flash_ob.WRPArea        = OB_WRPAREA_BANK1_AREAA;
  flash_ob.WRPStartOffset = wrp_start_offset;
  flash_ob.WRPEndOffset   = wrp_end_offset;
  // flash_ob.WRPLock        = DISABLE;

  HAL_FLASHEx_OBProgram(&flash_ob);

  /* Disable write protection of bank 1 area WRPA B area */
  flash_ob.WRPArea        = OB_WRPAREA_BANK1_AREAB;
  flash_ob.WRPStartOffset = wrp_start_offset;
  flash_ob.WRPEndOffset   = wrp_end_offset;
  // flash_ob.WRPLock        = DISABLE;

  HAL_FLASHEx_OBProgram(&flash_ob);

#ifdef FLASH_OPTR_DBANK
  /* Disable write protection of bank 2 area WRPB A area */
  flash_ob.WRPArea        = OB_WRPAREA_BANK2_AREAA;
  flash_ob.WRPStartOffset = wrp_start_offset;
  flash_ob.WRPEndOffset   = wrp_end_offset;
  // flash_ob.WRPLock        = DISABLE;

  HAL_FLASHEx_OBProgram(&flash_ob);

  /* Disable write protection of bank 2 area WRPB B area */
  flash_ob.WRPArea        = OB_WRPAREA_BANK2_AREAB;
  flash_ob.WRPStartOffset = wrp_start_offset;
  flash_ob.WRPEndOffset   = wrp_end_offset;
  // flash_ob.WRPLock        = DISABLE;

  HAL_FLASHEx_OBProgram(&flash_ob);
#endif //FLASH_OPTR_DBANK
  return status;
}

/**
  * @brief  Wait for a FLASH operation to complete.
  * @param  Timeout maximum flash operation timeout.
  * @retval HAL_Status
  */
#if defined (__ICCARM__)
__ramfunc static HAL_StatusTypeDef OPENBL_FLASH_SendBusyState(uint32_t Timeout)
#else
__attribute__((section(".ramfunc"))) static HAL_StatusTypeDef OPENBL_FLASH_SendBusyState(uint32_t Timeout)
#endif /* (__ICCARM__) */
{
  uint32_t tick = 0;
  uint32_t error;
  #if 0
  __IO uint32_t *reg_sr; 
  #endif
  /* While the FLASH is in busy state, send busy byte to the host */
  while (__HAL_FLASH_GET_FLAG(FLASH_FLAG_BSY))
  {
    if (tick++ > Timeout)
    {
      return HAL_TIMEOUT;      
    }
    else
    {
      /* Send busy byte */
      #ifdef I2Cx
      OPENBL_I2C_SendBusyByte();
      #endif
    }
  }

  #if 0
  /* Access to SECSR or NSSR registers depends on operation type */
  reg_sr = IS_FLASH_SECURE_OPERATION() ? &(FLASH->SECSR) : &(FLASH_NS->NSSR);

  /* Check FLASH operation error flags */
  error = ((*reg_sr) & FLASH_FLAG_SR_ERRORS);
  #else
  /* Check FLASH operation error flags */
  error = (FLASH->SR & FLASH_FLAG_SR_ERRORS);
  #endif  

#if defined (__ARM_FEATURE_CMSE) && (__ARM_FEATURE_CMSE == 3U)
  error |= (FLASH->NSSR & FLASH_FLAG_OPTWERR);
#endif /* __ARM_FEATURE_CMSE */

  if (error != 0u)
  {
   /* Clear error programming flags */
    #if 0
    (*reg_sr) = error;
    #else      
    __HAL_FLASH_CLEAR_FLAG(error);
    #endif
#if defined (__ARM_FEATURE_CMSE) && (__ARM_FEATURE_CMSE == 3U)
    if ((error & FLASH_FLAG_OPTWERR) != 0U)
    {
      FLASH->NSSR = FLASH_FLAG_OPTWERR;
    }
#endif /* __ARM_FEATURE_CMSE */

    return HAL_ERROR;
  }

  /* Check FLASH End of Operation flag  */
  #if 0
  if (((*reg_sr) & FLASH_FLAG_EOP) != 0U)
  {
    /* Clear FLASH End of Operation pending bit */
    (*reg_sr) = FLASH_FLAG_EOP;
  }
  #else
  if (__HAL_FLASH_GET_FLAG(FLASH_FLAG_EOP))
  {
    /* Clear FLASH End of Operation pending bit */
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP);
  }
  #endif

  return HAL_OK;
}

/**
  * @brief  Wait for a FLASH operation to complete.
  * @param  Timeout maximum flash operation timeout.
  * @retval HAL_Status
  */
#if defined (__ICCARM__)
__ramfunc static HAL_StatusTypeDef OPENBL_FLASH_WaitForLastOperation(uint32_t Timeout)
#else
__attribute__((section(".ramfunc"))) static HAL_StatusTypeDef OPENBL_FLASH_WaitForLastOperation(uint32_t Timeout)
#endif /* (__ICCARM__) */
{
  uint32_t tick = 0U;
  uint32_t error = 0U;
  #if 0
  __IO uint32_t *reg_sr;  
  #endif

  /* While the FLASH is in busy state, send busy byte to the host */
  while (__HAL_FLASH_GET_FLAG(FLASH_FLAG_BSY))
  {
    if (tick++ > Timeout)
    {
      return HAL_TIMEOUT;
    }
  }

  #if 0
  /* Access to SECSR or NSSR registers depends on operation type */
  reg_sr = IS_FLASH_SECURE_OPERATION() ? &(FLASH->SECSR) : &(FLASH_NS->NSSR);

  /* Check FLASH operation error flags */
  error = ((*reg_sr) & FLASH_FLAG_SR_ERRORS);
  #else
  /* Check FLASH operation error flags */
  error = (FLASH->SR & FLASH_FLAG_SR_ERRORS);
  #endif

#if defined (__ARM_FEATURE_CMSE) && (__ARM_FEATURE_CMSE == 3U)
  error |= (FLASH->NSSR & FLASH_FLAG_OPTWERR);
#endif /* __ARM_FEATURE_CMSE */

  if (error != 0u)
  {
    /*Save the error code*/
    FlashProcess.ErrorCode |= error;

    /* Clear error programming flags */
    #if 0
    (*reg_sr) = error;
    #else      
    __HAL_FLASH_CLEAR_FLAG(error);
    #endif
#if defined (__ARM_FEATURE_CMSE) && (__ARM_FEATURE_CMSE == 3U)
    if ((error & FLASH_FLAG_OPTWERR) != 0U)
    {
      FLASH->NSSR = FLASH_FLAG_OPTWERR;
    }
#endif /* __ARM_FEATURE_CMSE */

    return HAL_ERROR;
  }

  /* Check FLASH End of Operation flag  */
  #if 0
  if (((*reg_sr) & FLASH_FLAG_EOP) != 0U)
  {
    /* Clear FLASH End of Operation pending bit */
    (*reg_sr) = FLASH_FLAG_EOP;
  }
  #else
  if (__HAL_FLASH_GET_FLAG(FLASH_FLAG_EOP))
  {
    /* Clear FLASH End of Operation pending bit */
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP);
  }
  #endif

  return HAL_OK;
}

/**
  * @brief  Perform a mass erase or erase the specified FLASH memory pages.
  * @param[in]  pEraseInit pointer to an FLASH_EraseInitTypeDef structure that
  *         contains the configuration information for the erasing.
  * @param[out]  PageError pointer to variable that contains the configuration
  *         information on faulty page in case of error (0xFFFFFFFF means that all
  *         the pages have been correctly erased).
  * @retval HAL_Status
  */
#if defined (__ICCARM__)
__ramfunc static HAL_StatusTypeDef OPENBL_FLASH_ExtendedErase(FLASH_EraseInitTypeDef *pEraseInit, uint32_t *PageError)
#else
__attribute__((section(".ramfunc"))) static HAL_StatusTypeDef OPENBL_FLASH_ExtendedErase(
  FLASH_EraseInitTypeDef *pEraseInit, uint32_t *PageError)
#endif /* (__ICCARM__) */
{
  HAL_StatusTypeDef status;
  #if 0
  uint32_t errors = 0U;
  __IO uint32_t *reg_cr;
  #endif
#if defined (__ARM_FEATURE_CMSE) && (__ARM_FEATURE_CMSE == 3U)
  uint32_t primask_bit;
#endif

  /* Process Locked */
  __HAL_LOCK(&FlashProcess);

  /* Reset error code */
  FlashProcess.ErrorCode = HAL_FLASH_ERROR_NONE;

  /* Verify that next operation can be proceed */
  status = OPENBL_FLASH_WaitForLastOperation(PROGRAM_TIMEOUT);

  if (status == HAL_OK)
  {
    #if 0 //TODO: A different type of processor
    FlashProcess.ProcedureOnGoing = pEraseInit->TypeErase;

    /* Access to SECCR or NSCR depends on operation type */
    reg_cr = IS_FLASH_SECURE_OPERATION() ? &(FLASH->SECCR) : &(FLASH->NSCR);

    /*Initialization of PageError variable*/
    *PageError = 0xFFFFFFFFU;

    /* Access to SECCR or NSCR registers depends on operation type */
    reg_cr = IS_FLASH_SECURE_OPERATION() ? &(FLASH->SECCR) : &(FLASH_NS->NSCR);

    if (((pEraseInit->Banks) & FLASH_BANK_1) != 0U)
    {
      CLEAR_BIT((*reg_cr), FLASH_NSCR_BKER);
    }
    else
    {
      SET_BIT((*reg_cr), FLASH_NSCR_BKER);
    }

    /* Proceed to erase the page */
    MODIFY_REG((*reg_cr), (FLASH_NSCR_PNB | FLASH_NSCR_PER | FLASH_NSCR_STRT),
               (((pEraseInit->Page) << FLASH_NSCR_PNB_Pos) | FLASH_NSCR_PER | FLASH_NSCR_STRT));

    if (Flash_BusyState == FLASH_BUSY_STATE_ENABLED)
    {
      /* Wait for last operation to be completed to send busy byte */
      if (OPENBL_FLASH_SendBusyState(PROGRAM_TIMEOUT) != HAL_OK)
      {
        errors++;
      }
    }
    else
    {
      /* Wait for last operation to be completed */
      if (OPENBL_FLASH_WaitForLastOperation(PROGRAM_TIMEOUT) != HAL_OK)
      {
        errors++;
      }
    }

    /* If the erase operation is completed, disable the associated bits */
    CLEAR_BIT((*reg_cr), (pEraseInit->TypeErase) & (~(FLASH_NON_SECURE_MASK)));
    #else
    /* Deactivate the cache if they are activated to avoid data misbehavior */
    if (READ_BIT(FLASH->ACR, FLASH_ACR_ICEN) != 0U)
    {
      /* Disable instruction cache  */
      __HAL_FLASH_INSTRUCTION_CACHE_DISABLE();

      if (READ_BIT(FLASH->ACR, FLASH_ACR_DCEN) != 0U)
      {
        /* Disable data cache  */
        __HAL_FLASH_DATA_CACHE_DISABLE();
        FlashProcess.CacheToReactivate = FLASH_CACHE_ICACHE_DCACHE_ENABLED;
      }
      else
      {
        FlashProcess.CacheToReactivate = FLASH_CACHE_ICACHE_ENABLED;
      }
    }
    else if (READ_BIT(FLASH->ACR, FLASH_ACR_DCEN) != 0U)
    {
      /* Disable data cache  */
      __HAL_FLASH_DATA_CACHE_DISABLE();
      FlashProcess.CacheToReactivate = FLASH_CACHE_DCACHE_ENABLED;
    }
    else
    {
      FlashProcess.CacheToReactivate = FLASH_CACHE_DISABLED;
    }

    if (pEraseInit->TypeErase == FLASH_TYPEERASE_MASSERASE)
    {
      /* Check the parameters */
      assert_param(IS_FLASH_BANK(Banks));
      
      /* Mass erase to be done */
      SET_BIT(FLASH->CR, FLASH_CR_MER1);
      
      /* Proceed to erase all sectors */
      SET_BIT(FLASH->CR, FLASH_CR_STRT);

      if (Flash_BusyState == FLASH_BUSY_STATE_ENABLED)
      {
        /* Wait for last operation to be completed to send busy byte*/
        status = OPENBL_FLASH_SendBusyState(FLASH_TIMEOUT_VALUE);
      }
      else
      {
        /* Wait for last operation to be completed */
        status = FLASH_WaitForLastOperation(FLASH_TIMEOUT_VALUE);
      }

      /* If the erase operation is completed, disable the MER1 Bit */
      CLEAR_BIT(FLASH->CR, (FLASH_CR_MER1));
    }
    else
    {
      /*Initialization of PageError variable*/
      *PageError = 0xFFFFFFFFU;

      for (uint32_t page_index = pEraseInit->Page; page_index < (pEraseInit->Page + pEraseInit->NbPages); page_index++)
      {
        FLASH_PageErase(page_index, pEraseInit->Banks);

        if (Flash_BusyState == FLASH_BUSY_STATE_ENABLED)
        {
          /* Wait for last operation to be completed to send busy byte*/
          status = OPENBL_FLASH_SendBusyState(FLASH_TIMEOUT_VALUE);
        }
        else
        {
          /* Wait for last operation to be completed */
          status = FLASH_WaitForLastOperation(PROGRAM_TIMEOUT);
        }

        /* If the erase operation is completed, disable the PER Bit */
        CLEAR_BIT(FLASH->CR, (FLASH_CR_PER | FLASH_CR_PNB));

        if (status != HAL_OK)
        {
          /* In case of error, stop erase procedure and return the faulty page */
          *PageError = page_index;
          break;
        }
      }
    }

    /* Flush the caches to be sure of the data consistency */
    FLASH_FlushCaches();
    #endif
  }

  /* Process Unlocked */
  __HAL_UNLOCK(&FlashProcess);

  /* Disable Flash busy state sending */
  OPENBL_Disable_BusyState_Flag();

  #if 0 //TODO:
  if (errors > 0)
  {
    status = HAL_ERROR;
  }
  else
  {
    status = HAL_OK;
  }
  #endif
  return status;
}
