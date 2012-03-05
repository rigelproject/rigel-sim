	bool is_lock_reg = false; 
	switch (instr_type) {
		case I_ADD: if (I_ADD == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		case I_SUB: if (I_SUB == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		case I_ADDU: if (I_ADDU == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		case I_SUBU: if (I_SUBU == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		case I_ADDC: if (I_ADDC == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		case I_ADDG: if (I_ADDG == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		case I_ADDCU: if (I_ADDCU == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		case I_ADDGU: if (I_ADDGU == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		case I_AND: if (I_AND == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		case I_OR: if (I_OR == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		case I_XOR: if (I_XOR == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		case I_NOR: if (I_NOR == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		case I_SLL: if (I_SLL == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		case I_SRL: if (I_SRL == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		case I_SRA: if (I_SRA == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		case I_CEQ: if (I_CEQ == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		case I_CLT: if (I_CLT == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		case I_CLE: if (I_CLE == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		case I_CLTU: if (I_CLTU == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		case I_CLEU: if (I_CLEU == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		case I_CEQF: if (I_CEQF == instr_type) { sreg0.SetFloat(); sreg1.SetFloat(); dreg.SetInt(); is_lock_reg = false;}
		case I_CLTF: if (I_CLTF == instr_type) { sreg0.SetFloat(); sreg1.SetFloat(); dreg.SetInt(); is_lock_reg = false;}
		case I_CLTEF: if (I_CLTEF == instr_type) { sreg0.SetFloat(); sreg1.SetFloat(); dreg.SetInt(); is_lock_reg = false;}
		case I_CMEQ: if (I_CMEQ == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		case I_CMNE: if (I_CMNE == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		case I_ATOMXOR: if (I_ATOMXOR == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = true;}
		case I_ATOMOR: if (I_ATOMOR == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = true;}
		case I_ATOMAND: if (I_ATOMAND == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = true;}
		case I_ATOMMAX: if (I_ATOMMAX == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = true;}
		case I_ATOMMIN: if (I_ATOMMIN == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = true;}
		case I_FADD: if (I_FADD == instr_type) { sreg0.SetFloat(); sreg1.SetFloat(); dreg.SetFloat(); is_lock_reg = false;}
		case I_FSUB: if (I_FSUB == instr_type) { sreg0.SetFloat(); sreg1.SetFloat(); dreg.SetFloat(); is_lock_reg = false;}
		case I_FMUL: if (I_FMUL == instr_type) { sreg0.SetFloat(); sreg1.SetFloat(); dreg.SetFloat(); is_lock_reg = false;}
		case I_MUL: if (I_MUL == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		case I_MUL32: if (I_MUL32 == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		case I_MUL16: if (I_MUL16 == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		case I_MUL16C: if (I_MUL16C == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		case I_MUL16G: if (I_MUL16G == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		{
			if ( !scoreboards[instr->get_core_thread_id()]->is_bypass(sreg0.addr)) { sreg0 = get_regfile(instr->get_core_thread_id())->read(sreg0.addr); }
			else { instr->set_bypass_sreg0(); }
			if ( !scoreboards[instr->get_core_thread_id()]->is_bypass(sreg1.addr)) { sreg1 = get_regfile(instr->get_core_thread_id())->read(sreg1.addr); }
			else { instr->set_bypass_sreg1(); }
		
				if (is_lock_reg) scoreboards[instr->get_core_thread_id()]->lockReg(dreg.addr);
				scoreboards[instr->get_core_thread_id()]->get_reg(dreg.addr, instr_type);
				instr->set_sb_get();
				if (is_lock_reg) instr->set_lockReg(dreg.addr);
		
			goto done_regread;
		} 
		case I_LDL: if (I_LDL == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = true;}
		case I_TBLOFFSET: if (I_TBLOFFSET == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		case I_FRCP: if (I_FRCP == instr_type) { sreg0.SetFloat(); sreg1.SetInt(); dreg.SetFloat(); is_lock_reg = false;}
		case I_FRSQ: if (I_FRSQ == instr_type) { sreg0.SetFloat(); sreg1.SetInt(); dreg.SetFloat(); is_lock_reg = false;}
		case I_FABS: if (I_FABS == instr_type) { sreg0.SetFloat(); sreg1.SetInt(); dreg.SetFloat(); is_lock_reg = false;}
		case I_FMRS: if (I_FMRS == instr_type) { sreg0.SetFloat(); sreg1.SetInt(); dreg.SetFloat(); is_lock_reg = false;}
		case I_FMOV: if (I_FMOV == instr_type) { sreg0.SetFloat(); sreg1.SetInt(); dreg.SetFloat(); is_lock_reg = false;}
		case I_F2I: if (I_F2I == instr_type) { sreg0.SetFloat(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		case I_I2F: if (I_I2F == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetFloat(); is_lock_reg = false;}
		case I_CLZ: if (I_CLZ == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		case I_ZEXTB: if (I_ZEXTB == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		case I_SEXTB: if (I_SEXTB == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		case I_ZEXTS: if (I_ZEXTS == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		case I_SEXTS: if (I_SEXTS == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		{
			if ( !scoreboards[instr->get_core_thread_id()]->is_bypass(sreg0.addr)) { sreg0 = get_regfile(instr->get_core_thread_id())->read(sreg0.addr); }
			else { instr->set_bypass_sreg0(); }
		
				if (is_lock_reg) scoreboards[instr->get_core_thread_id()]->lockReg(dreg.addr);
				scoreboards[instr->get_core_thread_id()]->get_reg(dreg.addr, instr_type);
				instr->set_sb_get();
				if (is_lock_reg) instr->set_lockReg(dreg.addr);
		
			goto done_regread;
		} 
		case I_ADDIU: if (I_ADDIU == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		case I_SUBIU: if (I_SUBIU == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		case I_ANDI: if (I_ANDI == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		case I_ORI: if (I_ORI == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		case I_XORI: if (I_XORI == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		{
			if ( !scoreboards[instr->get_core_thread_id()]->is_bypass(sreg0.addr)) { sreg0 = get_regfile(instr->get_core_thread_id())->read(sreg0.addr); }
			else { instr->set_bypass_sreg0(); }
		
				if (is_lock_reg) scoreboards[instr->get_core_thread_id()]->lockReg(dreg.addr);
				scoreboards[instr->get_core_thread_id()]->get_reg(dreg.addr, instr_type);
				instr->set_sb_get();
				if (is_lock_reg) instr->set_lockReg(dreg.addr);
		
			goto done_regread;
		} 
		case I_ADDI: if (I_ADDI == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		case I_SUBI: if (I_SUBI == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		case I_PAUSE_LDW: if (I_PAUSE_LDW == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		case I_LDW: if (I_LDW == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = true;}
		case I_GLDW: if (I_GLDW == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = true;}
		{
			if ( !scoreboards[instr->get_core_thread_id()]->is_bypass(sreg0.addr)) { sreg0 = get_regfile(instr->get_core_thread_id())->read(sreg0.addr); }
			else { instr->set_bypass_sreg0(); }
		
				if (is_lock_reg) scoreboards[instr->get_core_thread_id()]->lockReg(dreg.addr);
				scoreboards[instr->get_core_thread_id()]->get_reg(dreg.addr, instr_type);
				instr->set_sb_get();
				if (is_lock_reg) instr->set_lockReg(dreg.addr);
		
			goto done_regread;
		} 
		case I_MVUI: if (I_MVUI == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		case I_STRTOF: if (I_STRTOF == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		case I_EVENT: if (I_EVENT == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		case I_PRIO: if (I_PRIO == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		{
			if ( !scoreboards[instr->get_core_thread_id()]->is_bypass(sreg0.addr)) { sreg0 = get_regfile(instr->get_core_thread_id())->read(sreg0.addr); }
			else { instr->set_bypass_sreg0(); }
		
				if (is_lock_reg) scoreboards[instr->get_core_thread_id()]->lockReg(dreg.addr);
				scoreboards[instr->get_core_thread_id()]->get_reg(dreg.addr, instr_type);
				instr->set_sb_get();
				if (is_lock_reg) instr->set_lockReg(dreg.addr);
		
			goto done_regread;
		} 
		case I_SLLI: if (I_SLLI == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		case I_SRLI: if (I_SRLI == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		case I_SRAI: if (I_SRAI == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		case I_SLEEP: if (I_SLEEP == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		{
			if ( !scoreboards[instr->get_core_thread_id()]->is_bypass(sreg0.addr)) { sreg0 = get_regfile(instr->get_core_thread_id())->read(sreg0.addr); }
			else { instr->set_bypass_sreg0(); }
		
				if (is_lock_reg) scoreboards[instr->get_core_thread_id()]->lockReg(dreg.addr);
				scoreboards[instr->get_core_thread_id()]->get_reg(dreg.addr, instr_type);
				instr->set_sb_get();
				if (is_lock_reg) instr->set_lockReg(dreg.addr);
		
			goto done_regread;
		} 
		case I_FR2A: if (I_FR2A == instr_type) { dacc.SetFloat(); sreg0.SetFloat(); is_lock_reg = false;}
   {
			if ( !scoreboards[instr->get_core_thread_id()]->is_bypass(sreg0.addr)) { sreg0 = get_regfile(instr->get_core_thread_id())->read(sreg0.addr); }
 		else { instr->set_bypass_sreg0(); }
  		if (is_lock_reg) ac_sbs[instr->get_core_thread_id()]->lockReg(dacc.addr,false);
			ac_sbs[instr->get_core_thread_id()]->get_reg(dacc.addr, instr_type,false);
			instr->set_sb_get();
			if (is_lock_reg) instr->set_lockReg(dacc.addr);
 		goto done_regread;
		}
		case I_FA2R: if (I_FA2R == instr_type) { sacc.SetFloat(); dreg.SetFloat(); is_lock_reg = false;}
   {
			if ( !ac_sbs[instr->get_core_thread_id()]->is_bypass(sacc.addr))
     {
       sacc = get_accumulator_regfile(instr->get_core_thread_id())->read(sacc.addr,false);
     }
			else { instr->set_bypass_sacc(); }
			if (is_lock_reg) scoreboards[instr->get_core_thread_id()]->lockReg(dreg.addr);
			scoreboards[instr->get_core_thread_id()]->get_reg(dreg.addr, instr_type);
			instr->set_sb_get();
			if (is_lock_reg) instr->set_lockReg(dreg.addr);
			goto done_regread;
		} 
		case I_FMADD: if (I_FMADD == instr_type) { dacc.SetFloat(); sacc.SetFloat(); sreg0.SetFloat(); sreg1.SetFloat(); dreg.SetFloat(); is_lock_reg = false;}
		case I_FMSUB: if (I_FMSUB == instr_type) { dacc.SetFloat(); sacc.SetFloat(); sreg0.SetFloat(); sreg1.SetFloat(); dreg.SetFloat(); is_lock_reg = false;}
   { 
			if ( !scoreboards[instr->get_core_thread_id()]->is_bypass(sreg0.addr))
     { 
        sreg0 = get_regfile(instr->get_core_thread_id())->read(sreg0.addr); 
     }
			else { instr->set_bypass_sreg0(); }
			if ( !scoreboards[instr->get_core_thread_id()]->is_bypass(sreg1.addr)) 
     { 
        sreg1 = get_regfile(instr->get_core_thread_id())->read(sreg1.addr); 
     }
			else { instr->set_bypass_sreg1(); }
			if ( !scoreboards[instr->get_core_thread_id()]->is_bypass(sacc.addr)) 
     { 
        sacc = get_regfile(instr->get_core_thread_id())->read(sacc.addr,true); 
     }
     /* Uncomment for separate acc_file */
			/*if ( !ac_sbs[instr->get_core_thread_id()]->is_bypass(sacc.addr)) 
     { 
        sacc = get_accumulator_regfile(instr->get_core_thread_id())->read(sacc.addr); 
     }*/
			else { instr->set_bypass_sacc(); }

 		if (is_lock_reg) scoreboards[instr->get_core_thread_id()]->lockReg(dacc.addr, true);
 		scoreboards[instr->get_core_thread_id()]->get_reg(dacc.addr, instr_type,true);
     /* Uncomment for separate acc_file */
 		/*if (is_lock_reg) ac_sbs[instr->get_core_thread_id()]->lockReg(dacc.addr, false);
 		ac_sbs[instr->get_core_thread_id()]->get_reg(dacc.addr, instr_type,false);*/
			instr->set_sb_get();
			if (is_lock_reg) instr->set_lockReg(dacc.addr);
			goto done_regread;
		} 
		case I_ATOMDEC: if (I_ATOMDEC == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = true;}
		case I_ATOMINC: if (I_ATOMINC == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = true;}
		case I_ATOMXCHG: if (I_ATOMXCHG == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = true;}
		{
			if ( !scoreboards[instr->get_core_thread_id()]->is_bypass(sreg0.addr)) { sreg0 = get_regfile(instr->get_core_thread_id())->read(sreg0.addr); }
			else { instr->set_bypass_sreg0(); }
			if ( !scoreboards[instr->get_core_thread_id()]->is_bypass(sreg1.addr)) { sreg1 = get_regfile(instr->get_core_thread_id())->read(sreg1.addr); }
			else { instr->set_bypass_sreg1(); }
		
				if (is_lock_reg) scoreboards[instr->get_core_thread_id()]->lockReg(dreg.addr);
				scoreboards[instr->get_core_thread_id()]->get_reg(dreg.addr, instr_type);
				instr->set_sb_get();
				if (is_lock_reg) instr->set_lockReg(dreg.addr);
		
			goto done_regread;
		} 
		case I_JMPR: if (I_JMPR == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		case I_FLUSH_BCAST: if (I_FLUSH_BCAST == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		case I_LINE_WB: if (I_LINE_WB == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		case I_LINE_INV: if (I_LINE_INV == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		case I_LINE_FLUSH: if (I_LINE_FLUSH == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		{
			if ( !scoreboards[instr->get_core_thread_id()]->is_bypass(sreg0.addr)) { sreg0 = get_regfile(instr->get_core_thread_id())->read(sreg0.addr); }
			else { instr->set_bypass_sreg0(); }
			goto done_regread;
		} 
		case I_NOP: if (I_NOP == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		case I_BRK: if (I_BRK == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		case I_HLT: if (I_HLT == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		case I_SYNC: if (I_SYNC == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		case I_UNDEF: if (I_UNDEF == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		case I_CC_WB: if (I_CC_WB == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		case I_CC_INV: if (I_CC_INV == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		case I_CC_FLUSH: if (I_CC_FLUSH == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		case I_IC_INV: if (I_IC_INV == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		case I_MB: if (I_MB == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		case I_RFE: if (I_RFE == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		case I_ABORT: if (I_ABORT == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		{
			 /* NO DEST/SOURCE REGS */ 
			goto done_regread;
		}
		case I_PREF_L: if (I_PREF_L == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		case I_PREF_NGA: if (I_PREF_NGA == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		case I_BCAST_INV: if (I_BCAST_INV == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		case I_SYSCALL: if (I_SYSCALL == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		case I_PRINTREG: if (I_PRINTREG == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		case I_START_TIMER: if (I_START_TIMER == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		case I_STOP_TIMER: if (I_STOP_TIMER == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		{
			if ( !scoreboards[instr->get_core_thread_id()]->is_bypass(sreg0.addr)) { sreg0 = get_regfile(instr->get_core_thread_id())->read(sreg0.addr); }
			else { instr->set_bypass_sreg0(); }
			goto done_regread;
		} 
		case I_LJ: if (I_LJ == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		case I_JMP: if (I_JMP == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		{
			 /* NO DEST/SOURCE REGS */ 
			goto done_regread;
		}
		case I_BEQ: if (I_BEQ == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		case I_BNE: if (I_BNE == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		{
			if ( !scoreboards[instr->get_core_thread_id()]->is_bypass(sreg0.addr)) { sreg0 = get_regfile(instr->get_core_thread_id())->read(sreg0.addr); }
			else { instr->set_bypass_sreg0(); }
			if ( !scoreboards[instr->get_core_thread_id()]->is_bypass(sreg1.addr)) { sreg1 = get_regfile(instr->get_core_thread_id())->read(sreg1.addr); }
			else { instr->set_bypass_sreg1(); }
		
		
			goto done_regread;
		} 
		case I_PREF_B_GC: if (I_PREF_B_GC == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		case I_PREF_B_CC: if (I_PREF_B_CC == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		case I_BCAST_UPDATE: if (I_BCAST_UPDATE == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		case I_STW: if (I_STW == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		case I_GSTW: if (I_GSTW == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		{
			if ( !scoreboards[instr->get_core_thread_id()]->is_bypass(sreg0.addr)) { sreg0 = get_regfile(instr->get_core_thread_id())->read(sreg0.addr); }
			else { instr->set_bypass_sreg0(); }
			if ( !scoreboards[instr->get_core_thread_id()]->is_bypass(sreg1.addr)) { sreg1 = get_regfile(instr->get_core_thread_id())->read(sreg1.addr); }
			else { instr->set_bypass_sreg1(); }
		
		
			goto done_regread;
		} 
		case I_BE: if (I_BE == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		case I_BN: if (I_BN == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		case I_BLT: if (I_BLT == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		case I_BGT: if (I_BGT == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		case I_BLE: if (I_BLE == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		case I_BGE: if (I_BGE == instr_type) { sreg0.SetInt(); sreg1.SetInt(); dreg.SetInt(); is_lock_reg = false;}
		{
			if ( !scoreboards[instr->get_core_thread_id()]->is_bypass(sreg0.addr)) { sreg0 = get_regfile(instr->get_core_thread_id())->read(sreg0.addr); }
			else { instr->set_bypass_sreg0(); }
			goto done_regread;
		} 
		case I_MFSR:
		{
			if ( !sp_sbs[instr->get_core_thread_id()]->is_bypass(sreg0.addr)) { sreg0 = sprfs[instr->get_core_thread_id()]->read(sreg0.addr); }
			else { instr->set_bypass_sreg0(); }
		
				if (is_lock_reg) scoreboards[instr->get_core_thread_id()]->lockReg(dreg.addr);
				scoreboards[instr->get_core_thread_id()]->get_reg(dreg.addr, instr_type);
				instr->set_sb_get();
				if (is_lock_reg) instr->set_lockReg(dreg.addr);
		
			goto done_regread;
		} 
		case I_MTSR:
		{
			if ( !scoreboards[instr->get_core_thread_id()]->is_bypass(sreg0.addr)) { sreg0 = get_regfile(instr->get_core_thread_id())->read(sreg0.addr); }
			else { instr->set_bypass_sreg0(); }
		
				if (is_lock_reg) sp_sbs[instr->get_core_thread_id()]->lockReg(dreg.addr);
				sp_sbs[instr->get_core_thread_id()]->get_reg(dreg.addr, instr_type);
				instr->set_sb_get();
				if (is_lock_reg) instr->set_lockReg(dreg.addr);
		
			goto done_regread;
		} 
		case I_LJL:
		{
			using namespace rigel::regs;
				if (is_lock_reg) scoreboards[instr->get_core_thread_id()]->lockReg(LINK_REG);
				scoreboards[instr->get_core_thread_id()]->get_reg(LINK_REG, instr_type);
				instr->set_sb_get();
				if (is_lock_reg) instr->set_lockReg(LINK_REG);
		
			goto done_regread;
		} 
		case I_JAL:
		{
			using namespace rigel::regs;
				if (is_lock_reg) scoreboards[instr->get_core_thread_id()]->lockReg(LINK_REG);
				scoreboards[instr->get_core_thread_id()]->get_reg(LINK_REG, instr_type);
				instr->set_sb_get();
				if (is_lock_reg) instr->set_lockReg(LINK_REG);
		
			goto done_regread;
		} 
		case I_JALR:
		{
			if (!scoreboards[instr->get_core_thread_id()]->is_bypass(sreg0.addr)) { sreg0 = get_regfile(instr->get_core_thread_id())->read(sreg0.addr); }
			else { instr->set_bypass_sreg0(); }
		
			using namespace rigel::regs;
				if (is_lock_reg) scoreboards[instr->get_core_thread_id()]->lockReg(LINK_REG);
				scoreboards[instr->get_core_thread_id()]->get_reg(LINK_REG, instr_type);
				instr->set_sb_get();
				if (is_lock_reg) instr->set_lockReg(LINK_REG);
		
			goto done_regread;
		} 
		case I_STC:
		{
		
			if ( !scoreboards[instr->get_core_thread_id()]->is_bypass(sreg0.addr)) { sreg0 = get_regfile(instr->get_core_thread_id())->read(sreg0.addr); }
			else { instr->set_bypass_sreg0(); }
			if ( !scoreboards[instr->get_core_thread_id()]->is_bypass(sreg1.addr)) { sreg1 = get_regfile(instr->get_core_thread_id())->read(sreg1.addr); }
			else { instr->set_bypass_sreg1(); }
		
				instr->set_sb_get();
				scoreboards[instr->get_core_thread_id()]->lockReg(dreg.addr);
				scoreboards[instr->get_core_thread_id()]->get_reg(dreg.addr, instr_type);
				instr->set_lockReg(dreg.addr);
		
			goto done_regread;
		} 
		case I_ATOMCAS: is_lock_reg = true;
		{
			if ( !scoreboards[instr->get_core_thread_id()]->is_bypass(sreg0.addr)) { sreg0 = get_regfile(instr->get_core_thread_id())->read(sreg0.addr); }
			else { instr->set_bypass_sreg0(); }
			if ( !scoreboards[instr->get_core_thread_id()]->is_bypass(sreg1.addr)) { sreg1 = get_regfile(instr->get_core_thread_id())->read(sreg1.addr); }
			else { instr->set_bypass_sreg1(); }
			if ( !scoreboards[instr->get_core_thread_id()]->is_bypass(dreg.addr)) { sreg2 = get_regfile(instr->get_core_thread_id())->read(dreg.addr); }
			else { instr->set_bypass_sreg2(); }
		
				if (is_lock_reg) scoreboards[instr->get_core_thread_id()]->lockReg(dreg.addr);
				scoreboards[instr->get_core_thread_id()]->get_reg(dreg.addr, instr_type);
				instr->set_sb_get();
				if (is_lock_reg) instr->set_lockReg(dreg.addr);
		
			goto done_regread;
		} 
		case I_ATOMADDU: is_lock_reg = true;
		{
			if ( !scoreboards[instr->get_core_thread_id()]->is_bypass(sreg0.addr)) { sreg0 = get_regfile(instr->get_core_thread_id())->read(sreg0.addr); }
			else { instr->set_bypass_sreg0(); }
			if ( !scoreboards[instr->get_core_thread_id()]->is_bypass(sreg1.addr)) { sreg1 = get_regfile(instr->get_core_thread_id())->read(sreg1.addr); }
			else { instr->set_bypass_sreg1(); }
			if ( !scoreboards[instr->get_core_thread_id()]->is_bypass(dreg.addr)) { sreg2 = get_regfile(instr->get_core_thread_id())->read(dreg.addr); }
			else { instr->set_bypass_sreg2(); }
		
				if (is_lock_reg) scoreboards[instr->get_core_thread_id()]->lockReg(dreg.addr);
				scoreboards[instr->get_core_thread_id()]->get_reg(dreg.addr, instr_type);
				instr->set_sb_get();
				if (is_lock_reg) instr->set_lockReg(dreg.addr);
		
			goto done_regread;
		} 
		case I_TQ_ENQUEUE:
		{
			using namespace rigel::task_queue; 
			if ( !scoreboards[instr->get_core_thread_id()]->is_bypass(TQ_REG0)) { sreg0 = get_regfile(instr->get_core_thread_id())->read(TQ_REG0); }
			else { instr->set_bypass_sreg0(); }
			if ( !scoreboards[instr->get_core_thread_id()]->is_bypass(TQ_REG1)) { sreg1 = get_regfile(instr->get_core_thread_id())->read(TQ_REG1); }
			else { instr->set_bypass_sreg1(); }
			if ( !scoreboards[instr->get_core_thread_id()]->is_bypass(TQ_REG2)) { sreg2 = get_regfile(instr->get_core_thread_id())->read(TQ_REG2); }
			else { instr->set_bypass_sreg2(); }
			if ( !scoreboards[instr->get_core_thread_id()]->is_bypass(TQ_REG3)) { sreg3 = get_regfile(instr->get_core_thread_id())->read(TQ_REG3); }
			else { instr->set_bypass_sreg3(); }
		
				scoreboards[instr->get_core_thread_id()]->lockReg(TQ_RET_REG);
				scoreboards[instr->get_core_thread_id()]->get_reg(TQ_RET_REG, instr_type);
				instr->set_sb_get();
				instr->set_lockReg(TQ_RET_REG);
		
			goto done_regread;
		} 
		case I_TQ_DEQUEUE:
		{
			using namespace rigel::task_queue; 
		
				instr->set_sb_get();
				scoreboards[instr->get_core_thread_id()]->lockReg(TQ_RET_REG);
				scoreboards[instr->get_core_thread_id()]->lockReg(TQ_REG0);
				scoreboards[instr->get_core_thread_id()]->lockReg(TQ_REG1);
				scoreboards[instr->get_core_thread_id()]->lockReg(TQ_REG2);
				scoreboards[instr->get_core_thread_id()]->lockReg(TQ_REG3);
				scoreboards[instr->get_core_thread_id()]->get_reg(TQ_RET_REG, instr_type);
				scoreboards[instr->get_core_thread_id()]->get_reg(TQ_REG0, instr_type);
				scoreboards[instr->get_core_thread_id()]->get_reg(TQ_REG1, instr_type);
				scoreboards[instr->get_core_thread_id()]->get_reg(TQ_REG2, instr_type);
				scoreboards[instr->get_core_thread_id()]->get_reg(TQ_REG3, instr_type);
				instr->set_lockReg(TQ_RET_REG);
		
			goto done_regread;
		} 
		case I_TQ_LOOP:
		{
			using namespace rigel::task_queue; 
			if ( !scoreboards[instr->get_core_thread_id()]->is_bypass(TQ_REG0)) { sreg0 = get_regfile(instr->get_core_thread_id())->read(TQ_REG0); }
			else { instr->set_bypass_sreg0(); }
			if ( !scoreboards[instr->get_core_thread_id()]->is_bypass(TQ_REG1)) { sreg1 = get_regfile(instr->get_core_thread_id())->read(TQ_REG1); }
			else { instr->set_bypass_sreg1(); }
			if ( !scoreboards[instr->get_core_thread_id()]->is_bypass(TQ_REG2)) { sreg2 = get_regfile(instr->get_core_thread_id())->read(TQ_REG2); }
			else { instr->set_bypass_sreg2(); }
			if ( !scoreboards[instr->get_core_thread_id()]->is_bypass(TQ_REG3)) { sreg3 = get_regfile(instr->get_core_thread_id())->read(TQ_REG3); }
			else { instr->set_bypass_sreg3(); }
		
				scoreboards[instr->get_core_thread_id()]->lockReg(TQ_RET_REG);
				scoreboards[instr->get_core_thread_id()]->get_reg(TQ_RET_REG, instr_type);
				instr->set_sb_get();
				instr->set_lockReg(TQ_RET_REG);
		
			goto done_regread;
		} 
		case I_TQ_INIT:
		{
			using namespace rigel::task_queue; 
			if ( !scoreboards[instr->get_core_thread_id()]->is_bypass(TQ_REG0)) { sreg0 = get_regfile(instr->get_core_thread_id())->read(TQ_REG0); }
			else { instr->set_bypass_sreg0(); }
			if ( !scoreboards[instr->get_core_thread_id()]->is_bypass(TQ_REG1)) { sreg1 = get_regfile(instr->get_core_thread_id())->read(TQ_REG1); }
			else { instr->set_bypass_sreg1(); }
		
				scoreboards[instr->get_core_thread_id()]->lockReg(TQ_RET_REG);
				scoreboards[instr->get_core_thread_id()]->get_reg(TQ_RET_REG, instr_type);
				instr->set_sb_get();
				instr->set_lockReg(TQ_RET_REG);
		
			goto done_regread;
		} 
		case I_TQ_END:
		{
			using namespace rigel::task_queue; 
				scoreboards[instr->get_core_thread_id()]->lockReg(TQ_RET_REG);
				scoreboards[instr->get_core_thread_id()]->get_reg(TQ_RET_REG, instr_type);
				instr->set_sb_get();
				instr->set_lockReg(TQ_RET_REG);
		
			goto done_regread;
		} 
		case I_VADD:
		{
			uint32_t s0 = instr->get_sreg0().addr;
			uint32_t s1 = instr->get_sreg1().addr;
			uint32_t s2 = instr->get_sreg2().addr;
			uint32_t s3 = instr->get_sreg3().addr;
			if ( !scoreboards[instr->get_core_thread_id()]->is_bypass(s0)) { sreg0 = get_regfile(instr->get_core_thread_id())->read(s0); }
			else { instr->set_bypass_sreg0(); }
			if ( !scoreboards[instr->get_core_thread_id()]->is_bypass(s1)) { sreg1 = get_regfile(instr->get_core_thread_id())->read(s1); }
			else { instr->set_bypass_sreg1(); }
			if ( !scoreboards[instr->get_core_thread_id()]->is_bypass(s2)) { sreg2 = get_regfile(instr->get_core_thread_id())->read(s2); }
			else { instr->set_bypass_sreg2(); }
			if ( !scoreboards[instr->get_core_thread_id()]->is_bypass(s3)) { sreg3 = get_regfile(instr->get_core_thread_id())->read(s3); }
			else { instr->set_bypass_sreg3(); }
			uint32_t s4 = instr->get_sreg4().addr;
			uint32_t s5 = instr->get_sreg5().addr;
			uint32_t s6 = instr->get_sreg6().addr;
			uint32_t s7 = instr->get_sreg7().addr;
			if ( !scoreboards[instr->get_core_thread_id()]->is_bypass(s4)) { sreg4 = get_regfile(instr->get_core_thread_id())->read(s4); }
			else { instr->set_bypass_sreg4(); }
			if ( !scoreboards[instr->get_core_thread_id()]->is_bypass(s5)) { sreg5 = get_regfile(instr->get_core_thread_id())->read(s5); }
			else { instr->set_bypass_sreg5(); }
			if ( !scoreboards[instr->get_core_thread_id()]->is_bypass(s6)) { sreg6 = get_regfile(instr->get_core_thread_id())->read(s6); }
			else { instr->set_bypass_sreg6(); }
			if ( scoreboards[instr->get_core_thread_id()]->is_bypass(s7)) { sreg7 = get_regfile(instr->get_core_thread_id())->read(s7); }
			else { instr->set_bypass_sreg7(); }
		
			uint32_t d0 = instr->get_dreg().addr;
			uint32_t d1 = instr->get_dreg1().addr;
			uint32_t d2 = instr->get_dreg2().addr;
			uint32_t d3 = instr->get_dreg3().addr;
// TODO: We may want to lock these going forward....
				scoreboards[instr->get_core_thread_id()]->get_reg(d0, instr_type);
				scoreboards[instr->get_core_thread_id()]->get_reg(d1, instr_type);
				scoreboards[instr->get_core_thread_id()]->get_reg(d2, instr_type);
				scoreboards[instr->get_core_thread_id()]->get_reg(d3, instr_type);
				instr->set_sb_get();
		
			goto done_regread;
		} 
		case I_VSUB:
		{
			uint32_t s0 = instr->get_sreg0().addr;
			uint32_t s1 = instr->get_sreg1().addr;
			uint32_t s2 = instr->get_sreg2().addr;
			uint32_t s3 = instr->get_sreg3().addr;
			if ( !scoreboards[instr->get_core_thread_id()]->is_bypass(s0)) { sreg0 = get_regfile(instr->get_core_thread_id())->read(s0); }
			else { instr->set_bypass_sreg0(); }
			if ( !scoreboards[instr->get_core_thread_id()]->is_bypass(s1)) { sreg1 = get_regfile(instr->get_core_thread_id())->read(s1); }
			else { instr->set_bypass_sreg1(); }
			if ( !scoreboards[instr->get_core_thread_id()]->is_bypass(s2)) { sreg2 = get_regfile(instr->get_core_thread_id())->read(s2); }
			else { instr->set_bypass_sreg2(); }
			if ( !scoreboards[instr->get_core_thread_id()]->is_bypass(s3)) { sreg3 = get_regfile(instr->get_core_thread_id())->read(s3); }
			else { instr->set_bypass_sreg3(); }
			uint32_t s4 = instr->get_sreg4().addr;
			uint32_t s5 = instr->get_sreg5().addr;
			uint32_t s6 = instr->get_sreg6().addr;
			uint32_t s7 = instr->get_sreg7().addr;
			if ( !scoreboards[instr->get_core_thread_id()]->is_bypass(s4)) { sreg4 = get_regfile(instr->get_core_thread_id())->read(s4); }
			else { instr->set_bypass_sreg4(); }
			if ( !scoreboards[instr->get_core_thread_id()]->is_bypass(s5)) { sreg5 = get_regfile(instr->get_core_thread_id())->read(s5); }
			else { instr->set_bypass_sreg5(); }
			if ( !scoreboards[instr->get_core_thread_id()]->is_bypass(s6)) { sreg6 = get_regfile(instr->get_core_thread_id())->read(s6); }
			else { instr->set_bypass_sreg6(); }
			if ( scoreboards[instr->get_core_thread_id()]->is_bypass(s7)) { sreg7 = get_regfile(instr->get_core_thread_id())->read(s7); }
			else { instr->set_bypass_sreg7(); }
		
			uint32_t d0 = instr->get_dreg().addr;
			uint32_t d1 = instr->get_dreg1().addr;
			uint32_t d2 = instr->get_dreg2().addr;
			uint32_t d3 = instr->get_dreg3().addr;
// TODO: We may want to lock these going forward....
				scoreboards[instr->get_core_thread_id()]->get_reg(d0, instr_type);
				scoreboards[instr->get_core_thread_id()]->get_reg(d1, instr_type);
				scoreboards[instr->get_core_thread_id()]->get_reg(d2, instr_type);
				scoreboards[instr->get_core_thread_id()]->get_reg(d3, instr_type);
				instr->set_sb_get();
		
			goto done_regread;
		} 
		case I_VADDI:
		{
			uint32_t s0 = instr->get_sreg0().addr;
			uint32_t s1 = instr->get_sreg1().addr;
			uint32_t s2 = instr->get_sreg2().addr;
			uint32_t s3 = instr->get_sreg3().addr;
			if ( !scoreboards[instr->get_core_thread_id()]->is_bypass(s0)) { sreg0 = get_regfile(instr->get_core_thread_id())->read(s0); }
			else { instr->set_bypass_sreg0(); }
			if ( !scoreboards[instr->get_core_thread_id()]->is_bypass(s1)) { sreg1 = get_regfile(instr->get_core_thread_id())->read(s1); }
			else { instr->set_bypass_sreg1(); }
			if ( !scoreboards[instr->get_core_thread_id()]->is_bypass(s2)) { sreg2 = get_regfile(instr->get_core_thread_id())->read(s2); }
			else { instr->set_bypass_sreg2(); }
			if ( !scoreboards[instr->get_core_thread_id()]->is_bypass(s3)) { sreg3 = get_regfile(instr->get_core_thread_id())->read(s3); }
			else { instr->set_bypass_sreg3(); }
		
			uint32_t d0 = instr->get_dreg().addr;
			uint32_t d1 = instr->get_dreg1().addr;
			uint32_t d2 = instr->get_dreg2().addr;
			uint32_t d3 = instr->get_dreg3().addr;
// TODO: We may want to lock these going forward....
				scoreboards[instr->get_core_thread_id()]->get_reg(d0, instr_type);
				scoreboards[instr->get_core_thread_id()]->get_reg(d1, instr_type);
				scoreboards[instr->get_core_thread_id()]->get_reg(d2, instr_type);
				scoreboards[instr->get_core_thread_id()]->get_reg(d3, instr_type);
				instr->set_sb_get();
		
			goto done_regread;
		} 
		case I_VSUBI:
		{
			uint32_t s0 = instr->get_sreg0().addr;
			uint32_t s1 = instr->get_sreg1().addr;
			uint32_t s2 = instr->get_sreg2().addr;
			uint32_t s3 = instr->get_sreg3().addr;
			if ( !scoreboards[instr->get_core_thread_id()]->is_bypass(s0)) { sreg0 = get_regfile(instr->get_core_thread_id())->read(s0); }
			else { instr->set_bypass_sreg0(); }
			if ( !scoreboards[instr->get_core_thread_id()]->is_bypass(s1)) { sreg1 = get_regfile(instr->get_core_thread_id())->read(s1); }
			else { instr->set_bypass_sreg1(); }
			if ( !scoreboards[instr->get_core_thread_id()]->is_bypass(s2)) { sreg2 = get_regfile(instr->get_core_thread_id())->read(s2); }
			else { instr->set_bypass_sreg2(); }
			if ( !scoreboards[instr->get_core_thread_id()]->is_bypass(s3)) { sreg3 = get_regfile(instr->get_core_thread_id())->read(s3); }
			else { instr->set_bypass_sreg3(); }
		
			uint32_t d0 = instr->get_dreg().addr;
			uint32_t d1 = instr->get_dreg1().addr;
			uint32_t d2 = instr->get_dreg2().addr;
			uint32_t d3 = instr->get_dreg3().addr;
// TODO: We may want to lock these going forward....
				scoreboards[instr->get_core_thread_id()]->get_reg(d0, instr_type);
				scoreboards[instr->get_core_thread_id()]->get_reg(d1, instr_type);
				scoreboards[instr->get_core_thread_id()]->get_reg(d2, instr_type);
				scoreboards[instr->get_core_thread_id()]->get_reg(d3, instr_type);
				instr->set_sb_get();
		
			goto done_regread;
		} 
		case I_VFADD:
		{
			uint32_t s0 = instr->get_sreg0().addr;
			uint32_t s1 = instr->get_sreg1().addr;
			uint32_t s2 = instr->get_sreg2().addr;
			uint32_t s3 = instr->get_sreg3().addr;
			if ( !scoreboards[instr->get_core_thread_id()]->is_bypass(s0)) { sreg0 = get_regfile(instr->get_core_thread_id())->read(s0); }
			else { instr->set_bypass_sreg0(); }
			if ( !scoreboards[instr->get_core_thread_id()]->is_bypass(s1)) { sreg1 = get_regfile(instr->get_core_thread_id())->read(s1); }
			else { instr->set_bypass_sreg1(); }
			if ( !scoreboards[instr->get_core_thread_id()]->is_bypass(s2)) { sreg2 = get_regfile(instr->get_core_thread_id())->read(s2); }
			else { instr->set_bypass_sreg2(); }
			if ( !scoreboards[instr->get_core_thread_id()]->is_bypass(s3)) { sreg3 = get_regfile(instr->get_core_thread_id())->read(s3); }
			else { instr->set_bypass_sreg3(); }
			uint32_t s4 = instr->get_sreg4().addr;
			uint32_t s5 = instr->get_sreg5().addr;
			uint32_t s6 = instr->get_sreg6().addr;
			uint32_t s7 = instr->get_sreg7().addr;
			if ( !scoreboards[instr->get_core_thread_id()]->is_bypass(s4)) { sreg4 = get_regfile(instr->get_core_thread_id())->read(s4); }
			else { instr->set_bypass_sreg4(); }
			if ( !scoreboards[instr->get_core_thread_id()]->is_bypass(s5)) { sreg5 = get_regfile(instr->get_core_thread_id())->read(s5); }
			else { instr->set_bypass_sreg5(); }
			if ( !scoreboards[instr->get_core_thread_id()]->is_bypass(s6)) { sreg6 = get_regfile(instr->get_core_thread_id())->read(s6); }
			else { instr->set_bypass_sreg6(); }
			if ( scoreboards[instr->get_core_thread_id()]->is_bypass(s7)) { sreg7 = get_regfile(instr->get_core_thread_id())->read(s7); }
			else { instr->set_bypass_sreg7(); }
		
			uint32_t d0 = instr->get_dreg().addr;
			uint32_t d1 = instr->get_dreg1().addr;
			uint32_t d2 = instr->get_dreg2().addr;
			uint32_t d3 = instr->get_dreg3().addr;
// TODO: We may want to lock these going forward....
				scoreboards[instr->get_core_thread_id()]->get_reg(d0, instr_type);
				scoreboards[instr->get_core_thread_id()]->get_reg(d1, instr_type);
				scoreboards[instr->get_core_thread_id()]->get_reg(d2, instr_type);
				scoreboards[instr->get_core_thread_id()]->get_reg(d3, instr_type);
				instr->set_sb_get();
		
			goto done_regread;
		} 
		case I_VFSUB:
		{
			uint32_t s0 = instr->get_sreg0().addr;
			uint32_t s1 = instr->get_sreg1().addr;
			uint32_t s2 = instr->get_sreg2().addr;
			uint32_t s3 = instr->get_sreg3().addr;
			if ( !scoreboards[instr->get_core_thread_id()]->is_bypass(s0)) { sreg0 = get_regfile(instr->get_core_thread_id())->read(s0); }
			else { instr->set_bypass_sreg0(); }
			if ( !scoreboards[instr->get_core_thread_id()]->is_bypass(s1)) { sreg1 = get_regfile(instr->get_core_thread_id())->read(s1); }
			else { instr->set_bypass_sreg1(); }
			if ( !scoreboards[instr->get_core_thread_id()]->is_bypass(s2)) { sreg2 = get_regfile(instr->get_core_thread_id())->read(s2); }
			else { instr->set_bypass_sreg2(); }
			if ( !scoreboards[instr->get_core_thread_id()]->is_bypass(s3)) { sreg3 = get_regfile(instr->get_core_thread_id())->read(s3); }
			else { instr->set_bypass_sreg3(); }
			uint32_t s4 = instr->get_sreg4().addr;
			uint32_t s5 = instr->get_sreg5().addr;
			uint32_t s6 = instr->get_sreg6().addr;
			uint32_t s7 = instr->get_sreg7().addr;
			if ( !scoreboards[instr->get_core_thread_id()]->is_bypass(s4)) { sreg4 = get_regfile(instr->get_core_thread_id())->read(s4); }
			else { instr->set_bypass_sreg4(); }
			if ( !scoreboards[instr->get_core_thread_id()]->is_bypass(s5)) { sreg5 = get_regfile(instr->get_core_thread_id())->read(s5); }
			else { instr->set_bypass_sreg5(); }
			if ( !scoreboards[instr->get_core_thread_id()]->is_bypass(s6)) { sreg6 = get_regfile(instr->get_core_thread_id())->read(s6); }
			else { instr->set_bypass_sreg6(); }
			if ( scoreboards[instr->get_core_thread_id()]->is_bypass(s7)) { sreg7 = get_regfile(instr->get_core_thread_id())->read(s7); }
			else { instr->set_bypass_sreg7(); }
		
			uint32_t d0 = instr->get_dreg().addr;
			uint32_t d1 = instr->get_dreg1().addr;
			uint32_t d2 = instr->get_dreg2().addr;
			uint32_t d3 = instr->get_dreg3().addr;
// TODO: We may want to lock these going forward....
				scoreboards[instr->get_core_thread_id()]->get_reg(d0, instr_type);
				scoreboards[instr->get_core_thread_id()]->get_reg(d1, instr_type);
				scoreboards[instr->get_core_thread_id()]->get_reg(d2, instr_type);
				scoreboards[instr->get_core_thread_id()]->get_reg(d3, instr_type);
				instr->set_sb_get();
		
			goto done_regread;
		} 
		case I_VFMUL:
		{
			uint32_t s0 = instr->get_sreg0().addr;
			uint32_t s1 = instr->get_sreg1().addr;
			uint32_t s2 = instr->get_sreg2().addr;
			uint32_t s3 = instr->get_sreg3().addr;
			if ( !scoreboards[instr->get_core_thread_id()]->is_bypass(s0)) { sreg0 = get_regfile(instr->get_core_thread_id())->read(s0); }
			else { instr->set_bypass_sreg0(); }
			if ( !scoreboards[instr->get_core_thread_id()]->is_bypass(s1)) { sreg1 = get_regfile(instr->get_core_thread_id())->read(s1); }
			else { instr->set_bypass_sreg1(); }
			if ( !scoreboards[instr->get_core_thread_id()]->is_bypass(s2)) { sreg2 = get_regfile(instr->get_core_thread_id())->read(s2); }
			else { instr->set_bypass_sreg2(); }
			if ( !scoreboards[instr->get_core_thread_id()]->is_bypass(s3)) { sreg3 = get_regfile(instr->get_core_thread_id())->read(s3); }
			else { instr->set_bypass_sreg3(); }
			uint32_t s4 = instr->get_sreg4().addr;
			uint32_t s5 = instr->get_sreg5().addr;
			uint32_t s6 = instr->get_sreg6().addr;
			uint32_t s7 = instr->get_sreg7().addr;
			if ( !scoreboards[instr->get_core_thread_id()]->is_bypass(s4)) { sreg4 = get_regfile(instr->get_core_thread_id())->read(s4); }
			else { instr->set_bypass_sreg4(); }
			if ( !scoreboards[instr->get_core_thread_id()]->is_bypass(s5)) { sreg5 = get_regfile(instr->get_core_thread_id())->read(s5); }
			else { instr->set_bypass_sreg5(); }
			if ( !scoreboards[instr->get_core_thread_id()]->is_bypass(s6)) { sreg6 = get_regfile(instr->get_core_thread_id())->read(s6); }
			else { instr->set_bypass_sreg6(); }
			if ( scoreboards[instr->get_core_thread_id()]->is_bypass(s7)) { sreg7 = get_regfile(instr->get_core_thread_id())->read(s7); }
			else { instr->set_bypass_sreg7(); }
		
			uint32_t d0 = instr->get_dreg().addr;
			uint32_t d1 = instr->get_dreg1().addr;
			uint32_t d2 = instr->get_dreg2().addr;
			uint32_t d3 = instr->get_dreg3().addr;
// TODO: We may want to lock these going forward....
				scoreboards[instr->get_core_thread_id()]->get_reg(d0, instr_type);
				scoreboards[instr->get_core_thread_id()]->get_reg(d1, instr_type);
				scoreboards[instr->get_core_thread_id()]->get_reg(d2, instr_type);
				scoreboards[instr->get_core_thread_id()]->get_reg(d3, instr_type);
				instr->set_sb_get();
		
			goto done_regread;
		} 
		case I_VLDW:
		{
			uint32_t s4 = instr->get_sreg4().addr;
			if ( !scoreboards[instr->get_core_thread_id()]->is_bypass(s4)) { sreg4 = get_regfile(instr->get_core_thread_id())->read(s4); }
			else { instr->set_bypass_sreg4(); }
		
			uint32_t d0 = instr->get_dreg().addr;
			uint32_t d1 = instr->get_dreg1().addr;
			uint32_t d2 = instr->get_dreg2().addr;
			uint32_t d3 = instr->get_dreg3().addr;
				scoreboards[instr->get_core_thread_id()]->lockReg(d0);
				scoreboards[instr->get_core_thread_id()]->lockReg(d1);
				scoreboards[instr->get_core_thread_id()]->lockReg(d2);
				scoreboards[instr->get_core_thread_id()]->lockReg(d3);
				scoreboards[instr->get_core_thread_id()]->get_reg(d0, instr_type);
				scoreboards[instr->get_core_thread_id()]->get_reg(d1, instr_type);
				scoreboards[instr->get_core_thread_id()]->get_reg(d2, instr_type);
				scoreboards[instr->get_core_thread_id()]->get_reg(d3, instr_type);
				instr->set_sb_get();
		
			goto done_regread;
		} 
		case I_VSTW:
		{
			uint32_t s0 = instr->get_sreg0().addr;
			uint32_t s1 = instr->get_sreg1().addr;
			uint32_t s2 = instr->get_sreg2().addr;
			uint32_t s3 = instr->get_sreg3().addr;
			if (!scoreboards[instr->get_core_thread_id()]->is_bypass(s0)) { sreg0 = get_regfile(instr->get_core_thread_id())->read(s0); }
			else { instr->set_bypass_sreg0(); }
			if ( !scoreboards[instr->get_core_thread_id()]->is_bypass(s1)) { sreg1 = get_regfile(instr->get_core_thread_id())->read(s1); }
			else { instr->set_bypass_sreg1(); }
			if ( !scoreboards[instr->get_core_thread_id()]->is_bypass(s2)) { sreg2 = get_regfile(instr->get_core_thread_id())->read(s2); }
			else { instr->set_bypass_sreg2(); }
			if ( !scoreboards[instr->get_core_thread_id()]->is_bypass(s3)) { sreg3 = get_regfile(instr->get_core_thread_id())->read(s3); }
			else { instr->set_bypass_sreg3(); }
			uint32_t s4 = instr->get_sreg4().addr;
			if (!scoreboards[instr->get_core_thread_id()]->is_bypass(s4)) { sreg4 = get_regfile(instr->get_core_thread_id())->read(s4); }
			else { instr->set_bypass_sreg4(); }
		
			goto done_regread;
		} 
		case I_NULL:
		case I_PRE_DECODE:
		case I_REP_HLT:
		case I_DONE:
		case I_BRANCH_FALL_THROUGH:
		case I_CMNE_NOP:
		case I_CMEQ_NOP:
		{
			 /* NO UPDATES FOR SPECIAL CASES */ 
			break;
		}

		default: 
		{
			goto bad_itype;
		}
	} /* END OF UPDATE SWITCH */
