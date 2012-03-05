
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
		case I_ATOMCAS:
		case I_ATOMADDU:
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
			 /* DEST REG_d */ 
			 RegisterBase result_reg = instr->get_dreg();
			if (scoreboards[instr->get_core_thread_id()]->isLocked(result_reg.addr)) {
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
			 /* DEST REG_d */ 
			 RegisterBase result_reg = instr->get_dreg();
			if (scoreboards[instr->get_core_thread_id()]->isLocked(result_reg.addr)) {
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
			 /* DEST REG_d */ 
			 RegisterBase result_reg = instr->get_dreg();
			if (scoreboards[instr->get_core_thread_id()]->isLocked(result_reg.addr)) {
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
			 /* DEST REG_d */ 
			 RegisterBase result_reg = instr->get_dreg();
			if (scoreboards[instr->get_core_thread_id()]->isLocked(result_reg.addr)) {
				    goto ExecuteStalledReturn;
			}

 			break;
		}
		case I_MVUI:
		case I_STRTOF:
		case I_EVENT:
		case I_PRIO:
		{
			 /* DEST REG_d */ 
			 RegisterBase result_reg = instr->get_dreg();
			if (scoreboards[instr->get_core_thread_id()]->isLocked(result_reg.addr)) {
				    goto ExecuteStalledReturn;
			}

 			break;
		}
		case I_SLLI:
		case I_SRLI:
		case I_SRAI:
		case I_SLEEP:
		{
			 /* DEST REG_d */ 
			 RegisterBase result_reg = instr->get_dreg();
			if (scoreboards[instr->get_core_thread_id()]->isLocked(result_reg.addr)) {
				    goto ExecuteStalledReturn;
			}

 			break;
		}
		case I_FA2R:
		case I_ATOMDEC:
		case I_ATOMINC:
		case I_ATOMXCHG:
		{
			 /* DEST REG_d */ 
			 RegisterBase result_reg = instr->get_dreg();
			if (scoreboards[instr->get_core_thread_id()]->isLocked(result_reg.addr)) {
				    goto ExecuteStalledReturn;
			}

 			break;
		}
		case I_MFSR:
		{
			 /* DEST REG_d (SOURCE IS SP REG) */ 
			 RegisterBase result_reg = instr->get_dreg();
			if (scoreboards[instr->get_core_thread_id()]->isLocked(result_reg.addr)) {
				    goto ExecuteStalledReturn;
			}

 			break;
		}
		case I_MTSR:
		{
			 /* DEST REG_d (SP REG) */ 
			 RegisterBase result_reg = instr->get_dreg();
			if (sp_sbs[instr->get_core_thread_id()]->isLocked(result_reg.addr)) {
				    goto ExecuteStalledReturn;
			}

 			break;
		}
		case I_LJL:
		{
			 /* DEST LINK_REG */ 
			 uint32_t result_reg = rigel::regs::LINK_REG;
			if (scoreboards[instr->get_core_thread_id()]->isLocked(result_reg)) {
				    goto ExecuteStalledReturn;
			}

 			break;
		}
		case I_JAL:
		{
			 /* DEST LINK_REG */ 
			 uint32_t result_reg = rigel::regs::LINK_REG;
			if (scoreboards[instr->get_core_thread_id()]->isLocked(result_reg)) {
				    goto ExecuteStalledReturn;
			}

 			break;
		}
		case I_JALR:
		{
			 /* DEST LINK_REG */ 
			 uint32_t result_reg = rigel::regs::LINK_REG;
			if (scoreboards[instr->get_core_thread_id()]->isLocked(result_reg)) {
				    goto ExecuteStalledReturn;
			}

 			break;
		}
		case I_STC:
		{
			 /* Store-conditional uses r1 to report success  */ 
			if (scoreboards[instr->get_core_thread_id()]->isLocked(instr->get_dreg().addr)) {
				    goto ExecuteStalledReturn;
			}

 			break;
		}
		case I_TQ_ENQUEUE:
		{
			 /* DEST LINK_REG */ 
			using namespace rigel::task_queue; 
			if (	scoreboards[instr->get_core_thread_id()]->isLocked(TQ_RET_REG)) { 
				    goto ExecuteStalledReturn;
			}

 			break;
		}
		case I_TQ_DEQUEUE:
		{
			 /* DEST LINK_REG */ 
			using namespace rigel::task_queue; 
			if (	scoreboards[instr->get_core_thread_id()]->isLocked(TQ_RET_REG) || 
						scoreboards[instr->get_core_thread_id()]->isLocked(TQ_REG0) || 
						scoreboards[instr->get_core_thread_id()]->isLocked(TQ_REG1) || 
						scoreboards[instr->get_core_thread_id()]->isLocked(TQ_REG2) || 
						scoreboards[instr->get_core_thread_id()]->isLocked(TQ_REG3)) {
				    goto ExecuteStalledReturn;
			}

 			break;
		}
		case I_TQ_LOOP:
		{
			 /* DEST LINK_REG */ 
			using namespace rigel::task_queue; 
			if (	scoreboards[instr->get_core_thread_id()]->isLocked(TQ_RET_REG)) { 
				    goto ExecuteStalledReturn;
			}

 			break;
		}
		case I_TQ_INIT:
		{
			 /* DEST LINK_REG */ 
			using namespace rigel::task_queue; 
			if (	scoreboards[instr->get_core_thread_id()]->isLocked(TQ_RET_REG)) { 
				    goto ExecuteStalledReturn;
			}

 			break;
		}
		case I_TQ_END:
		{
			 /* DEST LINK_REG */ 
			using namespace rigel::task_queue; 
			if (	scoreboards[instr->get_core_thread_id()]->isLocked(TQ_RET_REG)) { 
				    goto ExecuteStalledReturn;
			}

 			break;
		}
		case I_FMADD:
		{
			 /* DEST is Accumulator  */ 
			RegisterBase result_reg = instr->get_dacc();
			/*if (ac_sbs[instr->get_core_thread_id()]->isLocked(result_reg.addr,false)) { // Uncomment this instead if using a separate acc_file*/
			if (scoreboards[instr->get_core_thread_id()]->isLocked(result_reg.addr)) {
				    goto ExecuteStalledReturn;
			}

 			break;
		}
		case I_FMSUB:
		{
			 /* DEST is Accumulator  */ 
			RegisterBase result_reg = instr->get_dacc();
			/*if (ac_sbs[instr->get_core_thread_id()]->isLocked(result_reg.addr,false)) { // Uncomment this instead if using a separate acc_file*/
			if (scoreboards[instr->get_core_thread_id()]->isLocked(result_reg.addr)) {
				    goto ExecuteStalledReturn;
			}

 			break;
		}
		case I_FR2A:
		{
			 /* DEST is Accumulator  */ 
			RegisterBase result_reg = instr->get_dacc();
			/*if (ac_sbs[instr->get_core_thread_id()]->isLocked(result_reg.addr,false)) { // Uncomment this instead if using a separate acc_file*/
			if (scoreboards[instr->get_core_thread_id()]->isLocked(result_reg.addr)) {
				    goto ExecuteStalledReturn;
			}

 			break;
		}
		case I_VADD:
		{
			 /* DEST LINK_REG */ 
			 RegisterBase reg_d = instr->get_dreg();
			assert(reg_d.addr < 8
				 && "Invalid vector register");
			uint32_t d0 = (reg_d.addr * 4) + 0;
			uint32_t d1 = (reg_d.addr * 4) + 1;
			uint32_t d2 = (reg_d.addr * 4) + 2;
			uint32_t d3 = (reg_d.addr * 4) + 3;
			instr->set_regs_vec_d(d0, d1, d2, d3);
			if (	scoreboards[instr->get_core_thread_id()]->isLocked(d0) || 
						scoreboards[instr->get_core_thread_id()]->isLocked(d1) || 
						scoreboards[instr->get_core_thread_id()]->isLocked(d2) || 
						scoreboards[instr->get_core_thread_id()]->isLocked(d3)) {
				    goto ExecuteStalledReturn;
			}

 			break;
		}
		case I_VSUB:
		{
			 /* DEST LINK_REG */ 
			 RegisterBase reg_d = instr->get_dreg();
			assert(reg_d.addr < 8
				 && "Invalid vector register");
			uint32_t d0 = (reg_d.addr * 4) + 0;
			uint32_t d1 = (reg_d.addr * 4) + 1;
			uint32_t d2 = (reg_d.addr * 4) + 2;
			uint32_t d3 = (reg_d.addr * 4) + 3;
			instr->set_regs_vec_d(d0, d1, d2, d3);
			if (	scoreboards[instr->get_core_thread_id()]->isLocked(d0) || 
						scoreboards[instr->get_core_thread_id()]->isLocked(d1) || 
						scoreboards[instr->get_core_thread_id()]->isLocked(d2) || 
						scoreboards[instr->get_core_thread_id()]->isLocked(d3)) {
				    goto ExecuteStalledReturn;
			}

 			break;
		}
		case I_VADDI:
		{
			 /* DEST LINK_REG */ 
			 RegisterBase reg_d = instr->get_dreg();
			assert(reg_d.addr < 8
				 && "Invalid vector register");
			uint32_t d0 = (reg_d.addr * 4) + 0;
			uint32_t d1 = (reg_d.addr * 4) + 1;
			uint32_t d2 = (reg_d.addr * 4) + 2;
			uint32_t d3 = (reg_d.addr * 4) + 3;
			instr->set_regs_vec_d(d0, d1, d2, d3);
			if (	scoreboards[instr->get_core_thread_id()]->isLocked(d0) || 
						scoreboards[instr->get_core_thread_id()]->isLocked(d1) || 
						scoreboards[instr->get_core_thread_id()]->isLocked(d2) || 
						scoreboards[instr->get_core_thread_id()]->isLocked(d3)) {
				    goto ExecuteStalledReturn;
			}

 			break;
		}
		case I_VSUBI:
		{
			 /* DEST LINK_REG */ 
			 RegisterBase reg_d = instr->get_dreg();
			assert(reg_d.addr < 8
				 && "Invalid vector register");
			uint32_t d0 = (reg_d.addr * 4) + 0;
			uint32_t d1 = (reg_d.addr * 4) + 1;
			uint32_t d2 = (reg_d.addr * 4) + 2;
			uint32_t d3 = (reg_d.addr * 4) + 3;
			instr->set_regs_vec_d(d0, d1, d2, d3);
			if (	scoreboards[instr->get_core_thread_id()]->isLocked(d0) || 
						scoreboards[instr->get_core_thread_id()]->isLocked(d1) || 
						scoreboards[instr->get_core_thread_id()]->isLocked(d2) || 
						scoreboards[instr->get_core_thread_id()]->isLocked(d3)) {
				    goto ExecuteStalledReturn;
			}

 			break;
		}
		case I_VFADD:
		{
			 /* DEST LINK_REG */ 
			 RegisterBase reg_d = instr->get_dreg();
			assert(reg_d.addr < 8
				 && "Invalid vector register");
			uint32_t d0 = (reg_d.addr * 4) + 0;
			uint32_t d1 = (reg_d.addr * 4) + 1;
			uint32_t d2 = (reg_d.addr * 4) + 2;
			uint32_t d3 = (reg_d.addr * 4) + 3;
			instr->set_regs_vec_d(d0, d1, d2, d3);
			if (	scoreboards[instr->get_core_thread_id()]->isLocked(d0) || 
						scoreboards[instr->get_core_thread_id()]->isLocked(d1) || 
						scoreboards[instr->get_core_thread_id()]->isLocked(d2) || 
						scoreboards[instr->get_core_thread_id()]->isLocked(d3)) {
				    goto ExecuteStalledReturn;
			}

 			break;
		}
		case I_VFSUB:
		{
			 /* DEST LINK_REG */ 
			 RegisterBase reg_d = instr->get_dreg();
			assert(reg_d.addr < 8
				 && "Invalid vector register");
			uint32_t d0 = (reg_d.addr * 4) + 0;
			uint32_t d1 = (reg_d.addr * 4) + 1;
			uint32_t d2 = (reg_d.addr * 4) + 2;
			uint32_t d3 = (reg_d.addr * 4) + 3;
			instr->set_regs_vec_d(d0, d1, d2, d3);
			if (	scoreboards[instr->get_core_thread_id()]->isLocked(d0) || 
						scoreboards[instr->get_core_thread_id()]->isLocked(d1) || 
						scoreboards[instr->get_core_thread_id()]->isLocked(d2) || 
						scoreboards[instr->get_core_thread_id()]->isLocked(d3)) {
				    goto ExecuteStalledReturn;
			}

 			break;
		}
		case I_VFMUL:
		{
			 /* DEST LINK_REG */ 
			 RegisterBase reg_d = instr->get_dreg();
			assert(reg_d.addr < 8
				 && "Invalid vector register");
			uint32_t d0 = (reg_d.addr * 4) + 0;
			uint32_t d1 = (reg_d.addr * 4) + 1;
			uint32_t d2 = (reg_d.addr * 4) + 2;
			uint32_t d3 = (reg_d.addr * 4) + 3;
			instr->set_regs_vec_d(d0, d1, d2, d3);
			if (	scoreboards[instr->get_core_thread_id()]->isLocked(d0) || 
						scoreboards[instr->get_core_thread_id()]->isLocked(d1) || 
						scoreboards[instr->get_core_thread_id()]->isLocked(d2) || 
						scoreboards[instr->get_core_thread_id()]->isLocked(d3)) {
				    goto ExecuteStalledReturn;
			}

 			break;
		}
		case I_VLDW:
		{
			 /* DEST LINK_REG */ 
			 RegisterBase reg_d = instr->get_dreg();
			assert(reg_d.addr < 8
				 && "Invalid vector register");
			uint32_t d0 = (reg_d.addr * 4) + 0;
			uint32_t d1 = (reg_d.addr * 4) + 1;
			uint32_t d2 = (reg_d.addr * 4) + 2;
			uint32_t d3 = (reg_d.addr * 4) + 3;
			instr->set_regs_vec_d(d0, d1, d2, d3);
			if (	scoreboards[instr->get_core_thread_id()]->isLocked(d0) || 
						scoreboards[instr->get_core_thread_id()]->isLocked(d1) || 
						scoreboards[instr->get_core_thread_id()]->isLocked(d2) || 
						scoreboards[instr->get_core_thread_id()]->isLocked(d3)) {
				    goto ExecuteStalledReturn;
			}

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
			 /* NO DEST REGS */ 
			break;
		}

		default: 
		{
			/*NADA*/
		}
	} /* END OF WRITE SWITCH */
