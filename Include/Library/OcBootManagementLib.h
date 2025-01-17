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

#ifndef OC_BOOT_MANAGEMENT_LIB_H
#define OC_BOOT_MANAGEMENT_LIB_H

#include <Library/OcAppleBootPolicyLib.h>

/**
  Discovered boot entry.
  Note, inner resources must be freed with OcResetBootEntry.
**/
typedef struct OC_BOOT_ENTRY_ {
  //
  // Device path to booter or its directory.
  //
  EFI_DEVICE_PATH_PROTOCOL  *DevicePath;
  //
  // Obtained human visible name.
  //
  CHAR16                    *Name;
  //
  // Obtained boot path directory.
  //
  CHAR16                    *PathName;
  //
  // Should try booting from first dmg found in DevicePath.
  //
  BOOLEAN                   IsFolder;
  //
  // Heuristical value signalising about recovery os.
  //
  BOOLEAN                   IsRecovery;
  //
  // Heuristical value signalising about Windows os (otherwise macOS).
  //
  BOOLEAN                   IsWindows;
  //
  // Load option data (usually "boot args")
  //
  UINT32                    LoadOptionsSize;
  VOID                      *LoadOptions;
} OC_BOOT_ENTRY;

/**
  Describe boot entry contents by setting fields other than DevicePath.

  @param[in]  BootPolicy     Apple Boot Policy Protocol.
  @param[in]  BootEntry      Located boot entry.

  @retval EFI_SUCCESS          The entry point is described successfully.
**/
EFI_STATUS
OcDescribeBootEntry (
  IN     APPLE_BOOT_POLICY_PROTOCOL *BootPolicy,
  IN OUT OC_BOOT_ENTRY              *BootEntry
  );

/**
  Release boot entry contents allocated from pool.

  @param[in,out]  BootEntry      Located boot entry.
**/
VOID
OcResetBootEntry (
  IN OUT OC_BOOT_ENTRY              *BootEntry
  );

/**
  Release boot entries.

  @param[in,out]  BootEntry      Located boot entry array from pool.
  @param[in]      Count          Boot entry count.
**/
VOID
OcFreeBootEntries (
  IN OUT OC_BOOT_ENTRY              *BootEntries,
  IN     UINTN                      Count
  );

/**
  Perform filtering based on file system basis.
  Ignores all filesystems by default.
  Remove this bit to allow any file system.
**/
#define OC_SCAN_FILE_SYSTEM_LOCK         BIT0

/**
  Perform filtering based on device basis.
  Ignores all devices by default.
  Remove this bit to allow any device type.
**/
#define OC_SCAN_DEVICE_LOCK              BIT1

/**
  Allow scanning APFS filesystems.
**/
#define OC_SCAN_ALLOW_FS_APFS            BIT8

/**
  Allow scanning HFS filesystems.
**/
#define OC_SCAN_ALLOW_FS_HFS             BIT9

/**
  Allow scanning SATA devices.
**/
#define OC_SCAN_ALLOW_DEVICE_SATA        BIT16

/**
  Allow scanning SAS and Mac NVMe devices.
**/
#define OC_SCAN_ALLOW_DEVICE_SASEX       BIT17

/**
  Allow scanning SCSI devices.
**/
#define OC_SCAN_ALLOW_DEVICE_SCSI        BIT18

/**
  Allow scanning NVMe devices.
**/
#define OC_SCAN_ALLOW_DEVICE_NVME        BIT19

/**
  Allow scanning ATAPI devices.
**/
#define OC_SCAN_ALLOW_DEVICE_ATAPI       BIT20

/**
  Allow scanning USB devices.
**/
#define OC_SCAN_ALLOW_DEVICE_USB         BIT21

/**
  Allow scanning FireWire devices.
**/
#define OC_SCAN_ALLOW_DEVICE_FIREWIRE    BIT22

/**
  Allow scanning SD card devices.
**/
#define OC_SCAN_ALLOW_DEVICE_SDCARD      BIT23

/**
  All device bits used by OC_SCAN_DEVICE_LOCK.
**/
#define OC_SCAN_DEVICE_BITS ( \
  OC_SCAN_ALLOW_DEVICE_SATA     | OC_SCAN_ALLOW_DEVICE_SASEX | \
  OC_SCAN_ALLOW_DEVICE_SCSI     | OC_SCAN_ALLOW_DEVICE_NVME  | \
  OC_SCAN_ALLOW_DEVICE_ATAPI    | OC_SCAN_ALLOW_DEVICE_USB   | \
  OC_SCAN_ALLOW_DEVICE_FIREWIRE | OC_SCAN_ALLOW_DEVICE_SDCARD)

/**
  All device bits used by OC_SCAN_DEVICE_LOCK.
**/
#define OC_SCAN_FILE_SYSTEM_BITS ( \
  OC_SCAN_ALLOW_FS_APFS | OC_SCAN_ALLOW_FS_HFS)

/**
  By default allow booting from APFS from internal drives.
**/
#define OC_SCAN_DEFAULT_POLICY ( \
  OC_SCAN_FILE_SYSTEM_LOCK   | OC_SCAN_DEVICE_LOCK | \
  OC_SCAN_ALLOW_FS_APFS      | OC_SCAN_ALLOW_DEVICE_SATA | \
  OC_SCAN_ALLOW_DEVICE_SASEX | OC_SCAN_ALLOW_DEVICE_SCSI | \
  OC_SCAN_ALLOW_DEVICE_NVME)

/**
  Fill boot entry from device handle.

  @param[in]  BootPolicy          Apple Boot Policy Protocol.
  @param[in]  Policy              Scan policy.
  @param[in]  Handle              Device handle (with EfiSimpleFileSystem protocol).
  @param[in]  SimpleFs            Simple file system protocol of the device handle.
  @param[out] BootEntry           Resulting boot entry.
  @param[out] AlternateBootEntry  Resulting alternate boot entry (e.g. recovery).
  @param[in]  IsLoadHandle        OpenCore load handle, try skipping OC entry.

  @retval 0  no entries were filled.
  @retval 1  boot entry was filled.
  @retval 2  boot entry and alternate entry were filled.
**/
UINTN
OcFillBootEntry (
  IN  APPLE_BOOT_POLICY_PROTOCOL      *BootPolicy,
  IN  UINT32                          Policy,
  IN  EFI_HANDLE                      Handle,
  IN  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *SimpleFs,
  OUT OC_BOOT_ENTRY                   *BootEntry,
  OUT OC_BOOT_ENTRY                   *AlternateBootEntry OPTIONAL,
  IN  BOOLEAN                         IsLoadHandle
  );

/**
  Scan system for boot entries.

  @param[in]  BootPolicy     Apple Boot Policy Protocol.
  @param[in]  Policy         Scan policy.
  @param[out] BootEntries    List of boot entries (allocated from pool).
  @param[out] Count          Number of boot entries.
  @param[out] AllocCount     Number of allocated boot entries.
  @param[in]  LoadHandle     Load handle to skip.
  @param[in]  Describe       Automatically fill description fields

  @retval EFI_SUCCESS        Executed successfully and found entries.
**/
EFI_STATUS
OcScanForBootEntries (
  IN  APPLE_BOOT_POLICY_PROTOCOL  *BootPolicy,
  IN  UINT32                      Policy,
  OUT OC_BOOT_ENTRY               **BootEntries,
  OUT UINTN                       *Count,
  OUT UINTN                       *AllocCount OPTIONAL,
  IN  EFI_HANDLE                  LoadHandle  OPTIONAL,
  IN  BOOLEAN                     Describe
  );

/**
  Show simple boot entry selection menu and return chosen entry.

  @param[in]  BootEntries      Described list of entries.
  @param[in]  Count            Positive number of boot entries.
  @param[in]  DefaultEntry     Default boot entry (DefaultEntry < Count).
  @param[in]  TimeOutSeconds   Default entry selection timeout (pass 0 to ignore).
  @param[in]  ChosenBootEntry  Chosen boot entry from BootEntries on success.

  @retval EFI_SUCCESS          Executed successfully and picked up an entry.
  @retval EFI_ABORTED          When the user chose to by pressing Esc or 0.
**/
EFI_STATUS
OcShowSimpleBootMenu (
  IN  OC_BOOT_ENTRY               *BootEntries,
  IN  UINTN                       Count,
  IN  UINTN                       DefaultEntry,
  IN  UINTN                       TimeOutSeconds,
  OUT OC_BOOT_ENTRY               **ChosenBootEntry
  );

/**
  OcLoadBootEntry Mode policy bits allow to configure OcLoadBootEntry behaviour.
**/

/**
  Thin EFI image loading (normal PE) is allowed.
**/
#define OC_LOAD_ALLOW_EFI_THIN_BOOT  BIT0
/**
  FAT EFI image loading (Apple FAT PE) is allowed.
  These can be found on macOS 10.8 and below.
**/
#define OC_LOAD_ALLOW_EFI_FAT_BOOT   BIT1
/**
  One level recursion into dmg file is allowed.
  It is assumed that dmg contains a single volume and a single blessed entry.
  Loading dmg from dmg is not allowed in any case.
**/
#define OC_LOAD_ALLOW_DMG_BOOT       BIT2
/**
  Abort loading on invalid Apple-like signature.
  If file is signed with Apple-like signature, and it is mismatched, then abort.
  @warn Unsigned files or UEFI-signed files will skip this check.
  @warn It is ignored what certificate was used for signing.
**/
#define OC_LOAD_VERIFY_APPLE_SIGN    BIT8
/**
  Abort loading on missing Apple-like signature.
  If file is not signed with Apple-like signature (valid or not) then abort.
  @warn Unsigned files or UEFI-signed files will not load with this check. 
  @warn Without OC_LOAD_VERIFY_APPLE_SIGN corrupted binaries may still load.
**/
#define OC_LOAD_REQUIRE_APPLE_SIGN   BIT9
/**
  Abort loading on untrusted key (otherwise may warn).
  @warn Unsigned files or UEFI-signed files will skip this check.
**/
#define OC_LOAD_REQUIRE_TRUSTED_KEY  BIT10
/**
  Trust specified (as OcLoadBootEntry argument) custom keys.
**/
#define OC_LOAD_TRUST_CUSTOM_KEY     BIT16
/**
  Trust Apple CFFD3E6B public key.
  TODO: Move certificates from ApplePublicKeyDb.h to EfiPkg?
**/
#define OC_LOAD_TRUST_APPLE_V1_KEY   BIT17
/**
  Trust Apple E50AC288 public key.
  TODO: Move certificates from ApplePublicKeyDb.h to EfiPkg?
**/
#define OC_LOAD_TRUST_APPLE_V2_KEY   BIT18
/**
  Default moderate policy meant to augment secure boot facilities.
  Loads almost everything and bypasses secure boot for Apple and Custom signed binaries.
**/
#define OC_LOAD_DEFAULT_POLICY ( \
  OC_LOAD_ALLOW_EFI_THIN_BOOT | OC_LOAD_ALLOW_DMG_BOOT      | OC_LOAD_REQUIRE_APPLE_SIGN | \
  OC_LOAD_VERIFY_APPLE_SIGN   | OC_LOAD_REQUIRE_TRUSTED_KEY | \
  OC_LOAD_TRUST_CUSTOM_KEY    | OC_LOAD_TRUST_APPLE_V1_KEY  | OC_LOAD_TRUST_APPLE_V2_KEY)

/**
  Load boot entry loader image with given options and return its handle.

  @param[in]  BootPolicy     Apple Boot Policy Protocol.
  @param[in]  BootEntry      Located boot entry.
  @param[in]  Policy         Load policy.
  @param[in]  ParentHandle   Parent image handle.
  @param[out] EntryHandle    Loaded image handle.

  @retval EFI_SUCCESS        The image was found and loaded succesfully.
**/
EFI_STATUS
OcLoadBootEntry (
  IN  APPLE_BOOT_POLICY_PROTOCOL  *BootPolicy,
  IN  OC_BOOT_ENTRY               *BootEntry,
  IN  UINT32                      Policy,
  IN  EFI_HANDLE                  ParentHandle,
  OUT EFI_HANDLE                  *EntryHandle
  );

/**
  Exposed start interface with chosen boot entry but otherwise equivalent
  to EFI_BOOT_SERVICES StartImage.
**/
typedef
EFI_STATUS
(EFIAPI *OC_IMAGE_START) (
  IN  OC_BOOT_ENTRY               *ChosenEntry,
  IN  EFI_HANDLE                  ImageHandle,
  OUT UINTN                       *ExitDataSize,
  OUT CHAR16                      **ExitData    OPTIONAL
  );

/**
  Install missing boot policy, scan, and show simple boot menu.

  @param[in]  ScanPolicy       Scan policy.
  @param[in]  BootPolicy       Boot policy.
  @param[in]  TimeOutSeconds   Default entry selection timeout (pass 0 to ignore).
  @param[in]  StartImage       Image starting routine used.
  @param[in]  ShowPicker       Show boot menu or just boot the default option.
  @param[in]  CustomBootGuid   Use custom (gOcVendorVariableGuid) for Boot#### variables.
  @param[in]  LoadHandle       Load handle, skips main handle on this handle.

  @retval does not return unless a fatal error happened.
**/
EFI_STATUS
OcRunSimpleBootPicker (
  IN  UINT32           ScanPolicy,
  IN  UINT32           BootPolicy,
  IN  UINT32           TimeoutSeconds,
  IN  OC_IMAGE_START   StartImage,
  IN  BOOLEAN          ShowPicker,
  IN  BOOLEAN          CustomBootGuid,
  IN  EFI_HANDLE       LoadHandle  OPTIONAL
  );

#endif // OC_BOOT_MANAGEMENT_LIB_H
