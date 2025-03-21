/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2024, Zero Tang. All rights reserved.

  This file is the HyperVisor Invoker on UEFI Platform.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /xpf_core/uefi/uefihvm.c
*/

#include <Uefi.h>
#include <Guid/Acpi.h>
#include <IndustryStandard/Acpi.h>
#include <IndustryStandard/PeImage.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Register/Intel/Cpuid.h>
#include "uefihvm.h"

void NoirTestCpuid()
{
	CPUID_VERSION_INFO_ECX VerInfoEcx;
	AsmCpuid(CPUID_VERSION_INFO,NULL,NULL,(UINT32*)&VerInfoEcx,NULL);
	if(VerInfoEcx.Bits.ParaVirtualized)
	{
		UINT32 MaximumLeaf;
		CHAR8 VendorString[13];
		AsmCpuid(CPUID_LEAF_HV_VENDOR_ID,&MaximumLeaf,(UINT32*)&VendorString[0],(UINT32*)&VendorString[4],(UINT32*)&VendorString[8]);
		VendorString[12]='\0';
		Print(L"[Test] Hypervisor is detected! Maximum Leaf: 0x%X, Vendor: %a\n",MaximumLeaf,VendorString);
		if(MaximumLeaf>=CPUID_LEAF_HV_VENDOR_NEUTRAL)
		{
			CHAR8 Signature[5];
			AsmCpuid(CPUID_LEAF_HV_VENDOR_NEUTRAL,(UINT32*)Signature,NULL,NULL,NULL);
			Signature[4]='\0';
			Print(L"[Test] Signature: %a\n",Signature);
		}
	}
}

UINT32 NoirBuildHypervisor()
{
	DisableInterrupts();
	UINT32 st=nvc_build_hypervisor();
	EnableInterrupts();
	NoirTestCpuid();
	return st;
}

void NoirTeardownHypervisor()
{
	nvc_teardown_hypervisor();
}

UINT64 noir_query_enabled_features_in_system()
{
	UINT32 Type;
	UINT32 CpuidPresence=1,NestedVirtualization=0,EnableIommu=1;
	UINT64 Features=0;
	NoirGetConfigurationRecord("CpuidPresence",&Type,&CpuidPresence,sizeof(UINT32),NULL);
	NoirGetConfigurationRecord("NestedVirtualization",&Type,&NestedVirtualization,sizeof(UINT32),NULL);
	NoirGetConfigurationRecord("EnableIommu",&Type,&EnableIommu,sizeof(UINT32),NULL);
	Features|=(CpuidPresence!=0)<<NOIR_HVM_FEATURE_CPUID_PRESENCE_BIT;
	Features|=(NestedVirtualization!=0)<<NOIR_HVM_FEATURE_NESTED_VIRTUALIZATION_BIT;
	Features|=(EnableIommu!=0)<<NOIR_HVM_FEATURE_ENABLE_IOMMU_BIT;
	return Features;
}

void nvc_store_image_info(OUT VOID** Base,OUT UINT32* Size)
{
	if(Base)*Base=NvImageBase;
	if(Size)*Size=NvImageSize;
}

void NoirSaveImageInfo(IN EFI_LOADED_IMAGE_PROTOCOL *LoadedImageProtocol)
{
	if(LoadedImageProtocol)
	{
		NvImageBase=LoadedImageProtocol->ImageBase;
		NvImageSize=(UINT32)LoadedImageProtocol->ImageSize;
	}
}

// When system enters runtime stage, the operating system may relocate the pointers
// referenced by NoirVisor, and it may thereby results in unpredictable behaviors.
// Most common errors would be #PF exceptions in Host Code.
void NoirSuppressImageRelocation(IN VOID* ImageBase)
{
	EFI_IMAGE_DOS_HEADER *DosHead=(EFI_IMAGE_DOS_HEADER*)ImageBase;
	if(DosHead->e_magic==EFI_IMAGE_DOS_SIGNATURE)
	{
		EFI_IMAGE_NT_HEADERS *NtHead=(EFI_IMAGE_NT_HEADERS*)((UINTN)ImageBase+DosHead->e_lfanew);
		EFI_IMAGE_DATA_DIRECTORY* RelocTable=(EFI_IMAGE_DATA_DIRECTORY*)((UINTN)ImageBase+NtHead->OptionalHeader.DataDirectory[EFI_IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress);
		// EFI_IMAGE_SECTION_HEADER* SectionTable=(EFI_IMAGE_SECTION_HEADER*)((UINTN)NtHead+sizeof(EFI_IMAGE_NT_HEADERS));
		// Hide the relocation table in data directory.
		RelocTable->VirtualAddress=RelocTable->Size=0;
		// Make the NT Header Signature invalid so the firmware won't relocate pointers referenced by NoirVisor.
		// NtHead->Signature=0;
	}
}

UINT32 NoirQueryVirtualizationSupportability()
{
	return noir_get_virtualization_supportability();
}

BOOLEAN NoirIsVirtualizationEnabled()
{
	return noir_is_virtualization_enabled();
}

EFI_STATUS NoirConfigureInternalDebugger()
{
	UINT32 Type;
	CHAR8 DebugPortType[32];
	EFI_STATUS st=NoirGetConfigurationRecord("DebugPort",&Type,DebugPortType,sizeof(DebugPortType),NULL);
	if(st==EFI_SUCCESS)
	{
		if(AsciiStrnCmp(DebugPortType,"qemu_debugcon",sizeof(DebugPortType))==0)
		{
			UINT32 Port=0x402;
			NoirGetConfigurationRecord("QemuDebugConPortNumber",&Type,&Port,sizeof(UINT32),NULL);
			Print(L"NoirVisor will use QEMU ISA Debug Console at Port 0x%04X\n",Port);
			noir_configure_qemu_debug_console(Port);
			Print(L"Make sure you see a message on your debug console!\n");
			st=EFI_SUCCESS;
		}
		else if(AsciiStrnCmp(DebugPortType,"serial",sizeof(DebugPortType))==0)
		{
			UINT32 BaudRate=115200,PortNumber=2,PortBase=0x2F8;
			NoirGetConfigurationRecord("SerialBaudRate",&Type,&BaudRate,sizeof(UINT32),NULL);
			NoirGetConfigurationRecord("SerialPortNumber",&Type,&PortNumber,sizeof(UINT32),NULL);
			NoirGetConfigurationRecord("SerialPortBase",&Type,&PortBase,sizeof(UINT32),NULL);
			Print(L"NoirVisor will use Serial connection (COM%u) at Port 0x%04X with BaudRate %u Hz!\n",PortNumber,PortBase,BaudRate);
			noir_configure_serial_port_debugger(PortNumber-1,PortBase,BaudRate);
			st=EFI_SUCCESS;
		}
		else
		{
			Print(L"Warning: Internal Debugger is disabled because of unknown DebugPort medium (%a)!\n",DebugPortType);
			st=EFI_NO_MEDIA;
		}
	}
	return st;
}

VOID* noir_locate_acpi_rsdt(OUT UINTN *Length)
{
	EFI_ACPI_DESCRIPTION_HEADER *Rsdt=NULL,*Xsdt=NULL;
	for(UINTN i=0;i<gST->NumberOfTableEntries;i++)
	{
		EFI_CONFIGURATION_TABLE *CurrentEntry=(EFI_CONFIGURATION_TABLE*)&gST->ConfigurationTable[i];
		if(CompareGuid(&CurrentEntry->VendorGuid,&gEfiAcpi20TableGuid))
		{
			EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER* Rsdp=(EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER*)CurrentEntry->VendorTable;
			if(Rsdp->Signature==EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER_SIGNATURE)
			{
				Rsdt=(EFI_ACPI_DESCRIPTION_HEADER*)((UINTN)Rsdp->RsdtAddress);
				Xsdt=(EFI_ACPI_DESCRIPTION_HEADER*)((UINTN)Rsdp->XsdtAddress);
			}
		}
		else if(CompareGuid(&CurrentEntry->VendorGuid,&gEfiAcpi10TableGuid))
		{
			EFI_ACPI_1_0_ROOT_SYSTEM_DESCRIPTION_POINTER* Rsdp=(EFI_ACPI_1_0_ROOT_SYSTEM_DESCRIPTION_POINTER*)CurrentEntry->VendorTable;
			if(Rsdp->Signature==EFI_ACPI_1_0_ROOT_SYSTEM_DESCRIPTION_POINTER_SIGNATURE)Rsdt=(EFI_ACPI_DESCRIPTION_HEADER*)((UINTN)Rsdp->RsdtAddress);
		}
	}
	if(Xsdt)
	{
		*Length=(UINTN)Xsdt->Length;
		return Xsdt;
	}
	else
	{
		*Length=(UINTN)Rsdt->Length;
		return Rsdt;
	}
}

BOOLEAN NoirAcpiInitialize()
{
	return nvc_acpi_initialize()==0;
}

BOOLEAN NoirHpetInitialize()
{
	return nvc_hpet_initialize()==0;
}

BOOLEAN NoirInitializeCodeIntegrity(IN VOID* ImageBase)
{
	// Locate Section List
	EFI_IMAGE_DOS_HEADER *DosHead=(EFI_IMAGE_DOS_HEADER*)ImageBase;
	if(DosHead->e_magic==EFI_IMAGE_DOS_SIGNATURE)
	{
		EFI_IMAGE_NT_HEADERS *NtHead=(EFI_IMAGE_NT_HEADERS*)((UINTN)ImageBase+DosHead->e_lfanew);
		if(NtHead->Signature==EFI_IMAGE_NT_SIGNATURE)
		{
			EFI_IMAGE_SECTION_HEADER *SectionHeaders=(EFI_IMAGE_SECTION_HEADER*)((UINTN)NtHead+sizeof(EFI_IMAGE_NT_HEADERS));
			if(noir_initialize_ci(FALSE,TRUE)==FALSE)
			{
				NoirDebugPrint("Failed to initialize Code-Integrity!\n");
				return FALSE;
			}
			for(UINT16 i=0;i<NtHead->FileHeader.NumberOfSections;i++)
			{
				// Locate Code Section
				if(AsciiStrnCmp((CHAR8*)SectionHeaders[i].Name,"hvtext",EFI_IMAGE_SIZEOF_SHORT_NAME)==0)
				{
					VOID* CodeBase=(VOID*)((UINTN)ImageBase+SectionHeaders[i].VirtualAddress);
					UINT32 CodeSize=SectionHeaders[i].SizeOfRawData;
					// Software CI Enforcement won't be supported in EFI Runtime Stage.
					// Hence, we will run Hardware CI Enforcement only in EFI.
					if(noir_add_section_to_ci(CodeBase,CodeSize,TRUE)==FALSE)
					{
						NoirDebugPrint("Failed to add code section to CI!\n");
						noir_finalize_ci();
						return FALSE;
					}
				}
				// Locate Data Section
				if(AsciiStrnCmp((CHAR8*)SectionHeaders[i].Name,"hvdata",EFI_IMAGE_SIZEOF_SHORT_NAME)==0)
				{
					VOID* DataBase=(VOID*)((UINTN)ImageBase+SectionHeaders[i].VirtualAddress);
					UINT32 DataSize=SectionHeaders[i].SizeOfRawData;
					if(noir_add_section_to_ci(DataBase,DataSize,TRUE)==FALSE)
					{
						NoirDebugPrint("Failed to add data section to CI!\n");
						noir_finalize_ci();
						return FALSE;
					}
				}
			}
			return noir_activate_ci();
		}
	}
	return FALSE;
}

void NoirFinalizeCodeIntegrity()
{
	noir_finalize_ci();
}

// Sleep-and-Wake setup in ACPI.
BOOLEAN noir_query_pm1_port_address(OUT UINT16 *PM1a,OUT UINT16 *PM1b)
{
	EFI_ACPI_2_0_FIXED_ACPI_DESCRIPTION_TABLE *Fadt=(EFI_ACPI_2_0_FIXED_ACPI_DESCRIPTION_TABLE*)EfiLocateFirstAcpiTable(EFI_ACPI_2_0_FIXED_ACPI_DESCRIPTION_TABLE_SIGNATURE);
	if(Fadt)
	{
		NoirDebugPrint("Located ACPI FADT at 0x%p\n",Fadt);
		// Use non-extended data for now.
		*PM1a=(UINT16)Fadt->Pm1aCntBlk;
		*PM1b=(UINT16)Fadt->Pm1bCntBlk;
		// Do some assertions.
		if(Fadt->XPm1aCntBlk.AddressSpaceId!=EFI_ACPI_2_0_SYSTEM_IO)
			NoirDebugPrint("X-PM1a does not use port I/O! It use space type %u instead!\n",Fadt->XPm1aCntBlk.AddressSpaceId);
		else
			NoirDebugPrint("X-PM1a register bit width: %u\n",Fadt->XPm1aCntBlk.RegisterBitWidth);
		if(Fadt->XPm1bCntBlk.AddressSpaceId!=EFI_ACPI_2_0_SYSTEM_IO)
			NoirDebugPrint("X-PM1b does not use port I/O! It use space type %u instead!\n",Fadt->XPm1bCntBlk.AddressSpaceId);
		else
			NoirDebugPrint("X-PM1b register bit width: %u\n",Fadt->XPm1bCntBlk.RegisterBitWidth);
		NoirDebugPrint("X-PM1a Address=0x%llX, PM1a Address=0x%X\n",Fadt->XPm1aCntBlk.Address,Fadt->Pm1aCntBlk);
		NoirDebugPrint("X-PM1b Address=0x%llX, PM1b Address=0x%X\n",Fadt->XPm1bCntBlk.Address,Fadt->Pm1bCntBlk);
	}
	return FALSE;
}