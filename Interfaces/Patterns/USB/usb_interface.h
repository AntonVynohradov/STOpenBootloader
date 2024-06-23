/**
  ******************************************************************************
  * @file    usb_interface.h
  * @author  MCD Application Team
  * @brief   Contains USB protocol commands
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef USB_INTERFACE_H
#define USB_INTERFACE_H

/* Includes ------------------------------------------------------------------*/
#include "platform.h"


#ifdef __cplusplus
extern "C" {
#endif

#ifdef USB_OTG_FS
/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/
/* Exported macro ------------------------------------------------------------*/
/* Exported functions ------------------------------------------------------- */
void OPENBL_USB_Configuration(void);
void OPENBL_USB_DeInit(void);
uint8_t OPENBL_USB_ProtocolDetection(void);
uint32_t OPENBL_USB_GetPage(uint32_t Address);

#endif //USB_OTG_FS

#ifdef __cplusplus
}
#endif

#endif /* USB_INTERFACE_H */
