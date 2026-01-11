#include <stdlib.h>
#include <stdio.h>

#include "dylib.h"
#include "diagnostics.h"


static uint32_t* functionJumpTableEntryTemplate() {
	uint32_t* entry = (uint32_t*) malloc(sizeof(uint32_t) * 7);
	if (!entry) emitError(ERR_MEM, "Failed to allocate memory for jump table entry template");

	// A jump table entry is:
	/**
	 * ```
	 * ld x10, =funcAddr % save the function address into x10
	 * ubr x10 % jump to the function address
	 * % note that the called function is to save LR (contains the address after the instruction that called the jump table)
	 * % it should also restore LR before returning (so ret can use that LR)
	 * ```
	 */
	// There would need to be a relocation entry for the decomposed address

	// ld x10, =funcAddr --decomposes->
	entry[0] = 0x840003CA; // mv x10, #0x0
	entry[1] = 0x4800494A; // lsl x10, x10, #18
	entry[2] = 0x840003CB; // mv x11, #0x0
	entry[3] = 0x4800116B; // lsl x11, x11, #4
	entry[4] = 0x4100296A; // or x10, x10, x11
	entry[5] = 0x8000014A; // add x10, x10, #0x0

	// ubr x10
	entry[6] = 0xC200000A;

	return entry;
}


static uint32_t createFJTEntrySymbol(SymbolTable* symbTable, uint32_t symbAddr, const char* symbName) {
	initScope("createFJTEntrySymbol");

	// Format of the symbol name is: [fxnName].fjt
	char fullSymbName[128];
	snprintf(fullSymbName, sizeof(fullSymbName), "%s.fjt", symbName);

	AOEFFSymEnt newSymbEntry;
	newSymbEntry.seSymbName = appendString(symbTable, fullSymbName);
	newSymbEntry.seSymbInfo = SE_SET_INFO(SE_FUNC_T, SE_GLOBL);
	newSymbEntry.seSymbSect = 3;
	newSymbEntry.seSymbVal = symbAddr;
	newSymbEntry.seSymbSize = 7 * 4; // 7 instructions, 4 bytes each

	if (symbTable->count == symbTable->cap) {
		uint32_t newCap = symbTable->cap * 2;
		AOEFFSymEnt* newSymbols = (AOEFFSymEnt*) realloc(symbTable->symbols, sizeof(AOEFFSymEnt) * newCap);
		if (!newSymbols) emitError(ERR_MEM, "Failed to reallocate memory for symbol table");

		int* newFilectxIndices = (int*) realloc(symbTable->filectxIndices, sizeof(int) * newCap);
		if (!newFilectxIndices) emitError(ERR_MEM, "Failed to reallocate memory for symbol table file context indices");

		symbTable->symbols = newSymbols;
		symbTable->filectxIndices = newFilectxIndices;
		symbTable->cap = newCap;
	}

	symbTable->symbols[symbTable->count] = newSymbEntry;
	symbTable->filectxIndices[symbTable->count] = -1; // Jump table symbols do not belong to any file context

	symbTable->count++;

	log("Created FJT entry symbol: %s at address 0x%X", symbName, symbAddr);

	return symbTable->count - 1;
}

/**
 * Fixes the instruction in which had a relocation (for a library function call) to call the jump table entry instead.
 * It uses `instrAddr` and `targetAddr` to calculate the offset to use in the instruction. These addresses are to be 
 *   in reference of the memory layout. For example 0x20200000 instead of 0x100.
 * This is basically the RE_ARU32_IR24 relocation.
 * 
 * @param instrAddr The address of the instruction to fix
 * @param targetAddr The address of the FJT entry to jump to
 * @param instrOff The offset of the instruction in the global text section
 * @param text The text section data
 */
static void fixInstrCall(uint32_t instrAddr, uint32_t targetAddr, uint32_t instrOff, uint32_t* text) {
	initScope("fixInstrCall");

	detail("Fixing instruction call at address 0x%X to target address 0x%X", instrAddr, targetAddr);
	uint32_t offset = targetAddr - instrAddr;
	detail("Calculated offset for instruction fix: 0x%X", offset);
	int32_t encodedOffset = offset >> 2;
	detail("Encoded offset for instruction fix: 0x%X", encodedOffset);

	uint32_t* instr = (uint32_t*)((uint8_t*)text + instrOff);
	*instr |= (encodedOffset & 0xFFFFFF);
	log("Fixed instruction at offset 0x%X to be 0x%X", instrOff, *instr);
}



JumpTables* createJumpTables(RelocTable* relocTable, SymbolTable* symbTable, SectionTable* sectTable) {
	initScope("createJumpTables");

	log("Creating jump tables for unresolved symbols...");

	JumpTables* jumpTables = (JumpTables*) malloc(sizeof(JumpTables));
	if (!jumpTables) emitError(ERR_MEM, "Failed to allocate memory for jump tables");

	jumpTables->fjt._fjt = NULL;
	jumpTables->fjt.fjtEntryCount = 0;

	jumpTables->djt._djt = NULL;
	jumpTables->djt.djtEntryCount = 0;
	
	// For each unresolved relocation, create a jump table entry
	// Add the entry to the appropriate jump table
	// Use the static relocation data to patch the function call to use the jump table entry instead
	// 		The LP of the function call (to use for calculating the offset) is the offset in the relocation entry
	// "Convert" the static relocation to a dynamic relocation of type RE_ARU32_DECOMP, save it to relocTable->drelocs
	// Attach a symbol named fxnName.jft for functions, fxnName.djt for data
	// Attach the FJT at the end of the text section data
	// Attach the DJT at the end of the data section data


	// The relocation string table can be cleared out, as the new dynamic relocations will use new names
	// No need to free and allocate, just use same space but reset counts
	relocTable->RelocStringTable.strbCount = 0;
	relocTable->RelocStringTable.strCount = 0;
	relocTable->trelocs.count = 0; // Clear static relocations as they are no longer needed
	

	uint32_t startAddrFJT = sectTable->sections[SECT_TEXT].shSectSize + sectTable->sectionOffsets[SECT_TEXT];
	uint32_t startAddrDJT = sectTable->sections[SECT_DATA].shSectSize + sectTable->sectionOffsets[SECT_DATA];
	log("Function Jump Table start address: 0x%X", startAddrFJT);
	log("Data Jump Table start address: 0x%X", startAddrDJT);

	for (int i = 0; i < relocTable->unresolved.count; i++) {
		int* indexPair = relocTable->unresolved.unresolvedIndices[i];
		int relocTableIndex = indexPair[0];
		int relocEntryIndex = indexPair[1];

		AOEFFTRelTab* relTab = &relocTable->trelocs.tables[relocTableIndex];
		AOEFFTRelEnt* relEntry = &relTab->relEntries[relocEntryIndex];

		AOEFFSymEnt* symbEntry = &symbTable->symbols[relEntry->reSymb];
		const char* symbName = &symbTable->SymbolStringTable.strTab.stStrs[symbEntry->seSymbName];

		uint8_t symbType = SE_GET_TYPE(symbEntry->seSymbInfo);

		if (symbType == SE_FUNC_T) {
			// Function jump table
			log("Creating function jump table entry for symbol: %s", symbName);

			uint32_t* fjtEntry = functionJumpTableEntryTemplate();

			jumpTables->fjt._fjt = (uint32_t*) realloc(jumpTables->fjt._fjt, sizeof(uint32_t) * (jumpTables->fjt.fjtEntryCount + 7));
			if (!jumpTables->fjt._fjt) emitError(ERR_MEM, "Failed to reallocate memory for function jump table");

			uint32_t localFjtEntryOffset = jumpTables->fjt.fjtEntryCount * 4; // The offset of this entry in the FJT (not the entire text)

			for (int j = 0; j < 7; j++) {
				jumpTables->fjt._fjt[jumpTables->fjt.fjtEntryCount + j] = fjtEntry[j];
			}
			jumpTables->fjt.fjtEntryCount += 7;

			free(fjtEntry);

			// The absolute address of this entry in memory: start addr of text + text size + local offset
			uint32_t absoluteFjtEntryAddr = startAddrFJT + localFjtEntryOffset;
			detail("Function jump table entry absolute address: 0x%X", absoluteFjtEntryAddr);

			// Three things to do left:
			// Create a symbol entry for the fjt
			// Create (and append) a dynamic relocation entry for the fjt
			// Patch the original instruction to jump to the fjt entry instead

			uint32_t fjtEntrySymbIndex = createFJTEntrySymbol(symbTable, absoluteFjtEntryAddr, symbName);

			// Create and append dynamic relocation entry for FJT
			AOEFFDRelEnt* drelocEntry = initDRelocEntry(absoluteFjtEntryAddr, relEntry->reSymb, RE_ARU32_DECOMP, relEntry->reAddend);
			AOEFFDRelTab* drelocTable = initDRelocTable(3, appendRelocString(relocTable, ".drel.fjt"));
			appendDRelocEntry(drelocTable, drelocEntry);
			appendDRelocTable(relocTable, drelocTable);

			log("Created dynamic relocation entry for FJT: off=0x%X, symb=%d, type=%d, addend=0x%X",
					drelocEntry->reOff,
					drelocEntry->reSymb,
					drelocEntry->reType,
					drelocEntry->reAddend);

			// Patch original instruction to jump to FJT entry
			uint32_t instrOff = relEntry->reOff; // The instruction offset from the text section that needed relocation, will jump to the FJT entry instead
			uint32_t instrAddr = sectTable->sectionOffsets[SECT_TEXT] + instrOff;
			detail("Patching instruction at offset 0x%X to jump to FJT entry at address 0x%X", instrOff, absoluteFjtEntryAddr);
			fixInstrCall(instrAddr, absoluteFjtEntryAddr, instrOff, sectTable->_text);

		} else {
			// Data jump table
			log("Creating data jump table entry for symbol: %s", symbName);

		}
	}

	return jumpTables;
}