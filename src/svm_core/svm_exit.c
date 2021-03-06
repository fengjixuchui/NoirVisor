/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2021, Zero Tang. All rights reserved.

  This file is the basic Exit Handler of SVM Driver.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /svm_core/svm_exit.c
*/

#include <nvdef.h>
#include <nvbdk.h>
#include <noirhvm.h>
#include <svm_intrin.h>
#include <nv_intrin.h>
#include <amd64.h>
#include <ci.h>
#include "svm_vmcb.h"
#include "svm_npt.h"
#include "svm_exit.h"
#include "svm_def.h"

// Unexpected VM-Exit occured. You may want to debug your code if this function is invoked.
void static fastcall nvc_svm_default_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu)
{
	const void* vmcb=vcpu->vmcb.virt;
	i32 code=noir_svm_vmread32(vmcb,exit_code);
	/*
	  Following conditions might cause the default handler to be invoked:

	  1. You have set an unwanted flag in VMCB of interception.
	  2. You forgot to write a handler for the VM-Exit.
	  3. You forgot to set the handler address to the Exit Handler Group.

	  Use the printed Intercept Code to debug.
	  For example, you received 0x401 as the intercept code.
	  This means you enabled nested paging but did not set a #NPF handler.
	*/
	nv_dprintf("Unhandled VM-Exit! Intercept Code: 0x%X\n",code);
}

// Expected Intercept Code: -1
void static fastcall nvc_svm_invalid_guest_state(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu)
{
	const void* vmcb=vcpu->vmcb.virt;
	u64 efer;
	ulong_ptr cr0,cr3,cr4;
	ulong_ptr dr6,dr7;
	u32 list1,list2;
	u32 asid;
	nv_dprintf("[Processor %d] Guest State is Invalid! VMCB: 0x%p\n",vcpu->proc_id,vmcb);
	// Dump State in VMCB and Print them to Debugger.
	efer=noir_svm_vmread64(vmcb,guest_efer);
	nv_dprintf("Guest EFER MSR: 0x%llx\n",efer);
	dr6=noir_svm_vmread(vmcb,guest_dr6);
	dr7=noir_svm_vmread(vmcb,guest_dr7);
	nv_dprintf("Guest DR6: 0x%p\t DR7: 0x%p\n",dr6,dr7);
	cr0=noir_svm_vmread(vmcb,guest_cr0);
	cr3=noir_svm_vmread(vmcb,guest_cr3);
	cr4=noir_svm_vmread(vmcb,guest_cr4);
	nv_dprintf("Guest CR0: 0x%p\t CR3: 0x%p\t CR4: 0x%p\n",cr0,cr3,cr4);
	asid=noir_svm_vmread32(vmcb,guest_asid);
	nv_dprintf("Guest ASID: %d\n",asid);
	list1=noir_svm_vmread32(vmcb,intercept_instruction1);
	list2=noir_svm_vmread32(vmcb,intercept_instruction2);
	nv_dprintf("Control 1: 0x%X\t Control 2: 0x%X\n",list1,list2);
	// Generate a debug-break.
	noir_int3();
}

// Expected Intercept Code: 0x5E
void static fastcall nvc_svm_sx_exception_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu)
{
	void* vmcb=vcpu->vmcb.virt;
	u32 error_code=noir_svm_vmread32(vmcb,exit_info1);
	switch(error_code)
	{
		case amd64_sx_init_redirection:
		{
			// The default treatment of INIT interception is absurd in AMD-V
			// in that INIT signal would not disappear on interception!
			// We thereby have to redirect INIT signals into #SX exceptions.
			// Emulate what a real INIT Signal would do.
			// For details of emulation, see Table 14-1 in Vol.2 of AMD64 APM.
			u64 cr0=noir_svm_vmread64(vmcb,guest_cr0);
			cr0&=0x60000000;		// Bits CD & NW of CR0 are unchanged during INIT.
			cr0|=0x00000010;		// Bit ET of CR0 is always set.
			noir_svm_vmwrite64(vmcb,guest_cr0,cr0);
			// CR2, CR3 and CR4 are cleared to zero.
			noir_svm_vmwrite64(vmcb,guest_cr2,0);
			noir_svm_vmwrite64(vmcb,guest_cr3,0);
			noir_svm_vmwrite64(vmcb,guest_cr4,0);
			// Leave SVME set but others reset in EFER.
			noir_svm_vmwrite64(vmcb,guest_efer,amd64_efer_svme_bit);
			// Debug Registers...
			noir_writedr0(0);
			noir_writedr1(0);
			noir_writedr2(0);
			noir_writedr3(0);
			noir_svm_vmwrite64(vmcb,guest_dr6,0xFFFF0FF0);
			noir_svm_vmwrite64(vmcb,guest_dr7,0x400);
			// Segment Register - CS
			noir_svm_vmwrite16(vmcb,guest_cs_selector,0xF000);
			noir_svm_vmwrite16(vmcb,guest_cs_attrib,0x9B);
			noir_svm_vmwrite32(vmcb,guest_cs_limit,0xFFFF);
			noir_svm_vmwrite64(vmcb,guest_cs_base,0xFFFF0000);
			// Segment Register - DS
			noir_svm_vmwrite16(vmcb,guest_ds_selector,0);
			noir_svm_vmwrite16(vmcb,guest_ds_attrib,0x93);
			noir_svm_vmwrite32(vmcb,guest_ds_limit,0xFFFF);
			noir_svm_vmwrite64(vmcb,guest_ds_base,0);
			// Segment Register - ES
			noir_svm_vmwrite16(vmcb,guest_es_selector,0);
			noir_svm_vmwrite16(vmcb,guest_es_attrib,0x93);
			noir_svm_vmwrite32(vmcb,guest_es_limit,0xFFFF);
			noir_svm_vmwrite64(vmcb,guest_es_base,0);
			// Segment Register - FS
			noir_svm_vmwrite16(vmcb,guest_fs_selector,0);
			noir_svm_vmwrite16(vmcb,guest_fs_attrib,0x93);
			noir_svm_vmwrite32(vmcb,guest_fs_limit,0xFFFF);
			noir_svm_vmwrite64(vmcb,guest_fs_base,0);
			// Segment Register - GS
			noir_svm_vmwrite16(vmcb,guest_gs_selector,0);
			noir_svm_vmwrite16(vmcb,guest_gs_attrib,0x93);
			noir_svm_vmwrite32(vmcb,guest_gs_limit,0xFFFF);
			noir_svm_vmwrite64(vmcb,guest_gs_base,0);
			// Segment Register - SS
			noir_svm_vmwrite16(vmcb,guest_ss_selector,0);
			noir_svm_vmwrite16(vmcb,guest_ss_attrib,0x93);
			noir_svm_vmwrite32(vmcb,guest_ss_limit,0xFFFF);
			noir_svm_vmwrite64(vmcb,guest_ss_base,0);
			// Segment Register - GDTR
			noir_svm_vmwrite32(vmcb,guest_gdtr_limit,0xFFFF);
			noir_svm_vmwrite64(vmcb,guest_gdtr_base,0);
			// Segment Register - IDTR
			noir_svm_vmwrite32(vmcb,guest_idtr_limit,0xFFFF);
			noir_svm_vmwrite64(vmcb,guest_idtr_base,0);
			// Segment Register - LDTR
			noir_svm_vmwrite16(vmcb,guest_ldtr_selector,0);
			noir_svm_vmwrite16(vmcb,guest_ldtr_attrib,0x82);
			noir_svm_vmwrite32(vmcb,guest_ldtr_limit,0xFFFF);
			noir_svm_vmwrite64(vmcb,guest_ldtr_base,0);
			// Segment Register - TR
			noir_svm_vmwrite16(vmcb,guest_tr_selector,0);
			noir_svm_vmwrite16(vmcb,guest_tr_attrib,0x8b);
			noir_svm_vmwrite32(vmcb,guest_tr_limit,0xFFFF);
			noir_svm_vmwrite64(vmcb,guest_tr_base,0);
			// IDTR & GDTR
			noir_svm_vmwrite16(vmcb,guest_gdtr_limit,0xFFFF);
			noir_svm_vmwrite16(vmcb,guest_idtr_limit,0xFFFF);
			noir_svm_vmwrite64(vmcb,guest_gdtr_base,0);
			noir_svm_vmwrite64(vmcb,guest_idtr_base,0);
			// General Purpose Registers...
			noir_svm_vmwrite64(vmcb,guest_rsp,0);
			noir_svm_vmwrite64(vmcb,guest_rip,0xfff0);
			noir_svm_vmwrite64(vmcb,guest_rflags,2);
			noir_stosp(gpr_state,0,sizeof(void*)*2);	// Clear the GPRs.
			gpr_state->rdx=vcpu->cpuid_fms;				// Use info from cached CPUID.
			// FIXME: Set the vCPU to "Wait-for-SPI" State. AMD-V lacks this feature.
			// Flush all TLBs in that paging in the guest is switched off during INIT signal.
			noir_svm_vmwrite8(vmcb,tlb_control,nvc_svm_tlb_control_flush_guest);
			// Mark certain cached items as dirty in order to invalidate them.
			noir_btr((u32*)((ulong_ptr)vcpu->vmcb.virt+vmcb_clean_bits),noir_svm_clean_control_reg);
			noir_btr((u32*)((ulong_ptr)vcpu->vmcb.virt+vmcb_clean_bits),noir_svm_clean_debug_reg);
			noir_btr((u32*)((ulong_ptr)vcpu->vmcb.virt+vmcb_clean_bits),noir_svm_clean_idt_gdt);
			noir_btr((u32*)((ulong_ptr)vcpu->vmcb.virt+vmcb_clean_bits),noir_svm_clean_segment_reg);
			noir_btr((u32*)((ulong_ptr)vcpu->vmcb.virt+vmcb_clean_bits),noir_svm_clean_cr2);
			break;
		}
		default:
		{
			// Unrecognized #SX Exception, leave it be for the Guest.
			noir_svm_inject_event(vmcb,amd64_security_exception,amd64_fault_trap_exception,true,true,error_code);
			break;
		}
	}
}

// Hypervisor-Present CPUID Handler
void static fastcall nvc_svm_cpuid_hvp_handler(u32 leaf,u32 subleaf,noir_cpuid_general_info_p info)
{
	// First, classify the leaf function.
	u32 leaf_class=noir_cpuid_class(leaf);
	u32 leaf_func=noir_cpuid_index(leaf);
	if(leaf_class==hvm_leaf_index)
	{
		if(leaf_func<hvm_p->relative_hvm->hvm_cpuid_leaf_max)
			hvm_cpuid_handlers[leaf_func](info);
		else
			noir_stosd((u32*)&info,0,4);
	}
	else
	{
		noir_cpuid(leaf,subleaf,&info->eax,&info->ebx,&info->ecx,&info->edx);
		switch(leaf)
		{
			case amd64_cpuid_std_proc_feature:
			{
				noir_bts(&info->ecx,amd64_cpuid_hv_presence);
				break;
			}
			case amd64_cpuid_ext_proc_feature:
			{
				noir_btr(&info->ecx,amd64_cpuid_svm);
				break;
			}
			case amd64_cpuid_ext_svm_features:
			{
				noir_stosd((u32*)&info,0,4);
				break;
			}
			case amd64_cpuid_ext_mem_crypting:
			{
				noir_stosd((u32*)&info,0,4);
				break;
			}
		}
	}
}

// Hypervisor-Stealthy CPUID Handler
void static fastcall nvc_svm_cpuid_hvs_handler(u32 leaf,u32 subleaf,noir_cpuid_general_info_p info)
{
	noir_cpuid(leaf,subleaf,&info->eax,&info->ebx,&info->ecx,&info->edx);
	switch(leaf)
	{
		case amd64_cpuid_ext_proc_feature:
		{
			noir_btr(&info->ecx,amd64_cpuid_svm);
			break;
		}
		case amd64_cpuid_ext_svm_features:
		{
			noir_stosd((u32*)&info,0,4);
			break;
		}
		case amd64_cpuid_ext_mem_crypting:
		{
			noir_stosd((u32*)&info,0,4);
			break;
		}
	}
}

// Expected Intercept Code: 0x72
void static fastcall nvc_svm_cpuid_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu)
{
	u32 ia=(u32)gpr_state->rax;
	u32 ic=(u32)gpr_state->rcx;
	noir_cpuid_general_info info;
	nvcp_svm_cpuid_handler(ia,ic,&info);
	*(u32*)&gpr_state->rax=info.eax;
	*(u32*)&gpr_state->rbx=info.ebx;
	*(u32*)&gpr_state->rcx=info.ecx;
	*(u32*)&gpr_state->rdx=info.edx;
	// Finally, advance the instruction pointer.
	noir_svm_advance_rip(vcpu->vmcb.virt);
}

// This is a branch of MSR-Exit. DO NOT ADVANCE RIP HERE!
void static fastcall nvc_svm_rdmsr_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu)
{
	void* vmcb=vcpu->vmcb.virt;
	// The index of MSR is saved in ecx register (32-bit).
	u32 index=(u32)gpr_state->rcx;
	large_integer val={0};
	if(noir_is_synthetic_msr(index))
	{
		if(hvm_p->options.cpuid_hv_presence)
			val.value=nvc_mshv_rdmsr_handler(index);
		else
			// Synthetic MSR is not allowed, inject #GP to Guest.
			noir_svm_inject_event(vmcb,amd64_general_protection,amd64_fault_trap_exception,true,true,0);
	}
	else
	{
		switch(index)
		{
			case amd64_efer:
			{
				// Read the EFER value from VMCB.
				val.value=noir_svm_vmread64(vmcb,guest_efer);
				// The SVME bit should be filtered.
				if(vcpu->nested_hvm.svme==0)val.value&=~amd64_efer_svme_bit;
				break;
			}
			case amd64_hsave_pa:
			{
				// Read the physical address of Host-Save Area from nested HVM structure.
				val.value=vcpu->nested_hvm.hsave_gpa=val.value;
				break;
			}
			// To be implemented in future.
#if defined(_amd64)
			case amd64_lstar:
			{
				val.value=vcpu->virtual_msr.lstar;
				break;
			}
#else
			case amd64_sysenter_eip:
			{
				val.value=vcpu->virtual_msr.sysenter_eip;
				break;
			}
#endif
		}
	}
	// Higher 32 bits of rax and rdx will be cleared.
	gpr_state->rax=(ulong_ptr)val.low;
	gpr_state->rdx=(ulong_ptr)val.high;
}

// This is a branch of MSR-Exit. DO NOT ADVANCE RIP HERE!
void static fastcall nvc_svm_wrmsr_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu)
{
	void* vmcb=vcpu->vmcb.virt;
	// The index of MSR is saved in ecx register (32-bit).
	u32 index=(u32)gpr_state->rcx;
	large_integer val;
	// Get the value to be written.
	val.low=(u32)gpr_state->rax;
	val.high=(u32)gpr_state->rdx;
	if(noir_is_synthetic_msr(index))
	{
		if(hvm_p->options.cpuid_hv_presence)
			nvc_mshv_wrmsr_handler(index,val.value);
		else
			// Synthetic MSR is not allowed, inject #GP to Guest.
			noir_svm_inject_event(vmcb,amd64_general_protection,amd64_fault_trap_exception,true,true,0);
	}
	else
	{
		switch(index)
		{
			case amd64_efer:
			{
				// This is for future feature of nested virtualization.
				vcpu->nested_hvm.svme=noir_bt(&val.low,amd64_efer_svme);
				val.value|=amd64_efer_svme_bit;
				// Other bits can be ignored, but SVME should be always protected.
				noir_svm_vmwrite64(vmcb,guest_efer,val.value);
				// We updated EFER. Therefore, CRx fields should be invalidated.
				noir_svm_vmcb_btr32(vmcb,vmcb_clean_bits,noir_svm_clean_control_reg);
				break;
			}
			case amd64_hsave_pa:
			{
				// Store the physical address of Host-Save Area to nested HVM structure.
				vcpu->nested_hvm.hsave_gpa=val.value;
				break;
			}
#if defined(_amd64)
			case amd64_lstar:
			{
				vcpu->virtual_msr.lstar=val.value;
				break;
			}
#else
			case amd64_sysenter_eip:
			{
				vcpu->virtual_msr.sysenter_eip=val.value;
				break;
			}
#endif
		}
	}
}

// Expected Intercept Code: 0x7C
void static fastcall nvc_svm_msr_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu)
{
	const void* vmcb=vcpu->vmcb.virt;
	// Determine the type of operation.
	bool op_write=noir_svm_vmread8(vmcb,exit_info1);
	// 
	if(op_write)
		nvc_svm_wrmsr_handler(gpr_state,vcpu);
	else
		nvc_svm_rdmsr_handler(gpr_state,vcpu);
	noir_svm_advance_rip(vcpu->vmcb.virt);
}

// Expected Intercept Code: 0x7F
// If this VM-Exit occurs, it may indicate a triple fault.
void static fastcall nvc_svm_shutdown_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu)
{
	noir_svm_stgi();	// Enable GIF for Debug-Printing
	nv_dprintf("Shutdown is Intercepted!\n");
	noir_int3();
}

// Expected Intercept Code: 0x80
// This is the cornerstone of nesting virtualization.
void static fastcall nvc_svm_vmrun_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu)
{
	void* vmcb=vcpu->vmcb.virt;
	if(vcpu->nested_hvm.svme)
	{
		// Get the Nested VMCB.
		const ulong_ptr nested_vmcb_pa=gpr_state->rax;
		const void* nested_vmcb=noir_find_virt_by_phys(nested_vmcb_pa);
		// Some essential information for hypervisor-specific consistency check.
		const u32 nested_asid=noir_svm_vmread32(nested_vmcb,guest_asid);
		if(nested_asid==0)noir_int3();		// FIXME: Issue VM-Exit for invalid state.
		nv_dprintf("VM-Exit occured by vmrun instruction!\n");
			nv_dprintf("Nested Virtualization of SVM is not supported!\n");
		// There is absolutely no SVM instructions since we don't support nested virtualization at this point.
		noir_svm_advance_rip(vcpu->vmcb.virt);
	}
	else
	{
		// SVM is disabled in guest EFER. Inject #UD to guest.
		noir_svm_inject_event(vmcb,amd64_invalid_opcode,amd64_fault_trap_exception,false,true,0);
	}
}

// Expected Intercept Code: 0x81
void static fastcall nvc_svm_vmmcall_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu)
{
	u32 vmmcall_func=(u32)gpr_state->rcx;
	ulong_ptr context=gpr_state->rdx;
	ulong_ptr gsp=noir_svm_vmread(vcpu->vmcb.virt,guest_rsp);
	ulong_ptr gip=noir_svm_vmread(vcpu->vmcb.virt,guest_rip);
	ulong_ptr gcr3=noir_svm_vmread(vcpu->vmcb.virt,guest_cr3);
	unref_var(context);
	switch(vmmcall_func)
	{
		case noir_svm_callexit:
		{
			// Validate the caller to prevent malicious unloading request.
			if(gip>=hvm_p->hv_image.base && gip<hvm_p->hv_image.base+hvm_p->hv_image.size)
			{
				// Directly use space from the starting stack position.
				// Normally it is unused.
				noir_gpr_state_p saved_state=(noir_gpr_state_p)vcpu->hv_stack;
				noir_svm_stgi();
				// Before Debug-Print, GIF should be set because Debug-Printing requires IPI not to be blocked.
				nv_dprintf("VMM-Call for Restoration is intercepted. Exiting...\n");
				// Copy state.
				noir_movsp(saved_state,gpr_state,sizeof(void*)*2);
				saved_state->rax=noir_svm_vmread(vcpu->vmcb.virt,next_rip);
				saved_state->rcx=noir_svm_vmread(vcpu->vmcb.virt,guest_rflags);
				saved_state->rdx=gsp;
				// Restore processor's hidden state.
				noir_svm_vmwrite64(vcpu->vmcb.virt,guest_lstar,(u64)orig_system_call);
				noir_svm_vmload((ulong_ptr)vcpu->vmcb.phys);
				// Switch to Restored CR3
				noir_writecr3(gcr3);
				// Mark the processor is in transition mode.
				vcpu->status=noir_virt_trans;
				// Return to the caller at Host Mode.
				nvc_svm_return(saved_state);
				// Never reaches here.
			}
			// If execution goes here, then the invoker is malicious.
			nv_dprintf("Malicious call of exit!\n");
			break;
		}
		case noir_svm_run_custom_vcpu:
		{
			// Step 1: Save State of the Subverted Host.
			// Step 2: Load Guest State and run Guest.
			// Step 3: Save Guest State and Load Host State.
			// Step 4: Return to Host.
			break;
		}
		default:
		{
			nv_dprintf("Unknown vmmcall function!\n");
			break;
		}
	}
	noir_svm_advance_rip(vcpu->vmcb.virt);
}

// Expected Intercept Code: 0x82
void static fastcall nvc_svm_vmload_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu)
{
	void* vmcb=vcpu->vmcb.virt;
	if(vcpu->nested_hvm.svme)
	{
		// Get Address of Nested VMCB.
		const ulong_ptr nested_vmcb_pa=gpr_state->rax;
		void* nested_vmcb=noir_find_virt_by_phys(nested_vmcb_pa);
		// Load to Current VMCB - FS.
		noir_svm_vmwrite16(vmcb,guest_fs_selector,noir_svm_vmread16(nested_vmcb,guest_fs_selector));
		noir_svm_vmwrite16(vmcb,guest_fs_attrib,noir_svm_vmread16(nested_vmcb,guest_fs_attrib));
		noir_svm_vmwrite32(vmcb,guest_fs_limit,noir_svm_vmread32(nested_vmcb,guest_fs_limit));
		noir_svm_vmwrite64(vmcb,guest_fs_base,noir_svm_vmread64(nested_vmcb,guest_fs_base));
		// Load to Current VMCB - GS.
		noir_svm_vmwrite16(vmcb,guest_gs_selector,noir_svm_vmread16(nested_vmcb,guest_gs_selector));
		noir_svm_vmwrite16(vmcb,guest_gs_attrib,noir_svm_vmread16(nested_vmcb,guest_gs_attrib));
		noir_svm_vmwrite32(vmcb,guest_gs_limit,noir_svm_vmread32(nested_vmcb,guest_gs_limit));
		noir_svm_vmwrite64(vmcb,guest_gs_base,noir_svm_vmread64(nested_vmcb,guest_gs_base));
		// Load to Current VMCB - TR.
		noir_svm_vmwrite16(vmcb,guest_tr_selector,noir_svm_vmread16(nested_vmcb,guest_tr_selector));
		noir_svm_vmwrite16(vmcb,guest_tr_attrib,noir_svm_vmread16(nested_vmcb,guest_tr_attrib));
		noir_svm_vmwrite32(vmcb,guest_tr_limit,noir_svm_vmread32(nested_vmcb,guest_tr_limit));
		noir_svm_vmwrite64(vmcb,guest_tr_base,noir_svm_vmread64(nested_vmcb,guest_tr_base));
		// Load to Current VMCB - LDTR.
		noir_svm_vmwrite16(vmcb,guest_ldtr_selector,noir_svm_vmread16(nested_vmcb,guest_ldtr_selector));
		noir_svm_vmwrite16(vmcb,guest_ldtr_attrib,noir_svm_vmread16(nested_vmcb,guest_ldtr_attrib));
		noir_svm_vmwrite32(vmcb,guest_ldtr_limit,noir_svm_vmread32(nested_vmcb,guest_ldtr_limit));
		noir_svm_vmwrite64(vmcb,guest_ldtr_base,noir_svm_vmread64(nested_vmcb,guest_ldtr_base));
		// Load to Current VMCB - MSR.
		noir_svm_vmwrite64(vmcb,guest_sysenter_cs,noir_svm_vmread64(nested_vmcb,guest_sysenter_cs));
		noir_svm_vmwrite64(vmcb,guest_sysenter_esp,noir_svm_vmread64(nested_vmcb,guest_sysenter_esp));
		noir_svm_vmwrite64(vmcb,guest_sysenter_eip,noir_svm_vmread64(nested_vmcb,guest_sysenter_eip));
		noir_svm_vmwrite64(vmcb,guest_star,noir_svm_vmread64(nested_vmcb,guest_star));
		noir_svm_vmwrite64(vmcb,guest_lstar,noir_svm_vmread64(nested_vmcb,guest_lstar));
		noir_svm_vmwrite64(vmcb,guest_cstar,noir_svm_vmread64(nested_vmcb,guest_cstar));
		noir_svm_vmwrite64(vmcb,guest_sfmask,noir_svm_vmread64(nested_vmcb,guest_sfmask));
		noir_svm_vmwrite64(vmcb,guest_kernel_gs_base,noir_svm_vmread64(nested_vmcb,guest_kernel_gs_base));
		// Everything are loaded to Current VMCB. Return to guest.
		noir_svm_advance_rip(vcpu->vmcb.virt);
	}
	else
	{
		// SVM is disabled in guest EFER, inject #UD.
		noir_svm_inject_event(vmcb,amd64_invalid_opcode,amd64_fault_trap_exception,false,true,0);
	}
}

// Expected Intercept Code: 0x83
void static fastcall nvc_svm_vmsave_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu)
{
	void* vmcb=vcpu->vmcb.virt;
	if(vcpu->nested_hvm.svme)
	{
		// Get Address of Nested VMCB.
		const ulong_ptr nested_vmcb_pa=gpr_state->rax;
		void* nested_vmcb=noir_find_virt_by_phys(nested_vmcb_pa);
		// Save to Nested VMCB - FS.
		noir_svm_vmwrite16(nested_vmcb,guest_fs_selector,noir_svm_vmread16(vmcb,guest_fs_selector));
		noir_svm_vmwrite16(nested_vmcb,guest_fs_attrib,noir_svm_vmread16(vmcb,guest_fs_attrib));
		noir_svm_vmwrite32(nested_vmcb,guest_fs_limit,noir_svm_vmread32(vmcb,guest_fs_limit));
		noir_svm_vmwrite64(nested_vmcb,guest_fs_base,noir_svm_vmread64(vmcb,guest_fs_base));
		// Save to Nested VMCB - GS.
		noir_svm_vmwrite16(nested_vmcb,guest_gs_selector,noir_svm_vmread16(vmcb,guest_gs_selector));
		noir_svm_vmwrite16(nested_vmcb,guest_gs_attrib,noir_svm_vmread16(vmcb,guest_gs_attrib));
		noir_svm_vmwrite32(nested_vmcb,guest_gs_limit,noir_svm_vmread32(vmcb,guest_gs_limit));
		noir_svm_vmwrite64(nested_vmcb,guest_gs_base,noir_svm_vmread64(vmcb,guest_gs_base));
		// Save to Nested VMCB - TR.
		noir_svm_vmwrite16(nested_vmcb,guest_tr_selector,noir_svm_vmread16(vmcb,guest_tr_selector));
		noir_svm_vmwrite16(nested_vmcb,guest_tr_attrib,noir_svm_vmread16(vmcb,guest_tr_attrib));
		noir_svm_vmwrite32(nested_vmcb,guest_tr_limit,noir_svm_vmread32(vmcb,guest_tr_limit));
		noir_svm_vmwrite64(nested_vmcb,guest_tr_base,noir_svm_vmread64(vmcb,guest_tr_base));
		// Save to Nested VMCB - LDTR.
		noir_svm_vmwrite16(nested_vmcb,guest_ldtr_selector,noir_svm_vmread16(vmcb,guest_ldtr_selector));
		noir_svm_vmwrite16(nested_vmcb,guest_ldtr_attrib,noir_svm_vmread16(vmcb,guest_ldtr_attrib));
		noir_svm_vmwrite32(nested_vmcb,guest_ldtr_limit,noir_svm_vmread32(vmcb,guest_ldtr_limit));
		noir_svm_vmwrite64(nested_vmcb,guest_ldtr_base,noir_svm_vmread64(vmcb,guest_ldtr_base));
		// Save to Nested VMCB - MSR.
		noir_svm_vmwrite64(nested_vmcb,guest_sysenter_cs,noir_svm_vmread64(vmcb,guest_sysenter_cs));
		noir_svm_vmwrite64(nested_vmcb,guest_sysenter_esp,noir_svm_vmread64(vmcb,guest_sysenter_esp));
		noir_svm_vmwrite64(nested_vmcb,guest_sysenter_eip,noir_svm_vmread64(vmcb,guest_sysenter_eip));
		noir_svm_vmwrite64(nested_vmcb,guest_star,noir_svm_vmread64(vmcb,guest_star));
		noir_svm_vmwrite64(nested_vmcb,guest_lstar,noir_svm_vmread64(vmcb,guest_lstar));
		noir_svm_vmwrite64(nested_vmcb,guest_cstar,noir_svm_vmread64(vmcb,guest_cstar));
		noir_svm_vmwrite64(nested_vmcb,guest_sfmask,noir_svm_vmread64(vmcb,guest_sfmask));
		noir_svm_vmwrite64(nested_vmcb,guest_kernel_gs_base,noir_svm_vmread64(vmcb,guest_kernel_gs_base));
		// Everything are saved to Nested VMCB. Return to guest.
		noir_svm_advance_rip(vcpu->vmcb.virt);
	}
	else
	{
		// SVM is disabled in guest EFER, inject #UD.
		noir_svm_inject_event(vmcb,amd64_invalid_opcode,amd64_fault_trap_exception,false,true,0);
	}
}

// Expected Intercept Code: 0x84
void static fastcall nvc_svm_stgi_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu)
{
	void* vmcb=vcpu->vmcb.virt;
	if(vcpu->nested_hvm.svme)
	{
		vcpu->nested_hvm.gif=1;		// Marks that GIF is set in Guest.
		// FIXME: Inject pending interrupts held due to cleared GIF, and clear interceptions on certain interrupts.
		noir_svm_advance_rip(vcpu->vmcb.virt);
	}
	else
	{
		// SVM is disabled in guest EFER, inject #UD.
		noir_svm_inject_event(vmcb,amd64_invalid_opcode,amd64_fault_trap_exception,false,true,0);
	}
}

// Expected Intercept Code: 0x85
void static fastcall nvc_svm_clgi_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu)
{
	void* vmcb=vcpu->vmcb.virt;
	if(vcpu->nested_hvm.svme)
	{
		vcpu->nested_hvm.gif=0;		// Marks that GIF is reset in Guest.
		// FIXME: Setup interceptions on certain interrupts.
		noir_svm_advance_rip(vcpu->vmcb.virt);
	}
	else
	{
		// SVM is disabled in guest EFER, inject #UD.
		noir_svm_inject_event(vmcb,amd64_invalid_opcode,amd64_fault_trap_exception,false,true,0);
	}
}

// Expected Intercept Code: 0x86
void static fastcall nvc_svm_skinit_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu)
{
	void* vmcb=vcpu->vmcb.virt;
	// No skinit support in NoirVisor Guest.
	noir_svm_inject_event(vmcb,amd64_invalid_opcode,amd64_fault_trap_exception,false,true,0);
}

// Expected Intercept Code: 0x400
// Do not output to debugger since this may seriously degrade performance.
void static fastcall nvc_svm_nested_pf_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu)
{
	bool advance=true;
	// Necessary Information for #NPF VM-Exit.
	amd64_npt_fault_code fault;
	fault.value=noir_svm_vmread64(vcpu->vmcb.virt,exit_info1);
#if !defined(_hv_type1)
	if(fault.execute)
	{
		i32 lo=0,hi=noir_hook_pages_count;
		u64 gpa=noir_svm_vmread64(vcpu->vmcb.virt,exit_info2);
		// Check if we should switch to secondary.
		// Use binary search to reduce searching time complexity.
		while(hi>=lo)
		{
			i32 mid=(lo+hi)>>1;
			noir_hook_page_p nhp=&noir_hook_pages[mid];
			if(gpa>=nhp->orig.phys+page_size)
				lo=mid+1;
			else if(gpa<nhp->orig.phys)
				hi=mid-1;
			else
			{
				noir_npt_manager_p nptm=(noir_npt_manager_p)vcpu->secondary_nptm;
				noir_svm_vmwrite64(vcpu->vmcb.virt,npt_cr3,nptm->ncr3.phys);
				advance=false;
				break;
			}
		}
		if(advance)
		{
			// Execution is outside hooked page.
			// We should switch to primary.
			noir_npt_manager_p nptm=(noir_npt_manager_p)vcpu->primary_nptm;
			noir_svm_vmwrite64(vcpu->vmcb.virt,npt_cr3,nptm->ncr3.phys);
			advance=false;
		}
		// We switched NPT. Thus we should clean VMCB cache state.
		noir_btr((u32*)((ulong_ptr)vcpu->vmcb.virt+vmcb_clean_bits),noir_svm_clean_npt);
		// It is necessary to flush TLB.
		noir_svm_vmwrite8(vcpu->vmcb.virt,tlb_control,nvc_svm_tlb_control_flush_entire);
	}
#endif
	if(advance)
	{
		// Note that SVM won't save the next rip in #NPF.
		// Hence we should advance rip by software analysis.
		// Usually, if #NPF handler goes here, it might be induced by Hardware-Enforced CI.
		// In this regard, we assume this instruction is writing protected page.
		void* instruction=(void*)((ulong_ptr)vcpu->vmcb.virt+guest_instruction_bytes);
		// Determine Long-Mode through CS.L bit.
		u16* cs_attrib=(u16*)((ulong_ptr)vcpu->vmcb.virt+guest_cs_attrib);
		u32 increment=noir_get_instruction_length(instruction,noir_bt(cs_attrib,9));
		// Just increment the rip. Don't emulate a read/write for guest.
		ulong_ptr gip=noir_svm_vmread(vcpu->vmcb.virt,guest_rip);
		gip+=increment;
		noir_svm_vmwrite(vcpu->vmcb.virt,guest_rip,gip);
	}
}

void fastcall nvc_svm_exit_handler(noir_gpr_state_p gpr_state,noir_svm_vcpu_p vcpu)
{
	// Get the linear address of VMCB.
	noir_svm_initial_stack_p loader_stack=(noir_svm_initial_stack_p)((ulong_ptr)vcpu->hv_stack+nvc_stack_size-sizeof(noir_svm_initial_stack));
	// Confirm which vCPU is exiting so that the correct handler is to be invoked...
	if(likely(loader_stack->guest_vmcb_pa==vcpu->vmcb.phys))		// Let branch predictor favor subverted host.
	{
		// Subverted Host is exiting...
		const void* vmcb_va=vcpu->vmcb.virt;
		// Read the Intercept Code.
		i64 intercept_code=noir_svm_vmread64(vmcb_va,exit_code);
		// Determine the group and number of interception.
		u8 code_group=(u8)((intercept_code&0xC00)>>10);
		u16 code_num=(u16)(intercept_code&0x3FF);
		// rax is saved to VMCB, not GPR state.
		gpr_state->rax=noir_svm_vmread(vmcb_va,guest_rax);
		// Set VMCB Cache State as all to be cached, except the Intercept Caching
		// in that TSC Offseting field is one part of this cache.
		if(vcpu->enabled_feature & noir_svm_vmcb_caching)
			noir_svm_vmwrite32(vmcb_va,vmcb_clean_bits,0xffffffff);
		// Set TLB Control to Do-not-Flush
		noir_svm_vmwrite32(vmcb_va,tlb_control,nvc_svm_tlb_control_do_nothing);
		// Check if the interception is due to invalid guest state.
		// Invoke the handler accordingly.
		if(unlikely(intercept_code<0))		// Rare circumstance.
			svm_exit_handler_negative[-intercept_code](gpr_state,vcpu);
		else
			svm_exit_handlers[code_group][code_num](gpr_state,vcpu);
		// Since rax register is operated, save to VMCB.
		noir_svm_vmwrite(vmcb_va,guest_rax,gpr_state->rax);
	}
	else
	{
		// Customizable VM is exiting...
		const void* vmcb_va=loader_stack->custom_vcpu->vmcb.virt;
		;
	}
	// The rax in GPR state should be the physical address of VMCB
	// in order to execute the vmrun instruction properly.
	gpr_state->rax=(ulong_ptr)loader_stack->guest_vmcb_pa;
	noir_svm_vmload((ulong_ptr)loader_stack->guest_vmcb_pa);
}

/*
bool nvc_svm_build_exit_handler()
{
	// Allocate the array of Exit-Handler Group
	svm_exit_handlers=noir_alloc_nonpg_memory(sizeof(void*)*4);
	if(svm_exit_handlers)
	{
		// Allocate arrays of Exit-Handlers
		svm_exit_handlers[0]=noir_alloc_nonpg_memory(noir_svm_maximum_code1*sizeof(void*));
		svm_exit_handlers[1]=noir_alloc_nonpg_memory(noir_svm_maximum_code2*sizeof(void*));
		if(svm_exit_handlers[0] && svm_exit_handlers[1])
		{
			// Initialize it with default-handler.
			// Using stos instruction could accelerate the initialization.
			noir_stosp(svm_exit_handlers[0],(ulong_ptr)nvc_svm_default_handler,noir_svm_maximum_code1);
			noir_stosp(svm_exit_handlers[1],(ulong_ptr)nvc_svm_default_handler,noir_svm_maximum_code2);
		}
		else
		{
			// Allocation failed. Perform cleanup.
			if(svm_exit_handlers[0])noir_free_nonpg_memory(svm_exit_handlers[0]);
			if(svm_exit_handlers[1])noir_free_nonpg_memory(svm_exit_handlers[1]);
			noir_free_nonpg_memory(svm_exit_handlers);
			return false;
		}
		// Zero the group if it is unused.
		svm_exit_handlers[2]=svm_exit_handlers[3]=null;
		// Setup Exit-Handlers
		svm_exit_handlers[0][intercepted_cpuid]=nvc_svm_cpuid_handler;
		svm_exit_handlers[0][intercepted_msr]=nvc_svm_msr_handler;
		svm_exit_handlers[0][intercepted_shutdown]=nvc_svm_shutdown_handler;
		svm_exit_handlers[0][intercepted_vmrun]=nvc_svm_vmrun_handler;
		svm_exit_handlers[0][intercepted_vmmcall]=nvc_svm_vmmcall_handler;
		svm_exit_handlers[0][intercepted_vmload]=nvc_svm_vmload_handler;
		svm_exit_handlers[0][intercepted_vmsave]=nvc_svm_vmsave_handler;
		svm_exit_handlers[0][intercepted_stgi]=nvc_svm_stgi_handler;
		svm_exit_handlers[0][intercepted_clgi]=nvc_svm_clgi_handler;
		svm_exit_handlers[0][intercepted_skinit]=nvc_svm_skinit_handler;
		svm_exit_handlers[1][nested_page_fault-0x400]=nvc_svm_nested_pf_handler;
		// Special CPUID-Handler
		if(hvm_p->options.cpuid_hv_presence)
			nvcp_svm_cpuid_handler=nvc_svm_cpuid_hvp_handler;
		else
			nvcp_svm_cpuid_handler=nvc_svm_cpuid_hvs_handler;
		return true;
	}
	return false;
}

void nvc_svm_teardown_exit_handler()
{
	// Check if Exit Handler Group array is allocated.
	if(svm_exit_handlers)
	{
		// Check if Exit Handler Groups are allocated.
		// Free them accordingly.
		if(svm_exit_handlers[0])noir_free_nonpg_memory(svm_exit_handlers[0]);
		if(svm_exit_handlers[1])noir_free_nonpg_memory(svm_exit_handlers[1]);
		if(svm_exit_handlers[2])noir_free_nonpg_memory(svm_exit_handlers[2]);
		if(svm_exit_handlers[3])noir_free_nonpg_memory(svm_exit_handlers[3]);
		// Free the Exit Handler Group array.
		noir_free_nonpg_memory(svm_exit_handlers);
		// Mark as released.
		svm_exit_handlers=null;
	}
}
*/

void nvc_svm_set_mshv_handler(bool option)
{
	nvcp_svm_cpuid_handler=option?nvc_svm_cpuid_hvp_handler:nvc_svm_cpuid_hvs_handler;
}