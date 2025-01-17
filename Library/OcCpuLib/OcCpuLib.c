/** @file
  Copyright (C) 2016 - 2017, The HermitCrabs Lab. All rights reserved.

  All rights reserved.

  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
**/

#include <Uefi.h>

#include <IndustryStandard/CpuId.h>
#include <IndustryStandard/GenericIch.h>
#include <IndustryStandard/Pci.h>
#include <IndustryStandard/AppleSmBios.h>

#include <Protocol/PciIo.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/IoLib.h>
#include <Library/OcCpuLib.h>
#include <Library/PciLib.h>
#include <Library/OcStringLib.h>
#include <Library/OcTimerLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>

#include <ProcessorInfo.h>
#include <Register/Msr.h>
#include <Register/Msr/SandyBridgeMsr.h>
#include <Register/Msr/NehalemMsr.h>

STATIC
UINT8
DetectAppleMajorType (
  CONST CHAR8  *BrandString
  )
{
  CONST CHAR8  *BrandInfix;

  BrandInfix = AsciiStrStr (BrandString, "Core");
  if (BrandInfix != NULL) {
    while ((*BrandInfix != ' ') && (*BrandInfix != '\0')) {
      ++BrandInfix;
    }

    while (*BrandInfix == ' ') {
      ++BrandInfix;
    }

    if (AsciiStrnCmp (BrandInfix, "i7", L_STR_LEN ("i7")) == 0) {
      return AppleProcessorMajorI7;
    }
    if (AsciiStrnCmp (BrandInfix, "i5", L_STR_LEN ("i5")) == 0) {
      return AppleProcessorMajorI5;
    }
    if (AsciiStrnCmp (BrandInfix, "i3", L_STR_LEN ("i3")) == 0) {
      return AppleProcessorMajorI3;
    }
    if (AsciiStrnCmp (BrandInfix, "i9", L_STR_LEN ("i9")) == 0) {
      return AppleProcessorMajorI9;
    }
    if (AsciiStrnCmp (BrandInfix, "m3", L_STR_LEN ("m3")) == 0) {
      return AppleProcessorMajorM3;
    }
    if (AsciiStrnCmp (BrandInfix, "m5", L_STR_LEN ("m5")) == 0) {
      return AppleProcessorMajorM5;
    }
    if (AsciiStrnCmp (BrandInfix, "m7", L_STR_LEN ("m7")) == 0) {
      return AppleProcessorMajorM7;
    }
    if (AsciiStrnCmp (BrandInfix, "M", L_STR_LEN ("M")) == 0) {
      return AppleProcessorMajorM;
    }
    if (AsciiStrnCmp (BrandInfix, "Duo", L_STR_LEN ("Duo")) == 0) {
      return AppleProcessorMajorCore2;
    }
    if (AsciiStrnCmp (BrandInfix, "Quad", L_STR_LEN ("Quad")) == 0) {
      return AppleProcessorMajorXeonPenryn;
    }
    return AppleProcessorMajorCore;
  }

  BrandInfix = AsciiStrStr (BrandString, "Xeon");
  if (BrandInfix != NULL) {
    while ((*BrandInfix != ' ') && (*BrandInfix != '\0')) {
      ++BrandInfix;
    }

    while (*BrandInfix == ' ') {
      ++BrandInfix;
    }

    //
    // Support both variants: Xeon(R) E5-1234 and Xeon(R) CPU E5-1234
    //
    if (AsciiStrnCmp (BrandInfix, "CPU", L_STR_LEN ("CPU")) == 0) {
      BrandInfix += L_STR_LEN ("CPU");
      while (*BrandInfix == ' ') {
        ++BrandInfix;
      }
    }

    if (AsciiStrnCmp (BrandInfix, "E5", L_STR_LEN ("E5")) == 0) {
      return AppleProcessorMajorXeonE5;
    }
    if (AsciiStrnCmp (BrandInfix, "W", L_STR_LEN ("W")) == 0) {
      return AppleProcessorMajorXeonW;
    }
    return AppleProcessorMajorXeonNehalem;
  }

  return AppleProcessorMajorUnknown;
}

STATIC
UINT16
DetectAppleProcessorType (
  IN UINT8  Model,
  IN UINT8  Stepping,
  IN UINT8  AppleMajorType
  )
{
  switch (Model) {
    //
    // Yonah: https://en.wikipedia.org/wiki/Yonah_(microprocessor)#Models_and_brand_names
    //
    // Used by Apple:
    //   Core Duo, Core Solo
    //
    // NOT used by Apple:
    //   Pentium, Celeron
    //
    // All 0x0201.
    //
    case CPU_MODEL_DOTHAN: // 0x0D
    case CPU_MODEL_YONAH:  // 0x0E
      // IM41  (T2400/T2500), MM11 (Solo T1200 / Duo T2300/T2400),
      // MBP11 (L2400/T2400/T2500/T2600), MBP12 (T2600),
      // MB11  (T2400/T2500)
      return AppleProcessorTypeCoreSolo; // 0x0201

    //
    // Merom:  https://en.wikipedia.org/wiki/Merom_(microprocessor)#Variants
    // Penryn: https://en.wikipedia.org/wiki/Penryn_(microprocessor)#Variants
    //
    // Used by Apple:
    //   Core 2 Extreme, Core 2 Duo (Merom),
    //   Core 2 Duo,                (Penryn),
    //   certain Clovertown (Merom) / Harpertown (Penryn) based models
    //
    // Not used by Apple:
    //   Merom:  Core 2 Solo, Pentium, Celeron M, Celeron
    //   Penryn: Core 2 Extreme, Core 2 Quad, Core 2 Solo, Pentium, Celeron
    //
    case CPU_MODEL_MEROM:  // 0x0F
    case CPU_MODEL_PENRYN: // 0x17
      if (AppleMajorType == AppleProcessorMajorCore2) {
        // TODO: add check for models above. (by changing the following "if (0)")
        if (0) {
          // ONLY MBA31 (SU9400/SU9600) and MBA32 (SL9400/SL9600)
          return AppleProcessorTypeCore2DuoType2; // 0x0302
        }
        // IM51 (T7200), IM61 (T7400), IM71 (T7300), IM81 (E8435), IM101 (E7600),
        // MM21 (unknown), MM31 (P7350),
        // MBP21 (T7600), MBP22 (unknown), MBP31 (T7700), MBP41 (T8300), MBP71 (P8600),
        // MBP51 (P8600), MBP52 (T9600), MBP53 (P8800), MBP54 (P8700), MBP55 (P7550),
        // MBA11 (P7500), MBA21 (SL9600),
        // MB21 (unknown), MB31 (T7500), MB41 (T8300), MB51 (P8600), MB52 (P7450), MB61 (P7550), MB71 (P8600)
        return AppleProcessorTypeCore2DuoType1; // 0x0301
      }
      if (AppleMajorType == AppleProcessorMajorXeonPenryn) {
        // MP21 (2x X5365), MP31 (2x E5462) - 0x0402
        // FIXME: check when 0x0401 will be used.
        return AppleProcessorTypeXeonPenrynType2; // 0x0402
      }
      // here stands for models not used by Apple (Merom/Penryn), putting 0x0301 as lowest
      return AppleProcessorTypeCore2DuoType1;   // 0x0301

    //
    // Nehalem:  https://en.wikipedia.org/wiki/Nehalem_(microarchitecture)#Server_and_desktop_processors
    // Westmere: https://en.wikipedia.org/wiki/Westmere_(microarchitecture)#Server_/_Desktop_processors
    //
    // Used by Apple:
    //   Gainestown (Xeon), Bloomfield (Xeon), Lynnfield (i5/i7)                   [Nehalem]
    //   Gulftown (aka Westmere-EP, Xeon), Clarkdale (i3/i5), Arrandale (i5/i7)    [Westmere]
    //
    // Not used by Apple:
    //   Beckton (Xeon), Jasper Forest (Xeon), Clarksfield (i7), Pentium, Celeron [Nehalem]
    //   Westmere-EX (Xeon E7), Pentium, Celeron                                  [Westmere]
    //
    case CPU_MODEL_NEHALEM:     // 0x1A
    case CPU_MODEL_NEHALEM_EX:  // 0x2E, not used by Apple
    case CPU_MODEL_FIELDS:      // 0x1E, Lynnfield, Clarksfield (part of Nehalem)
    case CPU_MODEL_WESTMERE:    // 0x2C
    case CPU_MODEL_WESTMERE_EX: // 0x2F, not used by Apple
    case CPU_MODEL_DALES_32NM:  // 0x25, Clarkdale, Arrandale (part of Westmere)
      if (AppleMajorType == AppleProcessorMajorXeonNehalem) {
        // MP41 & Xserve31 (2x E5520, CPU_MODEL_NEHALEM), MP51 (2x X5670, CPU_MODEL_WESTMERE)
        return AppleProcessorTypeXeon;        // 0x0501
      }
      if (AppleMajorType == AppleProcessorMajorI3) {
        // IM112 (i3-540, 0x0901, CPU_MODEL_DALES_32NM)
        return AppleProcessorTypeCorei3Type1; // 0x0901
      }
      if (AppleMajorType == AppleProcessorMajorI5) {
        // FIXME: no idea what it is on IM112 (i5-680)
        // MBP61, i5-640M, 0x0602, CPU_MODEL_DALES_32NM
        return AppleProcessorTypeCorei5Type2; // 0x0602
      }
      if (AppleMajorType == AppleProcessorMajorI7) {
        // FIXME: used by Apple, no idea what to use, assuming 0x0702 for now (based off 0x0602 on i5)
        return AppleProcessorTypeCorei7Type2; // 0x0702
      }
      // here stands for Pentium and Celeron (Nehalem/Westmere), not used by Apple at all.
      // putting 0x0901 (i3) as lowest
      return AppleProcessorTypeCorei3Type1; // 0x0901

    //
    // Sandy Bridge:   https://en.wikipedia.org/wiki/Sandy_Bridge#List_of_Sandy_Bridge_processors
    // Sandy Bridge-E: https://en.wikipedia.org/wiki/Sandy_Bridge-E#Overview
    //
    // Used by Apple:
    //   Core i5/i7 / i3 (see NOTE below)
    //
    // NOTE: There seems to be one more i3-2100 used on IM121 (EDU),
    //       assuming it exists for now.
    //
    // Not used by Apple:
    //   Xeon v1 (E5/E3),
    //   SNB-E based Core i7 (and Extreme): 3970X, 3960X, 3930K, 3820,
    //   Pentium, Celeron
    //
    case CPU_MODEL_SANDYBRIDGE: // 0x2A
    case CPU_MODEL_JAKETOWN:    // 0x2D, SNB-E, not used by Apple
      if (AppleMajorType == AppleProcessorMajorI3) {
        // FIXME: used by Apple on iMac12,1 (EDU, i3-2100), not confirmed yet
        return AppleProcessorTypeCorei3Type3;   // 0x0903
      }
      if (AppleMajorType == AppleProcessorMajorI5) {
        // NOTE: two values are used here. (0x0602 and 0x0603)
        // TODO: how to classify them. (by changing "if (0)")
        if (0) {
          // MM51 (i5-2415M), MM52 (i5-2520M), MBA41 (i5-2467M), MBA42 (i5-2557M)
          return AppleProcessorTypeCorei5Type2; // 0x0602
        }
        // IM121 (i5-2400S), MBP81 (i5-2415M)
        return AppleProcessorTypeCorei5Type3; // 0x0603
      }
      if (AppleMajorType == AppleProcessorMajorI7) {
        // IM122 (i7-2600), MBP82 (i7-2675QM), MBP83 (i7-2820QM)
        //
        // FIXME: will those i7 not used by Apple (see above), be identified as AppleProcessorMajorI7?
        return AppleProcessorTypeCorei7Type3;   // 0x0703
      }
      if (AppleMajorType == AppleProcessorMajorXeonE5) { // see TODO below
        // for Sandy Xeon E5, not used by Apple
        // FIXME: is AppleProcessorMajorXeonE5, which seems to be for IVY-E only, compatible with SNB-E too?
        // TODO: write some decent code to check SNB-E based Xeon E5.
        return AppleProcessorTypeXeonE5;        // 0x0A01
      }
      if (AppleMajorType == AppleProcessorMajorXeonNehalem) { // see TODO below
        // for Sandy Xeon E3
        // NOTE: Xeon E3 is not used by Apple at all and should be somehow treated as i7,
        //       but here we'd like to show Xeon in "About This Mac".
        // TODO: CPU major type check for SNB based Xeon E3
        return AppleProcessorTypeXeon;          // 0x0501
      }
      // here stands for Pentium and Celeron (Sandy), not used by Apple at all.
      // putting 0x0903 (i3) as lowest
      return AppleProcessorTypeCorei3Type3;   // 0x0903

    //
    // Ivy Bridge:   https://en.wikipedia.org/wiki/Ivy_Bridge_(microarchitecture)#List_of_Ivy_Bridge_processors
    // Ivy Bridge-E: https://en.wikipedia.org/wiki/Ivy_Bridge_(microarchitecture)#Models_and_steppings_2
    //
    // Used by Apple:
    //   Core i5/i7 / i3 (see NOTE below),
    //   Xeon E5 v2
    //
    // NOTE: There seems to be an iMac13,1 (EDU version), or rather iMac13,3, with CPU i3-3225,
    //       assuming it exists for now.
    //
    // Not used by Apple:
    //   Xeon v2 (E7/E3),
    //   IVY-E based Core i7 (and Extreme): 4960X, 4930K, 4820K,
    //   Pentium, Celeron
    //
    case CPU_MODEL_IVYBRIDGE:    // 0x3A
    case CPU_MODEL_IVYBRIDGE_EP: // 0x3E
      if (AppleMajorType == AppleProcessorMajorXeonE5) {
        // MP61 (E5-1620 v2)
        return AppleProcessorTypeXeonE5;      // 0x0A01
      }
      if (AppleMajorType == AppleProcessorMajorI5) {
        // IM131 (i5-3470S), IM132  (i5-3470S),
        // MBP92 (i5-3210M), MBP102 (i5-3210M)
        // MBA51 (i6-3317U), MBA52  (i5-3427U)
        return AppleProcessorTypeCorei5Type4; // 0x0604
      }
      if (AppleMajorType == AppleProcessorMajorI7) {
        // MM62  (i7-3615QM),
        // MBP91 (i7-3615QM), MBP101 (i7-3820QM)
        //
        // FIXME: will those i7 not used by Apple (see above), be identified as AppleProcessorMajorI7?
        return AppleProcessorTypeCorei7Type4; // 0x0704
      }
      if (AppleMajorType == AppleProcessorMajorI3) {
        // FIXME: used by Apple (if iMac13,3 were existent, i3-3225), not confirmed yet
        // assuming it exists for now
        return AppleProcessorTypeCorei3Type4; // 0x0904
      }
      if (AppleMajorType == AppleProcessorMajorXeonNehalem) { // see TODO below
        // for Ivy/Ivy-E E3/E7, not used by Apple
        // NOTE: Xeon E3/E7 is not used by Apple at all and should be somehow treated as i7,
        //       but here we'd like to show Xeon in "About This Mac".
        // TODO: CPU major type check for IVY based Xeon E3/E7
        return AppleProcessorTypeXeon;        // 0x0501
      }
      // here stands for Pentium and Celeron (Ivy), not used by Apple at all.
      // putting 0x0904 (i3) as lowest.
      return AppleProcessorTypeCorei3Type4; // 0x0904

    //
    // Haswell:   https://en.wikipedia.org/wiki/Haswell_(microarchitecture)#List_of_Haswell_processors
    // Haswell-E: basically the same page.
    //
    // Used by Apple:
    //   Core i5/i7
    //
    // Not used by Apple:
    //   Xeon v3 (E7/E5/E3),
    //   Core i3,
    //   Haswell-E based Core i7 Extreme: 5960X, 5930K, 5820K,
    //   Pentium, Celeron
    //
    case CPU_MODEL_HASWELL:     // 0x3C
    case CPU_MODEL_HASWELL_EP:  // 0x3F
    case CPU_MODEL_HASWELL_ULT: // 0x45
      if (AppleMajorType == AppleProcessorMajorI5) {
        // IM141 (i5-4570R), IM142 (i5-4670), IM151 (i5-4690),
        // MM71  (i5-4260U),
        // MBA62 (i5-4250U)
        return AppleProcessorTypeCorei5Type5; // 0x0605
      }
      if (AppleMajorType == AppleProcessorMajorI7) {
        // MBP112 (i7-4770HQ), MBP113 (i7-4850HQ)
        //
        // FIXME: will those i7 not used by Apple (see above), be identified as AppleProcessorMajorI7?
        return AppleProcessorTypeCorei7Type5; // 0x0705
      }
      if (AppleMajorType ==  AppleProcessorMajorI3) {
        // for i3, not used by Apple, just for showing i3 in "About This Mac".
        return AppleProcessorTypeCorei3Type5; // 0x0905
      }
      if (AppleMajorType == AppleProcessorMajorXeonE5) { // see TODO below
        // for Haswell-E Xeon E5, not used by Apple
        // FIXME: is AppleProcessorMajorXeonE5, which seems to be for IVY-E only, compatible with Haswell-E too?
        // TODO: write some decent code to check Haswell-E based Xeon E5.
        return AppleProcessorTypeXeonE5;      // 0x0A01
      }
      if (AppleMajorType == AppleProcessorMajorXeonNehalem) { // see TODO below
        // for Haswell/Haswell-E E3/E7, not used by Apple
        // NOTE: Xeon E3/E7 is not used by Apple at all and should be somehow treated as i7,
        //       but here we'd like to show Xeon in "About This Mac".
        // TODO: CPU major type check for Haswell/Haswell-E based Xeon E3/E7
        return AppleProcessorTypeXeon;        // 0x0501
      }
      // here stands for Pentium and Celeron (Haswell), not used by Apple at all.
      // putting 0x0905 (i3) as lowest.
      return AppleProcessorTypeCorei3Type5; // 0x0905

    //
    // Broadwell:   https://en.wikipedia.org/wiki/Broadwell_(microarchitecture)#List_of_Broadwell_processors
    // Broadwell-E: https://en.wikipedia.org/wiki/Broadwell_(microarchitecture)#"Broadwell-E"_HEDT_(14_nm)
    //
    // NOTE: support table for BDW-E is missing in XNU, thus a CPUID patch might be needed. (See Clover FakeCPUID)
    //
    // Used by Apple:
    //   Core i5/i7, Core M
    //
    // Not used by Apple:
    //   Broadwell-E: i7 6950X/6900K/6850K/6800K,
    //   Xeon v4 (E5/E3),
    //   Core i3,
    //   Pentium, Celeron
    //
    case CPU_MODEL_BROADWELL:     // 0x3D
    case CPU_MODEL_CRYSTALWELL:   // 0x46
    case CPU_MODEL_BRYSTALWELL:   // 0x47
      if (AppleMajorType == AppleProcessorMajorM) {
        // MB81 (M 5Y51)
        return AppleProcessorTypeCoreMType6;   // 0x0B06
      }
      if (AppleMajorType == AppleProcessorMajorI5) {
        // IM161  (i5-5250U), IM162 (i5-5675R),
        // MBP121 (i5-5257U),
        // MBA71  (i5-5250U), MBA72 (unknown)
        return AppleProcessorTypeCorei5Type6; // 0x0606
      }
      if (AppleMajorType == AppleProcessorMajorI7) {
        // FIXME: 0x0706 is just an ideal value for i7, waiting for confirmation
        // FIXME: will those i7 not used by Apple (see above), be identified as AppleProcessorMajorI7?
        return AppleProcessorTypeCorei7Type6; // 0x0706
      }
      if (AppleMajorType == AppleProcessorMajorI3) {
        // for i3, not used by Apple, just for showing i3 in "About This Mac".
        // FIXME: 0x0906 is just an ideal value for i3, waiting for confirmation
        return AppleProcessorTypeCorei3Type6; // 0x0906
      }
      if (AppleMajorType == AppleProcessorMajorXeonE5) { // see TODO below
        // for Broadwell-E Xeon E5, not used by Apple
        // FIXME: is AppleProcessorMajorXeonE5, which seems to be for IVY-E only, compatible with Broadwell-E too?
        // TODO: write some decent code to check Broadwell-E based Xeon E5.
        return AppleProcessorTypeXeonE5;      // 0x0A01
      }
      if (AppleMajorType == AppleProcessorMajorXeonNehalem) { // see TODO below
        // for Broadwell E3, not used by Apple
        // NOTE: Xeon E3 is not used by Apple at all and should be somehow treated as i7,
        //       but here we'd like to show Xeon in "About This Mac".
        // TODO: CPU major type check for Broadwell based Xeon E3
        return AppleProcessorTypeXeon;        // 0x0501
      }
      // here stands for Pentium and Celeron (Broadwell), not used by Apple at all.
      // putting 0x0906 (i3) as lowest.
      return AppleProcessorTypeCorei3Type5; // 0x0906

    //
    // Skylake: https://en.wikipedia.org/wiki/Skylake_(microarchitecture)#List_of_Skylake_processor_models
    //
    // Used by Apple:
    //   Xeon W, Core m3, m5, m7, i5, i7
    //
    // Not used by Apple:
    //   Core i3,
    //   all high-end models (Core i9, i7 Extreme): see https://en.wikipedia.org/wiki/Skylake_(microarchitecture)#High-end_desktop_processors
    //   Xeon E3 v5,
    //   Pentium, Celeron
    //
    case CPU_MODEL_SKYLAKE:     // 0x4E
    case CPU_MODEL_SKYLAKE_DT:  // 0x5E
    case CPU_MODEL_SKYLAKE_W:   // 0x55, also SKL-X
      if (AppleMajorType == AppleProcessorMajorXeonW) {
        // IMP11 (Xeon W 2140B)
        return AppleProcessorTypeXeonW;       // 0x0F01
      }
      if (AppleMajorType == AppleProcessorMajorM3) {
        // FIXME: we dont have any m3 (Skylake) dump!
        // using an ideal value (0x0C07), which is used on MB101 (m3-7Y32)
        return AppleProcessorTypeCoreM3Type7; // 0x0C07
      }
      if (AppleMajorType == AppleProcessorMajorM5) {
        // MB91 (m5 6Y54)
        return AppleProcessorTypeCoreM5Type7; // 0x0D07
      }
      if (AppleMajorType == AppleProcessorMajorM7) {
        // FIXME: we dont have any m7 (Skylake) dump!
        // using an ideal value (0x0E07)
        return AppleProcessorTypeCoreM7Type7; // 0x0E07
      }
      if (AppleMajorType == AppleProcessorMajorI5) {
        return AppleProcessorTypeCorei5Type5; // 0x0605
      }
      if (AppleMajorType == AppleProcessorMajorI7) {
        // FIXME: used by Apple, but not sure what to use...
        // 0x0707 is used on MBP133 (i7-6700HQ),
        // 0x0705 is not confirmed, just an ideal one comparing to 0x0605 (AppleProcessorTypeCorei5Type5)
        // using 0x0705 for now
        return AppleProcessorTypeCorei7Type5; // 0x0705
      }
      if (AppleMajorType == AppleProcessorMajorI3) {
        // for i3, not used by Apple, just for showing i3 in "About This Mac".
        return AppleProcessorTypeCorei3Type5; // 0x0905
      }
      if (AppleMajorType == AppleProcessorMajorI9) {
        // for i9 (SKL-X), not used by Apple, just for showing i9 in "About This Mac".
        // FIXME: i9 was not introdced in this era but later (MBP151, Coffee Lake),
        //        will AppleProcessorMajorI9 work here?
        // NOTE: using a mostly invalid value 0x1005 for now...
        return AppleProcessorTypeCorei9Type5; // 0x1005
      }
      if (AppleMajorType == AppleProcessorMajorXeonNehalem) { // see TODO below
        // for Skylake E3 (there's no E5/E7 on Skylake), not used by Apple
        // NOTE: Xeon E3 is not used by Apple at all and should be somehow treated as i7,
        //       but here we'd like to show Xeon in "About This Mac".
        // TODO: CPU major type check for Skylake based Xeon E3
        return AppleProcessorTypeXeon;        // 0x0501
      }
      // here stands for Pentium and Celeron (Skylake), not used by Apple at all.
      // putting 0x0905 (i3) as lowest.
      return AppleProcessorTypeCorei3Type5; // 0x0905

    //
    // Kaby Lake:   https://en.wikipedia.org/wiki/Kaby_Lake#List_of_7th_generation_Kaby_Lake_processors
    // Coffee Lake: https://en.wikipedia.org/wiki/Coffee_Lake#List_of_8th_generation_Coffee_Lake_processors
    //
    // Used by Apple:
    //   Core m3    [Kaby],
    //   Core i5/i7 [Kaby/Coffee],
    //   Core i9    [Coffee],
    //
    // Not used by Apple:
    //   Core i3    [Kaby/Coffee],
    //   Xeon E3 v6 [Kaby],
    //   Xeon E     [Coffee],
    //   Pentium, Celeron
    //
    case CPU_MODEL_KABYLAKE:       // 0x8E
    case CPU_MODEL_COFFEELAKE:     // 0x9E
      if (AppleMajorType == AppleProcessorMajorM3) {
        // MB101 (m3 7Y32)
        return AppleProcessorTypeCoreM3Type7; // 0x0C07
      }
      if (AppleMajorType == AppleProcessorMajorI5) {
        // Kaby has 0x9 stepping, and Coffee use 0xA / 0xB stepping.
        if (Stepping == 9) {
          // IM181 (i5-7360U), IM182  (i5-7400), IM183 (i5-7600), IM191 (i5-8600) [NOTE 1]
          // MBP141 (i5-7360U), MBP142 (i5-7267U)
          //
          // NOTE 1: IM191 is Coffee and thus 0x0609 will be used, TODO.
          return AppleProcessorTypeCorei5Type5; // 0x0605
        }
        // MM81 (i5-8500B)
        // MBP152 (i5-8259U)
        return AppleProcessorTypeCorei5Type9; // 0x0609
      }
      if (AppleMajorType == AppleProcessorMajorI7) {
        // Kaby has 0x9 stepping, and Coffee use 0xA / 0xB stepping.
        if (Stepping == 9) {
          // FIXME: used by Apple, but not sure what to use...
          // 0x0709 is used on MBP151 (i7-8850H),
          // 0x0705 is not confirmed, just an ideal one comparing to 0x0605 (AppleProcessorTypeCorei5Type5)
          // using 0x0705 for now
          return AppleProcessorTypeCorei7Type5; // 0x0705
        }
        // MM81 (i7-8700B)
        return AppleProcessorTypeCorei7Type9; // 0x0709
      }
      if (AppleMajorType == AppleProcessorMajorI9) {
        // FIXME: find a dump from MBP151 with i9-8950HK,
        // for now using an ideal value (0x1009), comparing to 0x0709 (used on MBP151, i7-8850H and MM81, i7-8700B)
        return AppleProcessorTypeCorei9Type9; // 0x1009
      }
      if (AppleMajorType == AppleProcessorMajorI3) {
        // FIXME: find a dump from MM71 with i3...
        // for now using an idea value (0x0905)
        return AppleProcessorTypeCorei3Type5; // 0x0905
      }
      if (AppleMajorType == AppleProcessorMajorXeonNehalem) { // see TODO below
        // for Kaby Lake/Coffee Lake E3 (there's no E5/E7 on either), not used by Apple
        // NOTE: Xeon E3 is not used by Apple at all and should be somehow treated as i7,
        //       but here we'd like to show Xeon in "About This Mac".
        // TODO: CPU major type check for KBL/CFL based Xeon E3
        return AppleProcessorTypeXeon;        // 0x0501
      }
      // here stands for Pentium and Celeron (KBL/CFL), not used by Apple at all.
      // putting 0x0905 (i3) as lowest.
      return AppleProcessorTypeCorei3Type5; // 0x0905

    default:
      // NOTE: by default it is really unknown, but we fallback
      return AppleProcessorTypeCorei5Type5; // 0x0605
  }
}

VOID
ScanIntelProcessor (
  IN OUT OC_CPU_INFO  *Cpu
  )
{
  UINT32                                            CpuidEax;
  UINT32                                            CpuidEbx;
  UINT64                                            Msr;
  CPUID_CACHE_PARAMS_EAX                            CpuidCacheEax;
  CPUID_CACHE_PARAMS_EBX                            CpuidCacheEbx;
  UINT8                                             AppleMajorType;
  MSR_SANDY_BRIDGE_PKG_CST_CONFIG_CONTROL_REGISTER  PkgCstConfigControl;
  MSR_IA32_PERF_STATUS_REGISTER                     PerfStatus;
  MSR_NEHALEM_PLATFORM_INFO_REGISTER                PlatformInfo;
  MSR_NEHALEM_TURBO_RATIO_LIMIT_REGISTER            TurboLimit;
  UINT16                                            CoreCount;

  AppleMajorType = DetectAppleMajorType (Cpu->BrandString);
  Cpu->AppleProcessorType = DetectAppleProcessorType (Cpu->Model, Cpu->Stepping, AppleMajorType);

  DEBUG ((DEBUG_INFO, "Detected Apple Processor Type: %02X -> %04X\n", AppleMajorType, Cpu->AppleProcessorType));

  if ((Cpu->Family != 0x06 || Cpu->Model < 0x0c)
    && (Cpu->Family != 0x0f || Cpu->Model < 0x03)) {
    return;
  }

  if (Cpu->Model >= CPU_MODEL_SANDYBRIDGE) {
    PkgCstConfigControl.Uint64 = AsmReadMsr64 (MSR_SANDY_BRIDGE_PKG_CST_CONFIG_CONTROL);
    Cpu->CstConfigLock = PkgCstConfigControl.Bits.CFGLock == 1;
  }

  //
  // TODO: this may not be accurate on some older processors.
  //
  if (Cpu->Model >= CPU_MODEL_NEHALEM) {
    PerfStatus.Uint64 = AsmReadMsr64 (MSR_IA32_PERF_STATUS);
    Cpu->CurBusRatio = (UINT8) (PerfStatus.Bits.State >> 8U);
    PlatformInfo.Uint64 = AsmReadMsr64 (MSR_NEHALEM_PLATFORM_INFO);
    Cpu->MinBusRatio = (UINT8) PlatformInfo.Bits.MaximumEfficiencyRatio;
    Cpu->MaxBusRatio = (UINT8) PlatformInfo.Bits.MaximumNonTurboRatio;
  } else if (Cpu->Model >= CPU_MODEL_PENRYN) {
    PerfStatus.Uint64 = AsmReadMsr64 (MSR_IA32_PERF_STATUS);
    Cpu->MaxBusRatio = (UINT8) (PerfStatus.Uint64 >> 8U) & 0x1FU;
    //
    // Undocumented values:
    // Non-integer bus ratio for the max-multi.
    // Non-integer bus ratio for the current-multi.
    //
    // MaxBusRatioDiv = (UINT8)(PerfStatus.Uint64 >> 46U) & 0x01U;
    // CurrDiv = (UINT8)(PerfStatus.Uint64 >> 14U) & 0x01U;
    //
  }

  if (Cpu->Model >= CPU_MODEL_NEHALEM
    && Cpu->Model != CPU_MODEL_NEHALEM_EX
    && Cpu->Model != CPU_MODEL_WESTMERE_EX) {
    TurboLimit.Uint64 = AsmReadMsr64 (MSR_NEHALEM_TURBO_RATIO_LIMIT);
    Cpu->TurboBusRatio1 = (UINT8) TurboLimit.Bits.Maximum1C;
    Cpu->TurboBusRatio2 = (UINT8) TurboLimit.Bits.Maximum2C;
    Cpu->TurboBusRatio3 = (UINT8) TurboLimit.Bits.Maximum3C;
    Cpu->TurboBusRatio4 = (UINT8) TurboLimit.Bits.Maximum4C;
  }

  DEBUG ((
    DEBUG_INFO,
    "Ratio Min %d Max %d Current %d Turbo %d %d %d %d\n",
    Cpu->MinBusRatio,
    Cpu->MaxBusRatio,
    Cpu->CurBusRatio,
    Cpu->TurboBusRatio1,
    Cpu->TurboBusRatio2,
    Cpu->TurboBusRatio3,
    Cpu->TurboBusRatio4
    ));

  //
  // SkyLake and later have an Always Running Timer
  //
  if (Cpu->Model >= CPU_MODEL_SKYLAKE) {
    AsmCpuid (CPUID_TIME_STAMP_COUNTER, &CpuidEax, &CpuidEbx, NULL, NULL);

    if (CpuidEax > 0 && CpuidEbx > 0) {
      Cpu->CPUFrequency = MultU64x32 (BASE_ART_CLOCK_SOURCE, (UINT32) DivU64x32 (CpuidEbx, CpuidEax));

      DEBUG ((
        DEBUG_INFO,
        "%a %a %11lld %5dMHz %u * %u / %u = %ld\n",
        "ART",
        "Frequency",
        Cpu->CPUFrequency,
        DivU64x32 (Cpu->CPUFrequency, 1000000),
        BASE_ART_CLOCK_SOURCE,
        CpuidEbx,
        CpuidEax,
        Cpu->CPUFrequency
        ));

      Cpu->FSBFrequency = DivU64x32 (Cpu->CPUFrequency, Cpu->MaxBusRatio);
    }
  }

  //
  // Calculate the Tsc frequency
  //
  Cpu->TSCFrequency = GetPerformanceCounterProperties (NULL, NULL);

  if (Cpu->CPUFrequency == 0) {
    //
    // There may be some quirks with virtual CPUs (VMware is fine).
    // Formerly we checked Cpu->MinBusRatio > 0, but we have no MinBusRatio on Penryn.
    //
    if (Cpu->TSCFrequency > 0 && Cpu->MaxBusRatio > Cpu->MinBusRatio) {
      Cpu->FSBFrequency = DivU64x32 (Cpu->TSCFrequency, Cpu->MaxBusRatio);
      Cpu->CPUFrequency = MultU64x32 (Cpu->FSBFrequency, Cpu->MaxBusRatio);
    } else {
      Cpu->CPUFrequency = Cpu->TSCFrequency;
      Cpu->FSBFrequency = 100000000;
    }
  }
  //
  // Calculate number of cores
  //
  if (Cpu->MaxExtId >= CPUID_CACHE_PARAMS && Cpu->Model <= CPU_MODEL_PENRYN) {
    AsmCpuidEx (CPUID_CACHE_PARAMS, 0, &CpuidCacheEax.Uint32, &CpuidCacheEbx.Uint32, NULL, NULL);
    if (CpuidCacheEax.Bits.CacheType != CPUID_CACHE_PARAMS_CACHE_TYPE_NULL) {
      CoreCount = (UINT16)GetPowerOfTwo32 (CpuidCacheEax.Bits.MaximumAddressableIdsForProcessorCores + 1);
      if (CoreCount < CpuidCacheEax.Bits.MaximumAddressableIdsForProcessorCores + 1) {
        CoreCount *= 2;
      }
      Cpu->CoreCount   = CoreCount;
      //
      // We should not be blindly relying on Cpu->Features & CPUID_FEATURE_HTT.
      // On Penryn CPUs it is set even without Hyper Threading.
      //
      if (Cpu->ThreadCount < Cpu->CoreCount) {
        Cpu->ThreadCount = Cpu->CoreCount;
      }
    }
  } else if (Cpu->Model == CPU_MODEL_WESTMERE) {
    Msr = AsmReadMsr64 (MSR_CORE_THREAD_COUNT);
    Cpu->CoreCount   = (UINT16)BitFieldRead64 (Msr, 16, 19);
    Cpu->ThreadCount = (UINT16)BitFieldRead64 (Msr, 0,  15);
  } else {
    Msr = AsmReadMsr64 (MSR_CORE_THREAD_COUNT);
    Cpu->CoreCount   = (UINT16)BitFieldRead64 (Msr, 16, 31);
    Cpu->ThreadCount = (UINT16)BitFieldRead64 (Msr, 0,  15);
  }

  if (Cpu->CoreCount == 0) {
    Cpu->CoreCount = 1;
  }

  if (Cpu->ThreadCount == 0) {
    Cpu->ThreadCount = 1;
  }

  //
  // TODO: handle package count...
  //
}

VOID
ScanAmdProcessor (
  IN OUT OC_CPU_INFO  *Cpu
  )
{
  UINT32 CpuidEbx;
  UINT32 CpuidEcx;
  UINT64 CofVid;
  UINT64 CoreFrequencyID;
  UINT64 CoreDivisorID;
  UINT64 Divisor;
  //
  // Faking an Intel Core i5 Processor.
  // This value is purely cosmetic, but it makes sense to fake something
  // that is somewhat representative of the kind of Processor that's actually
  // in the system
  //
  Cpu->AppleProcessorType = AppleProcessorTypeCorei5Type5;
  //
  // get TSC Frequency calculated in OcTimerLib
  //
  Cpu->TSCFrequency = GetPerformanceCounterProperties (NULL, NULL);
  Cpu->CPUFrequency = Cpu->TSCFrequency;
  //
  // Get core and thread count from CPUID
  //
  if (Cpu->MaxExtId >= 0x80000008) {
    AsmCpuid (0x80000008, NULL, NULL, &CpuidEcx, NULL);
    Cpu->ThreadCount = (UINT16) (BitFieldRead32 (CpuidEcx, 0, 7) + 1);
  }
  //
  // CPUPM is not supported on AMD, meaning the current
  // and minimum bus ratio are equal to the maximum bus ratio
  //
  Cpu->CurBusRatio = Cpu->MaxBusRatio;
  Cpu->MinBusRatio = Cpu->MaxBusRatio;

  if (Cpu->Family == 0x0F) {
    switch (Cpu->ExtFamily) {
      case 0x08:
      {
        CofVid           = AsmReadMsr64 (K10_PSTATE_STATUS);
        CoreFrequencyID  = BitFieldRead64 (CofVid, 0, 7);
        CoreDivisorID    = BitFieldRead64 (CofVid, 8, 13);
        Cpu->MaxBusRatio = (UINT8) (CoreFrequencyID / CoreDivisorID * 2);
        //
        // Get core count from CPUID
        //
        if (Cpu->MaxExtId >= 0x8000001E) {
          AsmCpuid (0x8000001E, NULL, &CpuidEbx, NULL, NULL);
          Cpu->CoreCount =
            (UINT16) DivU64x32 (
              Cpu->ThreadCount,
              (BitFieldRead32 (CpuidEbx, 8, 15) + 1)
            );
        }
        break;
      }
      case 0x06:
      case 0x07:
      {
        CofVid           = AsmReadMsr64 (K10_COFVID_STATUS);
        CoreFrequencyID  = BitFieldRead64 (CofVid, 0, 5);
        CoreDivisorID    = CofVid & BIT6;
        Divisor          = 1U << CoreDivisorID;
        Cpu->MaxBusRatio = (CoreFrequencyID + 16) / Divisor + 1;
        //
        // AMD 15h and 16h CPUs don't support hyperthreading,
        // so the core count is equal to the thread count
        Cpu->CoreCount = Cpu->ThreadCount;
        break;
      }
      default:
      {
        break;
      }
    }
    Cpu->FSBFrequency = DivU64x32 (Cpu->TSCFrequency, Cpu->MaxBusRatio);
  }
}

/** Scan the processor and fill the cpu info structure with results

  @param[in] Cpu  A pointer to the cpu info structure to fill with results

  @retval EFI_SUCCESS  The scan was completed successfully.
**/
VOID
OcCpuScanProcessor (
  IN OUT OC_CPU_INFO  *Cpu
  )
{
  UINT32                  CpuidEax;
  UINT32                  CpuidEbx;
  UINT32                  CpuidEcx;
  UINT32                  CpuidEdx;
  CPUID_VERSION_INFO_EAX  CpuidVerEax;
  CPUID_VERSION_INFO_EBX  CpuidVerEbx;
  CPUID_VERSION_INFO_ECX  CpuidVerEcx;
  CPUID_VERSION_INFO_EDX  CpuidVerEdx;

  ASSERT (Cpu != NULL);

  ZeroMem (Cpu, sizeof (*Cpu));

  //
  // Get vendor CPUID 0x00000000
  //
  AsmCpuid (CPUID_SIGNATURE, &CpuidEax, &Cpu->Vendor[0], &Cpu->Vendor[2], &Cpu->Vendor[1]);

  //
  // Get extended CPUID 0x80000000
  //
  AsmCpuid (CPUID_EXTENDED_FUNCTION, &CpuidEax, &CpuidEbx, &CpuidEcx, &CpuidEdx);

  Cpu->MaxExtId = CpuidEax;

  //
  // Get brand string CPUID 0x80000002 - 0x80000004
  //
  if (Cpu->MaxExtId >= CPUID_BRAND_STRING3) {
    //
    // The brandstring 48 bytes max, guaranteed NULL terminated.
    //
    UINT32  *BrandString = (UINT32 *) Cpu->BrandString;

    AsmCpuid (
      CPUID_BRAND_STRING1,
      BrandString,
      (BrandString + 1),
      (BrandString + 2),
      (BrandString + 3)
      );

    AsmCpuid (
      CPUID_BRAND_STRING2,
      (BrandString + 4),
      (BrandString + 5),
      (BrandString + 6),
      (BrandString + 7)
      );

    AsmCpuid (
      CPUID_BRAND_STRING3,
      (BrandString + 8),
      (BrandString + 9),
      (BrandString + 10),
      (BrandString + 11)
      );
  }

  Cpu->PackageCount = 1;
  Cpu->CoreCount    = 1;
  Cpu->ThreadCount  = 1;

  //
  // Get processor signature and decode
  //
  if (Cpu->MaxExtId >= CPUID_VERSION_INFO) {
    AsmCpuid (CPUID_VERSION_INFO, &CpuidVerEax.Uint32, &CpuidVerEbx.Uint32, &CpuidVerEcx.Uint32, &CpuidVerEdx.Uint32);

    Cpu->Signature = (UINT8) CpuidVerEax.Uint32;
    Cpu->Stepping  = (UINT8) CpuidVerEax.Bits.SteppingId;
    Cpu->ExtModel  = (UINT8) CpuidVerEax.Bits.ExtendedModelId;
    Cpu->Model     = (UINT8) CpuidVerEax.Bits.Model | (UINT8) (CpuidVerEax.Bits.ExtendedModelId << 4U);
    Cpu->Family    = (UINT8) CpuidVerEax.Bits.FamilyId;
    Cpu->Type      = (UINT8) CpuidVerEax.Bits.ProcessorType;
    Cpu->ExtFamily = (UINT8) CpuidVerEax.Bits.ExtendedFamilyId;
    Cpu->Brand     = (UINT8) CpuidVerEbx.Bits.BrandIndex;
    Cpu->Features  = (((UINT64) CpuidVerEcx.Uint32) << 32ULL) | CpuidVerEdx.Uint32;
    if (Cpu->Features & CPUID_FEATURE_HTT) {
      Cpu->ThreadCount = (UINT16) CpuidVerEbx.Bits.MaximumAddressableIdsForLogicalProcessors;
    }
  }

  DEBUG ((DEBUG_INFO, "%a %a\n", "Found", Cpu->BrandString));

  DEBUG ((
    DEBUG_INFO,
    "Signature %0X Stepping %0X Model %0X Family %0X Type %0X ExtModel %0X ExtFamily %0X\n",
    Cpu->Signature,
    Cpu->Stepping,
    Cpu->Model,
    Cpu->Family,
    Cpu->Type,
    Cpu->ExtModel,
    Cpu->ExtFamily
    ));

  if (Cpu->Vendor[0] == CPUID_VENDOR_INTEL) {
    ScanIntelProcessor (Cpu);
  } else if (Cpu->Vendor[0] == CPUID_VENDOR_AMD) {
    ScanAmdProcessor (Cpu);
  } else {
    DEBUG ((DEBUG_WARN, "Found unsupported CPU vendor: %0X", Cpu->Vendor[0]));
    return;
  }

  DEBUG ((
    DEBUG_INFO,
    "%a %a %11lld %5dMHz\n",
    "TSC",
    "Frequency",
    Cpu->TSCFrequency,
    DivU64x32 (Cpu->TSCFrequency, 1000000)
    ));

  DEBUG ((
    DEBUG_INFO,
    "%a %a %11lld %5dMHz\n",
    "CPU",
    "Frequency",
    Cpu->CPUFrequency,
    DivU64x32 (Cpu->CPUFrequency, 1000000)
    ));

  DEBUG ((
    DEBUG_INFO,
    "%a %a %11lld %5dMHz\n",
    "FSB",
    "Frequency",
    Cpu->FSBFrequency,
    DivU64x32 (Cpu->FSBFrequency, 1000000)
    ));

  DEBUG ((
    DEBUG_INFO,
    "Pkg %u Cores %u Threads %u\n",
    Cpu->PackageCount,
    Cpu->CoreCount,
    Cpu->ThreadCount
    ));
}

VOID
OcCpuCorrectFlexRatio (
  IN OC_CPU_INFO  *Cpu
  )
{
  UINT64  Msr;
  UINT64  FlexRatio;

  if (Cpu->Vendor[0] == CPUID_VENDOR_INTEL
    && Cpu->Model != CPU_MODEL_GOLDMONT
    && Cpu->Model != CPU_MODEL_AIRMONT
    && Cpu->Model != CPU_MODEL_AVOTON) {
    Msr = AsmReadMsr64 (MSR_FLEX_RATIO);
    if (Msr & FLEX_RATIO_EN) {
      FlexRatio = BitFieldRead64 (Msr, 8, 15);
      if (FlexRatio == 0) {
        //
        // Disable Flex Ratio if current value is 0.
        //
        AsmWriteMsr64 (MSR_FLEX_RATIO, Msr & ~((UINT64) FLEX_RATIO_EN));
      }
    }
  }
}
