#include <stdlib.h>


#include "relocate.h"
#include "diagnostics.h"





static void applyRelocation(void* sectionData, AOEFFTRelEnt* relEntry, AOEFFSymEnt* symbEntry, uint32_t toRelOffset, uint32_t baseOffset) {
	rdetail("First instruction in text section before relocation: 0x%X", ((uint32_t*)sectionData)[0]);
	rdetail("%p", sectionData);

	// TODO: in the case that the relocation type is DECOMP and the symbol is absolute, it means ld reg, =ABSVAL
	// Refer to the comment in the assembler in `handleLDImmMove`
	// In such case, apply the relocation but it needs to be dropped from the table since it is an absolute value, not an address
	// This can be done when it can be figured out how to drop a relocation entry from the table safely
	//  since it is looping based off on the count and cannot be updated in-place, but writeBinary uses the count directly
	// Maybe have a list containing the indices of entries dropped
	// After relocation, go to each index, shift all entries after it up to the next index back by one
	// Repeat for all indices, the new count is original count - number of dropped entries

	// Later note: Since static relocations are not kept in the final binary, this is not necessary for now
	// They will be needed for shared libraries but as dynamic relocations

	uint32_t* location = (uint32_t*) ((uint8_t*) sectionData + toRelOffset);
	uint32_t symbValue = symbEntry->seSymbVal;
	uint32_t originalData = *location;

	rlog("Applying relocation at offset 0x%X: original data=0x%X, symbol value=0x%X, type=%d, addend=0x%X, baseOffset=0x%X",
			toRelOffset,
			originalData,
			symbValue,
			relEntry->reType,
			relEntry->reAddend,
			baseOffset);

	uint16_t newUnsignedData = 0;
	int16_t newSigned16Data = 0;
	int32_t newSigned32Data = 0;

	switch (relEntry->reType) {
		case RE_ARU32_ABS14: // unsigned 14-bit relocation in bits 10-23
			*location &= ~(0x3FFF << 10); // Clear bits 10-23

			newUnsignedData = (symbValue + relEntry->reAddend) & 0x3FFF;
			rdetail("Computed new ABS14 data: 0x%X", newUnsignedData);
			*location |= (newUnsignedData << 10);
			break;
		case RE_ARU32_MEM9: // signed 9-bit relocation in bits 15-23
			*location &= ~(0x1FF << 15); // Clear bits 15-23
			
			newSigned16Data = (int16_t)(symbValue + relEntry->reAddend);
			rdetail("Computed new MEM9 data: 0x%X", newSigned16Data);
			*location |= ((newSigned16Data & 0x1FF) << 15);
			break;
		case RE_ARU32_IR24: // signed 24-bit relocation in bits 0-23
			// This is a branch instruction
			// The value is (target - lp) << 2
			// Lp is stored as the offset of this relocation
			*location &= ~(0xFFFFFF); // Clear bits 0-23

			newSigned32Data = (int32_t)(((symbValue + relEntry->reAddend) - (baseOffset + toRelOffset)) >> 2);
			rdetail("Computed new IR24 data: 0x%X (symbValue=0x%X, addend=0x%X, reOff=0x%X, baseOffset=0x%X)", 
					newSigned32Data, symbValue, relEntry->reAddend, toRelOffset, baseOffset);

			*location |= (newSigned32Data & 0xFFFFFF);
			break;
		case RE_ARU32_IR19: // signed 19-bit relocation in bits 5-23
			// Same case as IR24
			*location &= ~(0x7FFFF << 5); // Clear bits 5-23

			newSigned32Data = (int32_t)(((symbValue + relEntry->reAddend) - (baseOffset + toRelOffset)) >> 2);
			rdetail("Computed new IR19 data: 0x%X", newSigned32Data);

			*location |= ((newSigned32Data & 0x7FFFF) << 5);
			break;
		case RE_ARU32_DECOMP:
			// A relocation of type DECOMP means that there are multiple relocations to be done
			// The data is an address, split into three: high 14, mid 14, low 4
			// The first instruction (*location) is to hold the high 14 bits, as if it was ABS14 rel type
			// The second instruction (*(location + 2)) is to hold the mid 14 bits, as if it was ABS14 rel type
			// The third instruction (*(location + 5)) is to hold the low 4 bits, as if it was ABS14 rel type

			uint32_t fullNumber = symbValue + relEntry->reAddend;

			// Recall that the value might either be an absolute number or an address
			// In the case of an absolute number, baseOffset is not applied since it does not refer to an address

			if (SE_GET_TYPE(symbEntry->seSymbInfo) != SE_ABSV_T) {
				fullNumber += baseOffset;
				rdetail("Relocated symbol is not absolute, full address to decompose: 0x%X", fullNumber);
			} else rdetail("Relocated symbol is absolute, full number to decompose: 0x%X", fullNumber);


			uint16_t high14 = (fullNumber >> 18) & 0x3FFF;
			uint16_t mid14 = (fullNumber >> 4) & 0x3FFF;
			uint8_t low4 = fullNumber & 0xF;

			rdetail("Decomposed full address 0x%X into high14=0x%X, mid14=0x%X, low4=0x%X", fullNumber, high14, mid14, low4);
			// Apply high 14 bits
			*location &= ~(0x3FFF << 10); // Clear bits 10-23
			*location |= (high14 << 10);

			// Apply mid 14 bits
			uint32_t* midLocation = location + 1;
			*midLocation &= ~(0x3FFF << 10); // Clear bits 10-23
			*midLocation |= (mid14 << 10);

			// Apply low 4 bits
			uint32_t* lowLocation = location + 2;
			*lowLocation &= ~(0xF << 10); // Clear bits 10-13
			*lowLocation |= (low4 << 10);
			break;
		case RE_ARU32_ABS8: // unsigned 8-bit relocation in bits 0-7
			*location &= ~(0xFF); // Clear bits 0-7

			newUnsignedData = (symbValue + relEntry->reAddend) & 0xFF;
			rdetail("Computed new ABS8 data: 0x%X", newUnsignedData);
			*location |= newUnsignedData;
			break;
		case RE_ARU32_ABS16: // unsigned 16-bit relocation in bits 0-15
			*location &= ~(0xFFFF); // Clear bits 0-15

			newUnsignedData = (symbValue + relEntry->reAddend) & 0xFFFF;
			rdetail("Computed new ABS16 data: 0x%X", newUnsignedData);
			*location |= newUnsignedData;
			break;
		case RE_ARU32_ABS32: // full 32-bit relocation
			*location = symbValue + relEntry->reAddend;

			rdetail("Computed new ABS32 data: 0x%X", *location);
			break;
		default:
			emitError(ERR_INTERNAL, "Unsupported relocation type %d", relEntry->reType);
			break;
	}

	rlog("Relocated data at offset 0x%X to 0x%X", relEntry->reOff, *location);
}


void relocate(RelocTable* relocTable, SectionTable* sectTable, SymbolTable* symbTable) {
	initScope("relocate");

	log("Applying relocations...\n");

	for (int i = 0; i < relocTable->trelocs.count; i++) {
		AOEFFTRelTab* table = &relocTable->trelocs.tables[i];

		char* relTabName = &relocTable->RelocStringTable.strTab.rstStrs[table->relTabName];

		rtrace("---------------- Table %d -----------------", i);
		rtrace("Section: %d | Name: %s | Entry Count: %d || FileCtx: %d |", table->relSect, relTabName, table->relCount, relocTable->trelocs.filectxIndices[i]);
		rtrace("------------------------------------------");
		rtrace(" Num |  Offset  | Symbol | Type | Addend |");
		rtrace("------------------------------------------");

		char* typeStr = NULL;
		for (uint32_t j = 0; j < table->relCount; j++) {
			AOEFFTRelEnt* entry = &table->relEntries[j];

			switch (entry->reType) {
				case RE_ARU32_ABS14:
					typeStr = "ABS14";
					break;
				case RE_ARU32_MEM9:
					typeStr = "MEM9";
					break;
				case RE_ARU32_IR24:
					typeStr = "IR24";
					break;
				case RE_ARU32_IR19:
					typeStr = "IR19";
					break;
				case RE_ARU32_DECOMP:
					typeStr = "DECOMP";
					break;
				case RE_ARU32_ABS8:
					typeStr = "ABS8";
					break;
				case RE_ARU32_ABS16:
					typeStr = "ABS16";
					break;
				case RE_ARU32_ABS32:
					typeStr = "ABS32";
					break;
				default:
					typeStr = "UNKNOWN";
					break;
			}

			rtrace("%4d | 0x%06X |  %5d | %4s | 0x%04X |", j, entry->reOff, entry->reSymb, typeStr, entry->reAddend);

			void* sectionData = NULL;
			if (table->relSect == 0) sectionData = sectTable->_data;
			else if (table->relSect == 1) sectionData = sectTable->_const;
			else if (table->relSect == 3) sectionData = sectTable->_text;
			else if (table->relSect == 4) sectionData = sectTable->_evt;
			else emitError(ERR_INTERNAL, "Unsupported relocation section %d", table->relSect);

			// Apply relocation

			// Note that relOff holds the local offset form the start of the section
			// However, the sectionData is already the global section data
			// This primarily applies for branching where it is used for the LP calculation
			// Update it so that it is the global offset
			// This is done using the symbol's file context to find where the section starts globally
			// If, locally, the symbol is at offset 0x100 in .text, and that file's .text starts at 0x2000 globally
			// Then the global offset is 0x2000 + 0x100 = 0x2100

			AOEFFSymEnt* symbEntry = &symbTable->symbols[entry->reSymb];

			// The location to relocate is needed, indicated by reOff
			// However, as mentioned, it needs to be updated to global offset
			uint32_t toRelOffset = entry->reOff;
			// Update to global offset, dependent on where the section to relocate starts at globally
			FileCtx* relFilectx = &sectTable->filectxs.ctx[relocTable->trelocs.filectxIndices[i]];
			if (table->relSect == 0) { // .data
				toRelOffset += relFilectx->dataOffset;
				rdetail("Relocation's file context data offset: 0x%X", relFilectx->dataOffset);
			} else if (table->relSect == 1) { // .const
				toRelOffset += relFilectx->constOffset;
				rdetail("Relocation's file context const offset: 0x%X", relFilectx->constOffset);
			} else if (table->relSect == 3) { // .text
				toRelOffset += relFilectx->textOffset;
				rdetail("Relocation's file context text offset: 0x%X", relFilectx->textOffset);
			} else if (table->relSect == 4) { // .evt
				rdetail("Relocation's file context evt fixed offset: 0x%X", 0x0);
			}
			rdetail("Computed relocation global offset: 0x%X", toRelOffset);
			// Update it again for it to be in accordance of the section starting point in the final binary
			// toRelOffset += sectTable->sectionOffsets[table->relSect];
			// rdetail("Adjusted relocation global offset with final section offset: 0x%X", toRelOffset);

			// In the case that the symbol is undefined, it cannot be statically relocated
			// This means that the symbol comes from a dynamic library and is to be resolved at load time
			// Skip such relocations
			// This is here instead of earlier so that the relOff is updated to use the global in order for it to be converted to dynamic relocation later
			//   and allow for use in the FJT/DJT
			if (SE_GET_TYPE(symbEntry->seSymbInfo) == SE_NONE_T && symbEntry->seSymbSect == SE_SECT_UNDEF) {
				rlog("Skipping relocation entry %d in relocation table %d for undefined symbol %d", j, i, entry->reSymb);
				entry->reOff = toRelOffset;
				rlog("Updated unresolved relocation entry %d's reOff to global offset 0x%X", j, entry->reOff);
				addUnresolved(relocTable, i, j);
				continue;
			}

			int symbFilectxIndex = symbTable->filectxIndices[entry->reSymb];
			rdetail("Symbol %d belongs to file context index %d", entry->reSymb, symbFilectxIndex);
			FileCtx* symbolFilectx = &sectTable->filectxs.ctx[symbFilectxIndex];

			uint32_t sectOffset = 0; // The offset where the defined symbol's section starts globally
			if (table->relSect == 0) { // .data
				sectOffset = symbolFilectx->dataOffset;
				rdetail("Symbol's file context section offset: dataOffset=0x%X", symbolFilectx->dataOffset);
				rdetail("Resolved to 0x%X in data", sectOffset);
			} else if (table->relSect == 1) { // .const
				sectOffset = symbolFilectx->constOffset;
				rdetail("Symbol's file context section offset: constOffset=0x%X", symbolFilectx->constOffset);
				rdetail("Resolved to 0x%X in const", sectOffset);
			} else if (table->relSect == 3) { // .text
				sectOffset = symbolFilectx->textOffset;
				rdetail("Symbol's file context section offset: textOffset=0x%X", symbolFilectx->textOffset);
				rdetail("Resolved to 0x%X in text", sectOffset);
			} else if (table->relSect == 4) { // .evt
				sectOffset = 0x0; // Evt section starts at fixed address once
				rdetail("Symbol's file context section offset: evt fixed offset=0x%X", sectOffset);
			}
			rdetail("Symbol's file context section offset: 0x%X", sectOffset);

			applyRelocation(sectionData, entry, symbEntry, toRelOffset, sectTable->sectionOffsets[symbEntry->seSymbSect]);

			// The relocation will remain for the loader to use
			// However, relOff is to be updated to global offset
			// This is because the loader will need to know where to apply the relocation in the final binary
			entry->reOff = toRelOffset;
			rlog("Updated relocation entry %d's reOff to global offset 0x%X", j, entry->reOff);
		}
		rtrace("------------------------------------------\n");
	}
}