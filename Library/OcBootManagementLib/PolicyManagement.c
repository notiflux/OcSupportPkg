/** @file
  Copyright (C) 2019, vit9696. All rights reserved.

  All rights reserved.

  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
**/

#include "BootManagementInternal.h"

#include <Guid/AppleApfsInfo.h>
#include <Guid/AppleBless.h>
#include <Guid/AppleVariable.h>
#include <Guid/FileInfo.h>
#include <Guid/GlobalVariable.h>
#include <Guid/OcVariables.h>

#include <Protocol/AppleBootPolicy.h>
#include <Protocol/ApfsEfiBootRecordInfo.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/SimpleTextOut.h>

#include <Library/DevicePathLib.h>
#include <Library/OcBootManagementLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>

EFI_STATUS
InternalCheckScanPolicy (
  IN  EFI_HANDLE                       Handle,
  IN  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL  *SimpleFs,
  IN  UINT32                           Policy
  )
{
  EFI_STATUS                Status;
  EFI_DEVICE_PATH_PROTOCOL  *DevicePath;
  EFI_FILE_PROTOCOL         *Root;
  UINTN                     BufferSize;

  Status = EFI_SUCCESS;


  if ((Policy & OC_SCAN_DEVICE_LOCK) != 0) {
    Status = gBS->HandleProtocol (Handle, &gEfiDevicePathProtocolGuid, (VOID **) &DevicePath);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    Status = EFI_SECURITY_VIOLATION;

    while (!IsDevicePathEnd (DevicePath)) {
      if (DevicePathType (DevicePath) == MESSAGING_DEVICE_PATH) {
        if ((Policy & OC_SCAN_ALLOW_DEVICE_SATA) != 0
          && DevicePathSubType (DevicePath) == MSG_SATA_DP) {
          Status = EFI_SUCCESS;
        } else if ((Policy & OC_SCAN_ALLOW_DEVICE_SASEX) != 0
          && DevicePathSubType (DevicePath) == MSG_SASEX_DP) {
          Status = EFI_SUCCESS;
        } else if ((Policy & OC_SCAN_ALLOW_DEVICE_SCSI) != 0
          && DevicePathSubType (DevicePath) == MSG_SCSI_DP) {
          Status = EFI_SUCCESS;
        } else if ((Policy & OC_SCAN_ALLOW_DEVICE_NVME) != 0
          && DevicePathSubType (DevicePath) == MSG_NVME_NAMESPACE_DP) {
          Status = EFI_SUCCESS;
        } else if ((Policy & OC_SCAN_ALLOW_DEVICE_ATAPI) != 0
          && DevicePathSubType (DevicePath) == MSG_ATAPI_DP) {
          Status = EFI_SUCCESS;
        } else if ((Policy & OC_SCAN_ALLOW_DEVICE_USB) != 0
          && DevicePathSubType (DevicePath) == MSG_USB_DP) {
          Status = EFI_SUCCESS;
        } else if ((Policy & OC_SCAN_ALLOW_DEVICE_FIREWIRE) != 0
          && DevicePathSubType (DevicePath) == MSG_1394_DP) {
          Status = EFI_SUCCESS;
        } else if ((Policy & OC_SCAN_ALLOW_DEVICE_SDCARD) != 0
          && (DevicePathSubType (DevicePath) == MSG_EMMC_DP
            || DevicePathSubType (DevicePath) == MSG_SD_DP)) {
          Status = EFI_SUCCESS;
        }

        //
        // We do not have good protection against device tunneling.
        // These things must be considered:
        // - Thunderbolt 2 PCI-e pass-through
        // - Thunderbolt 3 PCI-e pass-through (Type-C, may be different from 2)
        // - FireWire devices
        // For now we hope that first messaging type protects us, and all
        // subsequent messaging types are tunneled.
        //

        break;
      }

      DevicePath = NextDevicePathNode (DevicePath);
    }

    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  if ((Policy & OC_SCAN_FILE_SYSTEM_LOCK) != 0) {
    Status = SimpleFs->OpenVolume (SimpleFs, &Root);

    if (EFI_ERROR (Status)) {
      return Status;
    }

    Status = EFI_SECURITY_VIOLATION;

    //
    // FIXME: We cannot use EfiPartitionInfo protocol, as it is not
    // widely available, and when it is, it is not guaranteed to be spec compliant.
    // For this reason we would really like to implement ApplePartitionInfo protocol,
    // but currently it is not a priority.
    //
    if ((Policy & OC_SCAN_ALLOW_FS_APFS) != 0 && EFI_ERROR (Status)) {
      BufferSize = 0;
      Status = Root->GetInfo (Root, &gAppleApfsVolumeInfoGuid, &BufferSize, NULL);
      if (Status == EFI_BUFFER_TOO_SMALL) {
        Status = EFI_SUCCESS;
      }
    }

    //
    // FIXME: This is even worse but works for testing the concept purposes.
    // Current logic is blessed but not APFS.
    //
    if ((Policy & OC_SCAN_ALLOW_FS_HFS) != 0 && EFI_ERROR (Status)) {
      BufferSize = 0;
      Status = Root->GetInfo (Root, &gAppleApfsVolumeInfoGuid, &BufferSize, NULL);
      if (Status != EFI_BUFFER_TOO_SMALL) {
        BufferSize = 0;
        Status = Root->GetInfo (Root, &gAppleBlessedSystemFileInfoGuid, &BufferSize, NULL);
        if (Status == EFI_BUFFER_TOO_SMALL) {
          Status = EFI_SUCCESS;
        } else {
          BufferSize = 0;
          Status = Root->GetInfo (Root, &gAppleBlessedSystemFolderInfoGuid, &BufferSize, NULL);
          if (Status == EFI_BUFFER_TOO_SMALL) {
            Status = EFI_SUCCESS;
          }
        }
      }
    }

    Root->Close (Root);

    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  return EFI_SUCCESS;
}
