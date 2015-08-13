#pragma once
#include "lightspeed\base\types.h"
#include "lightspeed\base\containers\autoArray.h"
#include "wordid.h"
#include "docid.h"

namespace SourceIndex {

	using namespace LightSpeed;

	struct WordMatch {
		Bin::natural16 classId, page, line, col;

		WordMatch() {}
		WordMatch(Bin::natural16 classId, Bin::natural16 page, Bin::natural16 line, Bin::natural16 col)
			:classId(classId), page(page), line(line), col(col) {}
		WordMatch(Bin::natural16 classId, Bin::natural32 line, Bin::natural16 col)
			:classId(classId), page(0x8000 | (line >> 16)), line((Bin::natural16)line & 0xFFFF), col(col) {}


		natural getClassID() const { return classId; }
		natural getPage() const { return page >= 0x8000 ? 0 : page; }
		natural getLine() const {
			return (natural)line + ((natural)(page >= 0x8000 ? (page - 0x8000) : 0) << 16);
		}
		natural getCol() const { return col; }
	};

	struct WordIncrement {
		WordID wordID;
		WordMatch match;
	};

	typedef AutoArray<WordIncrement> DocIncrement;



}
