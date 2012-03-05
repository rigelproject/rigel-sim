	RegisterBase reg_t = instr->get_sreg0();
	RegisterBase reg_s = instr->get_sreg1();
	RegisterBase reg_d = instr->get_dreg();
	RegisterBase acc_s = instr->get_sacc();


	instr->set_num_read_ports(0);
	switch (instr->get_type()) {
		case I_ADD:
		case I_SUB:
		case I_ADDU:
		case I_SUBU:
		case I_ADDC:
		case I_ADDG:
		case I_ADDCU:
		case I_ADDGU:
		case I_AND:
		case I_OR:
		case I_XOR:
		case I_NOR:
		case I_SLL:
		case I_SRL:
		case I_SRA:
		case I_CEQ:
		case I_CLT:
		case I_CLE:
		case I_CLTU:
		case I_CLEU:
		case I_CEQF:
		case I_CLTF:
		case I_CLTEF:
		case I_CMEQ:
		case I_CMNE:
		case I_STC:
		case I_ATOMXOR:
		case I_ATOMOR:
		case I_ATOMAND:
		case I_ATOMMAX:
		case I_ATOMMIN:
		case I_FADD:
		case I_FSUB:
		case I_FMUL:
		case I_MUL:
		case I_MUL32:
		case I_MUL16:
		case I_MUL16C:
		case I_MUL16G:
		{
			 /* SOURCE REGS REG_S + REG_T */ 
			instr->set_num_read_ports(2);
			if ( !scoreboards[instr->get_core_thread_id()]->check_reg(reg_s.addr, reg_t.addr) ) {
				    goto ExecuteStalledReturn;
			}

 			break;
		}
		case I_LDL:
		case I_TBLOFFSET:
		case I_FRCP:
		case I_FRSQ:
		case I_FABS:
		case I_FMRS:
		case I_FMOV:
		case I_F2I:
		case I_I2F:
		case I_CLZ:
		case I_ZEXTB:
		case I_SEXTB:
		case I_ZEXTS:
		case I_SEXTS:
		{
			 /* SOURCE REGS REG_T */ 
			instr->set_num_read_ports(1);
			if ( !scoreboards[instr->get_core_thread_id()]->check_reg(reg_t.addr) ) {
				    goto ExecuteStalledReturn;
			}

 			break;
		}
		case I_ADDIU:
		case I_SUBIU:
		case I_ANDI:
		case I_ORI:
		case I_XORI:
		{
			 /* SOURCE REGS REG_T */ 
			instr->set_num_read_ports(1);
			if ( !scoreboards[instr->get_core_thread_id()]->check_reg(reg_t.addr) ) {
				    goto ExecuteStalledReturn;
			}

 			break;
		}
		case I_ADDI:
		case I_SUBI:
		case I_PAUSE_LDW:
		case I_LDW:
		case I_GLDW:
		{
			 /* SOURCE REGS REG_T */ 
			instr->set_num_read_ports(1);
			if ( !scoreboards[instr->get_core_thread_id()]->check_reg(reg_t.addr) ) {
				    goto ExecuteStalledReturn;
			}

 			break;
		}
		case I_MVUI:
		case I_STRTOF:
		case I_EVENT:
		case I_PRIO:
		{
			 /* SOURCE REGS REG_D */ 
			instr->set_num_read_ports(1);
			if ( !scoreboards[instr->get_core_thread_id()]->check_reg(reg_d.addr) ) {
				    goto ExecuteStalledReturn;
			}

 			break;
		}
		case I_SLLI:
		case I_SRLI:
		case I_SRAI:
		case I_SLEEP:
		{
			 /* SOURCE REGS REG_T */ 
			instr->set_num_read_ports(1);
			if ( !scoreboards[instr->get_core_thread_id()]->check_reg(reg_t.addr) ) {
				    goto ExecuteStalledReturn;
			}

 			break;
		}
		case I_FR2A:
		{
			 /* SOURCE REGS REG_T */ 
			instr->set_num_read_ports(1);
			if ( !scoreboards[instr->get_core_thread_id()]->check_reg(reg_t.addr) ) {
				    goto ExecuteStalledReturn;
			}

 			break;
		}
		case I_FA2R:
		{
			 /* SOURCE REGS ACC */ 
			instr->set_num_read_ports(1); // Set to 0 if using separate acc_file
			if ( !ac_sbs[instr->get_core_thread_id()]->check_reg(acc_s.addr, false)) {
				    goto ExecuteStalledReturn;
			}

 			break;
		}
		case I_FMADD:
		case I_FMSUB:
		{
			 /* SOURCE REGS ACC + REG_S + REG_T */ 
			instr->set_num_read_ports(3); // Set to 2 if using separate acc_file
			if ( !scoreboards[instr->get_core_thread_id()]->check_reg(reg_s.addr, reg_t.addr) || 
			     !scoreboards[instr->get_core_thread_id()]->check_reg(acc_s.addr)) {
			 /*  !ac_sbs[instr->get_core_thread_id()]->check_reg(acc_s.addr, false)) { // Use this line instead
			                                                                           // if using a separate acc_file*/
				    goto ExecuteStalledReturn;
			}

 			break;
		}
		case I_ATOMDEC:
		case I_ATOMINC:
		case I_ATOMXCHG:
		{
			 /* SOURCE REGS REG_S + REG_T */ 
			instr->set_num_read_ports(2);
			if ( !scoreboards[instr->get_core_thread_id()]->check_reg(reg_s.addr, reg_t.addr) ) {
				    goto ExecuteStalledReturn;
			}

 			break;
		}
		case I_JMPR:
		case I_FLUSH_BCAST:
		case I_LINE_WB:
		case I_LINE_INV:
		case I_LINE_FLUSH:
		{
			 /* SOURCE REGS REG_T */ 
			instr->set_num_read_ports(1);
			if ( !scoreboards[instr->get_core_thread_id()]->check_reg(reg_t.addr) ) {
				    goto ExecuteStalledReturn;
			}

 			break;
		}
		case I_NOP:
		case I_BRK:
		case I_HLT:
		case I_SYNC:
		case I_UNDEF:
		case I_CC_WB:
		case I_CC_INV:
		case I_CC_FLUSH:
		case I_IC_INV:
		case I_MB:
		case I_RFE:
		case I_ABORT:
		{
			 /* NO SOURCE REGS */ 
			instr->set_num_read_ports(0);
			break;
		}
		case I_PREF_L:
		case I_PREF_NGA:
		case I_BCAST_INV:
		case I_SYSCALL:
		case I_PRINTREG:
		case I_START_TIMER:
		case I_STOP_TIMER:
		{
			 /* SOURCE REGS REG_T */ 
			instr->set_num_read_ports(1);
			if ( !scoreboards[instr->get_core_thread_id()]->check_reg(reg_t.addr) ) {
				    goto ExecuteStalledReturn;
			}

 			break;
		}
		case I_LJ:
		case I_JMP:
		{
			 /* NO SOURCE REGS */ 
			instr->set_num_read_ports(0);
			break;
		}
		case I_BEQ:
		case I_BNE:
		{
			 /* SOURCE REGS REG_S + REG_T */ 
			instr->set_num_read_ports(2);
			if ( !scoreboards[instr->get_core_thread_id()]->check_reg(reg_s.addr, reg_t.addr) ) {
				    goto ExecuteStalledReturn;
			}

 			break;
		}
		case I_PREF_B_GC:
		case I_PREF_B_CC:
		case I_BCAST_UPDATE:
		case I_STW:
		case I_GSTW:
		{
			 /* SOURCE REGS REG_S + REG_T */ 
			instr->set_num_read_ports(2);
			if ( !scoreboards[instr->get_core_thread_id()]->check_reg(reg_s.addr, reg_t.addr) ) {
				    goto ExecuteStalledReturn;
			}

 			break;
		}
		case I_BE:
		case I_BN:
		case I_BLT:
		case I_BGT:
		case I_BLE:
		case I_BGE:
		{
			 /* SOURCE REGS REG_T */ 
			instr->set_num_read_ports(1);
			if ( !scoreboards[instr->get_core_thread_id()]->check_reg(reg_t.addr) ) {
				    goto ExecuteStalledReturn;
			}

 			break;
		}
		case I_MFSR:
		{
			 /* SOURCE REGS SP_REG_T */ 
			instr->set_num_read_ports(0);
			if ( !sp_sbs[instr->get_core_thread_id()]->check_reg(reg_t.addr) ) {
				    goto ExecuteStalledReturn;
			}

 			break;
		}
		case I_MTSR:
		{
			 /* GP SOURCE (SP DEST)*/ 
			instr->set_num_read_ports(1);
			if ( !scoreboards[instr->get_core_thread_id()]->check_reg(reg_t.addr) ) {
				    goto ExecuteStalledReturn;
			}

 			break;
		}
		case I_LJL:
		{
			 /* NO SOURCES (LINK_REG DEST) */ 
			instr->set_num_read_ports(0);
			break;
		}
		case I_JAL:
		{
			 /* NO SOURCES (LINK_REG DEST) */ 
			instr->set_num_read_ports(0);
			break;
		}
		case I_JALR:
		{
			 /* SOURCE REGS REG_T (LINK_REG DEST) */ 
			instr->set_num_read_ports(1);
			if ( !scoreboards[instr->get_core_thread_id()]->check_reg(reg_t.addr) ) {
				    goto ExecuteStalledReturn;
			}

 			break;
		}
		case I_ATOMCAS:
		{
			/* (CAS USES THREE REGS) */ 
			instr->set_num_read_ports(3);
			if ( !scoreboards[instr->get_core_thread_id()]->check_reg(reg_t.addr, reg_s.addr) ) {
				    goto ExecuteStalledReturn;
			};
			if ( !scoreboards[instr->get_core_thread_id()]->check_reg(reg_d.addr) ) {
				    goto ExecuteStalledReturn;
			};
			break;
		}
		case I_ATOMADDU:
		{
			/* (CAS USES THREE REGS) */ 
			instr->set_num_read_ports(3);
			if ( !scoreboards[instr->get_core_thread_id()]->check_reg(reg_t.addr, reg_s.addr) ) {
				    goto ExecuteStalledReturn;
			};
			if ( !scoreboards[instr->get_core_thread_id()]->check_reg(reg_d.addr) ) {
				    goto ExecuteStalledReturn;
			};
			break;
		}
		case I_TQ_ENQUEUE:
		{
			/* (TQ REGS: ALL TQ_REGS) */ 
			instr->set_num_read_ports(4);
			using namespace rigel::task_queue; 
			if ( !scoreboards[instr->get_core_thread_id()]->check_reg(TQ_REG0, TQ_REG1) ) {
				    goto ExecuteStalledReturn;
			};
			if ( !scoreboards[instr->get_core_thread_id()]->check_reg(TQ_REG2, TQ_REG3) ) {
				    goto ExecuteStalledReturn;
			};
			break;
		}
		case I_TQ_DEQUEUE:
		{
			/* (TQ REGS: No sources) */ 
			instr->set_num_read_ports(0);
			using namespace rigel::task_queue; 
			break;
		}
		case I_TQ_LOOP:
		{
			/* (TQ REGS: ALL TQ_REGS) */ 
			instr->set_num_read_ports(4);
			using namespace rigel::task_queue; 
			if ( !scoreboards[instr->get_core_thread_id()]->check_reg(TQ_REG0, TQ_REG1) ) {
				    goto ExecuteStalledReturn;
			};
			if ( !scoreboards[instr->get_core_thread_id()]->check_reg(TQ_REG2, TQ_REG3) ) {
				    goto ExecuteStalledReturn;
			};
			break;
		}
		case I_TQ_INIT:
		{
			/* (TQ REGS: TQ_REG0, TQ_REG1 ) */ 
			instr->set_num_read_ports(2);
			using namespace rigel::task_queue; 
			if ( !scoreboards[instr->get_core_thread_id()]->check_reg(TQ_REG0, TQ_REG1) ) {
				    goto ExecuteStalledReturn;
			};
			break;
		}
		case I_TQ_END:
		{
			/* (TQ REGS: No sources) */ 
			instr->set_num_read_ports(0);
			using namespace rigel::task_queue; 
			break;
		}
		case I_VADD:
		{
			/* VECTOR REGS */ 
			uint32_t s0 = instr->get_sreg0().addr;
			uint32_t s1 = instr->get_sreg1().addr;
			uint32_t s2 = instr->get_sreg2().addr;
			uint32_t s3 = instr->get_sreg3().addr;
			uint32_t t0 = instr->get_sreg4().addr;
			uint32_t t1 = instr->get_sreg5().addr;
			uint32_t t2 = instr->get_sreg6().addr;
			uint32_t t3 = instr->get_sreg7().addr;
			if (! instr->get_is_vec_op()) {
			assert(reg_s.addr < 8 && reg_t.addr < 8
				 && "Invalid vector register");
				s0 = (reg_s.addr * 4) + 0;
				s1 = (reg_s.addr * 4) + 1;
				s2 = (reg_s.addr * 4) + 2;
				s3 = (reg_s.addr * 4) + 3;
				t0 = (reg_t.addr * 4) + 0;
				t1 = (reg_t.addr * 4) + 1;
				t2 = (reg_t.addr * 4) + 2;
				t3 = (reg_t.addr * 4) + 3;
				instr->set_regs_vec_s(s0, s1, s2, s3);
				instr->set_regs_vec_t(t0, t1, t2, t3);
				instr->set_is_vec_op();
			}
			if ( !scoreboards[instr->get_core_thread_id()]->check_reg(s0, s1) ) {
				    goto ExecuteStalledReturn;
			};
			if ( !scoreboards[instr->get_core_thread_id()]->check_reg(s2, s3) ) {
				    goto ExecuteStalledReturn;
			};
			if ( !scoreboards[instr->get_core_thread_id()]->check_reg(t0, t1) ) {
				    goto ExecuteStalledReturn;
			};
			if ( !scoreboards[instr->get_core_thread_id()]->check_reg(t2, t3) ) {
				    goto ExecuteStalledReturn;
			};
			break;
		}
		case I_VSUB:
		{
			/* VECTOR REGS */ 
			uint32_t s0 = instr->get_sreg0().addr;
			uint32_t s1 = instr->get_sreg1().addr;
			uint32_t s2 = instr->get_sreg2().addr;
			uint32_t s3 = instr->get_sreg3().addr;
			uint32_t t0 = instr->get_sreg4().addr;
			uint32_t t1 = instr->get_sreg5().addr;
			uint32_t t2 = instr->get_sreg6().addr;
			uint32_t t3 = instr->get_sreg7().addr;
			if (! instr->get_is_vec_op()) {
			assert(reg_s.addr < 8 && reg_t.addr < 8
				 && "Invalid vector register");
				s0 = (reg_s.addr * 4) + 0;
				s1 = (reg_s.addr * 4) + 1;
				s2 = (reg_s.addr * 4) + 2;
				s3 = (reg_s.addr * 4) + 3;
				t0 = (reg_t.addr * 4) + 0;
				t1 = (reg_t.addr * 4) + 1;
				t2 = (reg_t.addr * 4) + 2;
				t3 = (reg_t.addr * 4) + 3;
				instr->set_regs_vec_s(s0, s1, s2, s3);
				instr->set_regs_vec_t(t0, t1, t2, t3);
				instr->set_is_vec_op();
			}
			if ( !scoreboards[instr->get_core_thread_id()]->check_reg(s0, s1) ) {
				    goto ExecuteStalledReturn;
			};
			if ( !scoreboards[instr->get_core_thread_id()]->check_reg(s2, s3) ) {
				    goto ExecuteStalledReturn;
			};
			if ( !scoreboards[instr->get_core_thread_id()]->check_reg(t0, t1) ) {
				    goto ExecuteStalledReturn;
			};
			if ( !scoreboards[instr->get_core_thread_id()]->check_reg(t2, t3) ) {
				    goto ExecuteStalledReturn;
			};
			break;
		}
		case I_VADDI:
		{
			/* VECTOR REGS */ 
			uint32_t s0 = instr->get_sreg0().addr;
			uint32_t s1 = instr->get_sreg1().addr;
			uint32_t s2 = instr->get_sreg2().addr;
			uint32_t s3 = instr->get_sreg3().addr;
			if (! instr->get_is_vec_op()) {
				assert(reg_t.addr < 8
					 && "Invalid vector register");
				s0 = (reg_t.addr * 4) + 0;
				s1 = (reg_t.addr * 4) + 1;
				s2 = (reg_t.addr * 4) + 2;
				s3 = (reg_t.addr * 4) + 3;
				instr->set_regs_vec_s(s0, s1, s2, s3);
				instr->set_regs_vec_t(-1, -1, -1, -1);
				instr->set_is_vec_op();
			}
			if ( !scoreboards[instr->get_core_thread_id()]->check_reg(s0, s1) ) {
				    goto ExecuteStalledReturn;
			};
			if ( !scoreboards[instr->get_core_thread_id()]->check_reg(s2, s3) ) {
				    goto ExecuteStalledReturn;
			};
			break;
		}
		case I_VSUBI:
		{
			/* VECTOR REGS */ 
			uint32_t s0 = instr->get_sreg0().addr;
			uint32_t s1 = instr->get_sreg1().addr;
			uint32_t s2 = instr->get_sreg2().addr;
			uint32_t s3 = instr->get_sreg3().addr;
			if (! instr->get_is_vec_op()) {
				assert(reg_t.addr < 8
					 && "Invalid vector register");
				s0 = (reg_t.addr * 4) + 0;
				s1 = (reg_t.addr * 4) + 1;
				s2 = (reg_t.addr * 4) + 2;
				s3 = (reg_t.addr * 4) + 3;
				instr->set_regs_vec_s(s0, s1, s2, s3);
				instr->set_regs_vec_t(-1, -1, -1, -1);
				instr->set_is_vec_op();
			}
			if ( !scoreboards[instr->get_core_thread_id()]->check_reg(s0, s1) ) {
				    goto ExecuteStalledReturn;
			};
			if ( !scoreboards[instr->get_core_thread_id()]->check_reg(s2, s3) ) {
				    goto ExecuteStalledReturn;
			};
			break;
		}
		case I_VFADD:
		{
			/* VECTOR REGS */ 
			uint32_t s0 = instr->get_sreg0().addr;
			uint32_t s1 = instr->get_sreg1().addr;
			uint32_t s2 = instr->get_sreg2().addr;
			uint32_t s3 = instr->get_sreg3().addr;
			uint32_t t0 = instr->get_sreg4().addr;
			uint32_t t1 = instr->get_sreg5().addr;
			uint32_t t2 = instr->get_sreg6().addr;
			uint32_t t3 = instr->get_sreg7().addr;
			if (! instr->get_is_vec_op()) {
			assert(reg_s.addr < 8 && reg_t.addr < 8
				 && "Invalid vector register");
				s0 = (reg_s.addr * 4) + 0;
				s1 = (reg_s.addr * 4) + 1;
				s2 = (reg_s.addr * 4) + 2;
				s3 = (reg_s.addr * 4) + 3;
				t0 = (reg_t.addr * 4) + 0;
				t1 = (reg_t.addr * 4) + 1;
				t2 = (reg_t.addr * 4) + 2;
				t3 = (reg_t.addr * 4) + 3;
				instr->set_regs_vec_s(s0, s1, s2, s3);
				instr->set_regs_vec_t(t0, t1, t2, t3);
				instr->set_is_vec_op();
			}
			if ( !scoreboards[instr->get_core_thread_id()]->check_reg(s0, s1) ) {
				    goto ExecuteStalledReturn;
			};
			if ( !scoreboards[instr->get_core_thread_id()]->check_reg(s2, s3) ) {
				    goto ExecuteStalledReturn;
			};
			if ( !scoreboards[instr->get_core_thread_id()]->check_reg(t0, t1) ) {
				    goto ExecuteStalledReturn;
			};
			if ( !scoreboards[instr->get_core_thread_id()]->check_reg(t2, t3) ) {
				    goto ExecuteStalledReturn;
			};
			break;
		}
		case I_VFSUB:
		{
			/* VECTOR REGS */ 
			uint32_t s0 = instr->get_sreg0().addr;
			uint32_t s1 = instr->get_sreg1().addr;
			uint32_t s2 = instr->get_sreg2().addr;
			uint32_t s3 = instr->get_sreg3().addr;
			uint32_t t0 = instr->get_sreg4().addr;
			uint32_t t1 = instr->get_sreg5().addr;
			uint32_t t2 = instr->get_sreg6().addr;
			uint32_t t3 = instr->get_sreg7().addr;
			if (! instr->get_is_vec_op()) {
			assert(reg_s.addr < 8 && reg_t.addr < 8
				 && "Invalid vector register");
				s0 = (reg_s.addr * 4) + 0;
				s1 = (reg_s.addr * 4) + 1;
				s2 = (reg_s.addr * 4) + 2;
				s3 = (reg_s.addr * 4) + 3;
				t0 = (reg_t.addr * 4) + 0;
				t1 = (reg_t.addr * 4) + 1;
				t2 = (reg_t.addr * 4) + 2;
				t3 = (reg_t.addr * 4) + 3;
				instr->set_regs_vec_s(s0, s1, s2, s3);
				instr->set_regs_vec_t(t0, t1, t2, t3);
				instr->set_is_vec_op();
			}
			if ( !scoreboards[instr->get_core_thread_id()]->check_reg(s0, s1) ) {
				    goto ExecuteStalledReturn;
			};
			if ( !scoreboards[instr->get_core_thread_id()]->check_reg(s2, s3) ) {
				    goto ExecuteStalledReturn;
			};
			if ( !scoreboards[instr->get_core_thread_id()]->check_reg(t0, t1) ) {
				    goto ExecuteStalledReturn;
			};
			if ( !scoreboards[instr->get_core_thread_id()]->check_reg(t2, t3) ) {
				    goto ExecuteStalledReturn;
			};
			break;
		}
		case I_VFMUL:
		{
			/* VECTOR REGS */ 
			uint32_t s0 = instr->get_sreg0().addr;
			uint32_t s1 = instr->get_sreg1().addr;
			uint32_t s2 = instr->get_sreg2().addr;
			uint32_t s3 = instr->get_sreg3().addr;
			uint32_t t0 = instr->get_sreg4().addr;
			uint32_t t1 = instr->get_sreg5().addr;
			uint32_t t2 = instr->get_sreg6().addr;
			uint32_t t3 = instr->get_sreg7().addr;
			if (! instr->get_is_vec_op()) {
			assert(reg_s.addr < 8 && reg_t.addr < 8
				 && "Invalid vector register");
				s0 = (reg_s.addr * 4) + 0;
				s1 = (reg_s.addr * 4) + 1;
				s2 = (reg_s.addr * 4) + 2;
				s3 = (reg_s.addr * 4) + 3;
				t0 = (reg_t.addr * 4) + 0;
				t1 = (reg_t.addr * 4) + 1;
				t2 = (reg_t.addr * 4) + 2;
				t3 = (reg_t.addr * 4) + 3;
				instr->set_regs_vec_s(s0, s1, s2, s3);
				instr->set_regs_vec_t(t0, t1, t2, t3);
				instr->set_is_vec_op();
			}
			if ( !scoreboards[instr->get_core_thread_id()]->check_reg(s0, s1) ) {
				    goto ExecuteStalledReturn;
			};
			if ( !scoreboards[instr->get_core_thread_id()]->check_reg(s2, s3) ) {
				    goto ExecuteStalledReturn;
			};
			if ( !scoreboards[instr->get_core_thread_id()]->check_reg(t0, t1) ) {
				    goto ExecuteStalledReturn;
			};
			if ( !scoreboards[instr->get_core_thread_id()]->check_reg(t2, t3) ) {
				    goto ExecuteStalledReturn;
			};
			break;
		}
		case I_VLDW:
		{
			/* VECTOR REGS */ 
			uint32_t treg = instr->get_sreg4().addr;
			if (! instr->get_is_vec_op()) {
				treg = reg_t.addr;
				instr->set_regs_vec_t(treg, -1, -1, -1);
				instr->set_is_vec_op();
			}
			if ( !scoreboards[instr->get_core_thread_id()]->check_reg(treg) ) {
				    goto ExecuteStalledReturn;
			};
			break;
		}
		case I_VSTW:
		{
			/* VECTOR REGS */ 
			uint32_t s0 = instr->get_sreg0().addr;
			uint32_t s1 = instr->get_sreg1().addr;
			uint32_t s2 = instr->get_sreg2().addr;
			uint32_t s3 = instr->get_sreg3().addr;
			uint32_t treg = instr->get_sreg4().addr;
			if (! instr->get_is_vec_op()) {
				assert(reg_t.addr < 8
					 && "Invalid vector register");
				s0 = (reg_t.addr * 4) + 0;
				s1 = (reg_t.addr * 4) + 1;
				s2 = (reg_t.addr * 4) + 2;
				s3 = (reg_t.addr * 4) + 3;
				treg = reg_s.addr;
				instr->set_regs_vec_s(s0, s1, s2, s3);
				instr->set_regs_vec_t(treg, -1, -1, -1);
				instr->set_is_vec_op();
			}
			if ( !scoreboards[instr->get_core_thread_id()]->check_reg(s0, s1) ) {
				    goto ExecuteStalledReturn;
			};
			if ( !scoreboards[instr->get_core_thread_id()]->check_reg(s2, s3) ) {
				    goto ExecuteStalledReturn;
			};
			if ( !scoreboards[instr->get_core_thread_id()]->check_reg(treg) ) {
				    goto ExecuteStalledReturn;
			};
			break;
		}
		case I_NULL:
		case I_PRE_DECODE:
		case I_REP_HLT:
		case I_DONE:
		case I_BRANCH_FALL_THROUGH:
		case I_CMNE_NOP:
		case I_CMEQ_NOP:
		{
			 /* NO SOURCE REGS */ 
			instr->set_num_read_ports(0);
			break;
		}

		default: 
		{
			/* Other branches are ignored */
			if (instr->instr_is_branch()) break;
			std::cerr << "Last instr: ";
			instr->dis_print();
			std::cerr << std::endl;
			throw ExitSim("DC: Unknown type for scoreboard read!", 1);
		}
	} /* END OF READ SWITCH */
