@echo off
set ddkpath=V:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.38.33130
set path=%ddkpath%\bin\Hostx64\x64;%path%
set incpath=V:\Program Files\Windows Kits\10\Include\10.0.26100.0
set mdepath=%EDK2_PATH%\edk2\MdePkg
set libpath=%EDK2_PATH%\bin\MdePkg
set binpath=..\bin\compchk_uefix64
set objpath=..\bin\compchk_uefix64\Intermediate

title Compiling NoirVisor, Checked Build, UEFI (AMD64 Architecture)
echo Project: NoirVisor
echo Platform: Unified Extensible Firmware Interface
echo Preset: Debug/Checked Build
echo Powered by zero.tangptr@gmail.com
echo Copyright (c) 2018-2023, zero.tangptr@gmail.com. All Rights Reserved.
if "%~1"=="/s" (echo DO-NOT-PAUSE is activated!) else (pause)

echo ============Start Compiling============
echo Compiling UEFI Booting Facility...
cl ..\src\booting\efiapp\efimain.c /I"%mdepath%\Include" /I"%mdepath%\Include\X64" /nologo /Zi /W3 /WX /Od /Oi /D"_efi_boot" /FAcs /Fa"%objpath%\efiapp\efimain.cod" /Fo"%objpath%\efiapp\efimain.obj" /Fd"%objpath%\vc140.pdb" /GS- /Qspectre /Gr /TC /c

cl ..\src\booting\efiapp\driver.c /I"%mdepath%\Include" /I"%mdepath%\Include\X64" /nologo /Zi /W3 /WX /Od /Oi /D"_efi_boot" /FAcs /Fa"%objpath%\driver\driver.cod" /Fo"%objpath%\driver\driver.obj" /Fd"%objpath%\vc140.pdb" /GS- /Qspectre /Gr /TC /c

echo Compiling NoirVisor CVM Emulator...
cl ..\src\disasm\emulator.c /I"..\src\include" /I"..\src\disasm\zydis\include" /I"..\src\disasm\zydis\dependencies\zycore\include" /I"..\src\disasm\zydis\msvc" /nologo /Zi /W3 /WX /Od /Oi /D"ZYDIS_STATIC_BUILD" /D"ZYAN_NO_LIBC" /D"_msvc" /D"_amd64" /D"_emulator" /FAcs /Fa"%objpath%\driver\emulator.cod" /Fo"%objpath%\driver\emulator.obj" /Fd"%objpath%\vc140.pdb" /GS- /Gr /Qspectre /TC /c /errorReport:queue

echo Compiling Core Engine of Intel VT-x...
for %%1 in (..\src\vt_core\*.c) do (cl %%1 /I"..\src\include" /nologo /Zi /W3 /WX /Od /Oi /D"_msvc" /D"_amd64" /D"_hv_type1" /D"_vt_core" /D"_%%~n1" /FAcs /Fa"%objpath%\driver\%%~n1.cod" /Fo"%objpath%\driver\%%~n1.obj" /Fd"%objpath%\vc140.pdb" /GS- /Qspectre /Gr /TC /c)

echo Compiling Core Engine of AMD-V...
for %%1 in (..\src\svm_core\*.c) do (cl %%1 /I"..\src\include" /nologo /Zi /W3 /WX /Od /Oi /D"_msvc" /D"_amd64" /D"_hv_type1" /D"_svm_core" /D"_%%~n1" /FAcs /Fa"%objpath%\driver\%%~n1.cod" /Fo"%objpath%\driver\%%~n1.obj" /Fd"%objpath%\vc140.pdb" /GS- /Qspectre /Gr /TC /c)

echo Compiling Core Engine of Microsoft Hypervisor (MSHV)...
for %%1 in (..\src\mshv_core\*.c) do (cl %%1 /I"..\src\include" /nologo /Zi /W3 /WX /Od /Oi /D"_msvc" /D"_amd64" /D"_hv_type1" /D"_mshv_core" /D"_%%~n1" /FAcs /Fa"%objpath%\driver\%%~n1.cod" /Fo"%objpath%\driver\%%~n1.obj" /Fd"%objpath%\vc140.pdb" /GS- /Qspectre /Gr /TC /c)

echo Compiling Core of Cross-Platform Framework (XPF)...
for %%1 in (..\src\xpf_core\uefi\*.c) do (cl %%1 /I"%mdepath%\Include" /I"%mdepath%\Include\X64" /I"%ddkpath%\include" /I"..\src\disasm\zydis\include" /I"..\src\disasm\zydis\dependencies\zycore\include" /nologo /Zi /W3 /WX /Od /Oi /D"_efi_boot" /FAcs /Fa"%objpath%\driver\%%~n1.cod" /Fo"%objpath%\driver\%%~n1.obj" /Fd"%objpath%\vc140.pdb" /GS- /Qspectre /Gr /TC /utf-8 /c)

for %%1 in (..\src\xpf_core\msvc\*.asm) do (ml64 /X /D"_amd64" /D"_msvc" /D"_efi" /nologo /I"..\src\xpf_core\msvc" /Fo"%objpath%\driver\%%~n1.obj" /c %%1)

cl ..\src\xpf_core\noirhvm.c /I"..\src\include" /nologo /Zi /W3 /WX /Od /Oi /D"_msvc" /D"_amd64" /D"_hv_type1" /D"_central_hvm" /FAcs /Fa"%objpath%\driver\noirhvm.cod" /Fo"%objpath%\driver\noirhvm.obj" /Fd"%objpath%\vc140.pdb" /GS- /Qspectre /Gr /TC /c

cl ..\src\xpf_core\ci.c /I"..\src\include" /nologo /Zi /W3 /WX /Od /Oi /D"_msvc" /D"_amd64" /D"_hv_type1" /D"_code_integrity" /FAcs /Fa"%objpath%\driver\ci.cod" /Fo"%objpath%\driver\ci.obj" /Fd"%objpath%\vc140.pdb" /GS- /Qspectre /Gr /TC /c

cl ..\src\xpf_core\devkits.c /I"..\src\include" /I"%ddkpath%\include" /nologo /Zi /W3 /WX /Od /Oi /D"_msvc" /D"_amd64" /D"_hv_type1" /D"_devkits" /FAcs /Fa"%objpath%\driver\devkits.cod" /Fo"%objpath%\driver\devkits.obj" /Fd"%objpath%\vc140.pdb" /GS- /Qspectre /Gr /TC /c

cl ..\src\xpf_core\nvdbg.c /I"..\src\include" /I"%ddkpath%\include" /Zi /nologo /W3 /WX /Oi /Od /D"_msvc" /D"_amd64" /D"_nvdbg" /Zc:wchar_t /std:c17 /FAcs /Fa"%objpath%\driver\nvdbg.cod" /Fo"%objpath%\driver\nvdbg.obj" /Fd"%objpath%\vc140.pdb" /GS- /Qspectre /TC /c /errorReport:queue

cl ..\src\xpf_core\c99-snprintf\snprintf.c /I"%incpath%\shared" /I"%incpath%\um" /I"%incpath%\ucrt" /I"%ddkpath%\include" /Zi /nologo /W3 /WX /wd4267 /wd4244 /Od /D"HAVE_STDARG_H" /D"HAVE_LOCALE_H" /D"HAVE_STDDEF_H" /D"HAVE_FLOAT_H" /D"HAVE_STDINT_H" /D"HAVE_INTTYPES_H" /D"HAVE_LONG_LONG_INT" /D"HAVE_UNSIGNED_LONG_LONG_INT" /D"HAVE_ASPRINTF" /D"HAVE_VASPRINTF" /D"HAVE_SNPRINTF" /Zc:wchar_t /std:c17 /FAcs /Fa"%objpath%\driver\snprintf.cod" /Fo"%objpath%\driver\snprintf.obj" /Fd"vc140.pdb" /GS- /Qspectre /TC /c /errorReport:queue

echo Compiling Core of Drivers...
cl ..\src\drv_core\serial\serial.c /I"..\src\include" /Zi /nologo /W3 /WX /Oi /Od /D"_msvc" /D"_amd64" /D"_drv_serial" /Zc:wchar_t /std:c17 /FAcs /Fa"%objpath%\driver\serial.cod" /Fo"%objpath%\driver\serial.obj" /Fd"%objpath%\vc140.pdb" /GS- /Qspectre /TC /c /errorReport:queue

cl ..\src\drv_core\qemu_debugcon\qemu_debugcon.c /I"..\src\include" /Zi /nologo /W3 /WX /Oi /Od /D"_msvc" /D"_amd64" /D"_drv_qemu_debugcon" /Zc:wchar_t /std:c17 /FAcs /Fa"%objpath%\driver\qemu_debugcon.cod" /Fo"%objpath%\driver\qemu_debugcon.obj" /Fd"%objpath%\vc140.pdb" /GS- /Qspectre /TC /c /errorReport:queue

cl ..\src\drv_core\hpet\hpet.c /I"..\src\include" /Zi /nologo /W3 /WX /Oi /Od /D"_msvc" /D"_amd64" /D"_drv_hpet" /Zc:wchar_t /std:c17 /FAcs /Fa"%objpath%\driver\hpet.cod" /Fo"%objpath%\driver\hpet.obj" /Fd"%objpath%\vc140.pdb" /GS- /Qspectre /TC /c /errorReport:queue

for %%1 in (..\src\drv_core\acpi\*.c) do (cl %%1 /I"..\src\include" /Zi /nologo /W3 /WX /Oi /Od /D"_msvc" /D"_amd64" /D"_%%~n1" /Zc:wchar_t /std:c17 /FAcs /Fa"%objpath%\driver\%%~n1.cod" /Fo"%objpath%\driver\%%~n1.obj" /Fd"%objpath%\vc140.pdb" /GS- /Qspectre /TC /c /errorReport:queue)

echo ============Start Linking============
echo Linking NoirVisor EFI Loader Application...
link "%objpath%\efiapp\*.obj" /NODEFAULTLIB /LIBPATH:"%libpath%\compchk_uefix64" "MdePkgGuids.lib" "BaseLib.lib" "BaseDebugPrintErrorLevelLib.lib" "BaseMemoryLib.lib" "BasePrintLib.lib" "UefiLib.lib" "UefiDebugLibConOut.lib" "UefiMemoryAllocationLib.lib" "UefiDevicePathLibDevicePathProtocol.Lib" "UefiBootServicesTableLib.Lib" "UefiRuntimeServicesTableLib.Lib" /NOLOGO /OUT:"%binpath%\bootx64.efi" /SUBSYSTEM:EFI_APPLICATION /ENTRY:"NoirEfiEntry" /DEBUG /PDB:"%binpath%\bootx64.pdb" /Machine:X64

echo Linking NoirVisor EFI Hypervisor Runtime Driver...
link "%objpath%\driver\*.obj"  /NODEFAULTLIB /LIBPATH:"%libpath%\compchk_uefix64" "MdePkgGuids.lib" "BaseLib.lib" "BaseDebugPrintErrorLevelLib.lib" "BaseIoLibIntrinsic.Lib" "BaseMemoryLib.lib" "BasePrintLib.lib" "UefiLib.lib" "UefiDebugLibConOut.lib" "UefiMemoryAllocationLib.lib" "UefiDevicePathLibDevicePathProtocol.Lib" "UefiBootServicesTableLib.Lib" "UefiRuntimeServicesTableLib.Lib" "..\src\disasm\bin\compchk_win11x64\zydis.lib" /NOLOGO /OUT:"%binpath%\NoirVisor.efi" /SUBSYSTEM:EFI_RUNTIME_DRIVER /ENTRY:"NoirDriverEntry" /DEBUG /PDB:"%binpath%\NoirVisor.pdb" /Machine:X64

echo ============Start Imaging============
echo Creating Disk Image...
set /A imagesize_kb=1440
set /A imagesize_b=%imagesize_kb*1024
if exist %binpath%\NoirVisor-Uefi.img (fsutil file setzerodata offset=0 length=%imagesize_b% %binpath%\NoirVisor-Uefi.img) else (fsutil file createnew %binpath%\NoirVisor-Uefi.img %imagesize_b%)
echo Formatting Disk Image...
mformat -i %binpath%\NoirVisor-Uefi.img -f %imagesize_kb% ::
mmd -i %binpath%\NoirVisor-Uefi.img ::/EFI
mmd -i %binpath%\NoirVisor-Uefi.img ::/EFI/BOOT
echo Making Config...
python makeueficonfig.py DefaultUefiConfig.json %binpath%\NoirVisorConfig.bin
echo Copying into Disk Image...
mcopy -i %binpath%\NoirVisor-Uefi.img %binpath%\NoirVisor.efi ::/
mcopy -i %binpath%\NoirVisor-Uefi.img %binpath%\NoirVisorConfig.bin ::/
mcopy -i %binpath%\NoirVisor-Uefi.img %binpath%\bootx64.efi ::/EFI/BOOT

if "%~1"=="/s" (echo Completed!) else (pause)